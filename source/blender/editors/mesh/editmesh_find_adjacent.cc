/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 *
 * Puerto nativo en C++ de `scripts/startup/bl_operators/bmesh/find_adjacent.py` y de los
 * dos operadores de `bl_operators/mesh.py` que tiraban de el (Flipendo carril C):
 *
 *   mesh.select_next_item -> MESH_OT_select_next_item
 *   mesh.select_prev_item -> MESH_OT_select_prev_item
 *
 * Detecta el "siguiente" elemento (vertice, arista o cara) a partir de la pareja de
 * elementos que el usuario selecciono por ultimo, midiendo la profundidad topologica de
 * los vertices del destino respecto al origen por dos caminos distintos (saltando de
 * arista a arista por vertices compartidos, y saltando por caras) y buscando otro
 * elemento con exactamente el mismo perfil de profundidades.
 *
 * Sobre los conjuntos: el original usa `set` de Python, cuyo orden de iteracion depende
 * del hash de los punteros. Aqui se lleva un `Vector` en paralelo al `Set` para iterar
 * siempre en orden de insercion. El resultado no cambia —lo que sale de esas iteraciones
 * son conjuntos, y los empates acaban descartados por ambiguos—, pero asi el volcado se
 * reproduce a si mismo, que es lo que exige el reglamento de una linea base.
 */

#include <algorithm>

#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_report.hh"

#include "bmesh.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_mesh.hh"
#include "ED_screen.hh"

#include "mesh_intern.hh" /* own include */

using namespace blender;

namespace {

using YieldEdgeFn = FunctionRef<void(BMEdge *)>;
using OtherEdgesFn = void (*)(BMEdge *, YieldEdgeFn);

/* `other_edges_over_face()`: las dos aristas vecinas dentro de cada cara que comparte
 * esta arista. Puede repetir la misma arista, y da igual: el llamador deduplica. */
void other_edges_over_face(BMEdge *e, YieldEdgeFn yield)
{
  BMIter iter;
  BMLoop *l;
  BM_ITER_ELEM (l, &iter, e, BM_LOOPS_OF_EDGE) {
    yield(l->next->e);
    yield(l->prev->e);
  }
}

/* `other_edges_over_edge()`: las demas aristas que tocan los dos vertices de esta.
 *
 * TRAMPA: el `if not e.is_wire` del original mira la arista de PARTIDA, no la vecina, y
 * ademas esta dentro del bucle interior. Se conserva tal cual. */
void other_edges_over_edge(BMEdge *e, YieldEdgeFn yield)
{
  BMVert *verts[2] = {e->v1, e->v2};
  for (BMVert *v : verts) {
    BMIter iter;
    BMEdge *e_other;
    BM_ITER_ELEM (e_other, &iter, v, BM_EDGES_OF_VERT) {
      if (e_other != e) {
        if (!BM_edge_is_wire(e)) {
          yield(e_other);
        }
      }
    }
  }
}

Vector<BMVert *> verts_from_elem(BMElem *ele)
{
  Vector<BMVert *> result;
  switch (ele->head.htype) {
    case BM_FACE: {
      BMIter iter;
      BMLoop *l;
      BM_ITER_ELEM (l, &iter, reinterpret_cast<BMFace *>(ele), BM_LOOPS_OF_FACE) {
        result.append(l->v);
      }
      break;
    }
    case BM_EDGE: {
      BMEdge *e = reinterpret_cast<BMEdge *>(ele);
      result.append(e->v1);
      result.append(e->v2);
      break;
    }
    default:
      result.append(reinterpret_cast<BMVert *>(ele));
      break;
  }
  return result;
}

Vector<BMEdge *> edges_from_elem(BMElem *ele)
{
  Vector<BMEdge *> result;
  switch (ele->head.htype) {
    case BM_FACE: {
      BMIter iter;
      BMLoop *l;
      BM_ITER_ELEM (l, &iter, reinterpret_cast<BMFace *>(ele), BM_LOOPS_OF_FACE) {
        result.append(l->e);
      }
      break;
    }
    case BM_EDGE:
      result.append(reinterpret_cast<BMEdge *>(ele));
      break;
    default: {
      BMIter iter;
      BMEdge *e;
      BM_ITER_ELEM (e, &iter, reinterpret_cast<BMVert *>(ele), BM_EDGES_OF_VERT) {
        result.append(e);
      }
      break;
    }
  }
  return result;
}

/* Recorrido en anchura desde `ele_init` guardando, para cada vertice alcanzado, la
 * primera profundidad a la que aparece. Devuelve tambien el orden de insercion. */
struct VertDepths {
  Map<BMVert *, int> depths;
  Vector<BMVert *> order;

  void set_default(BMVert *v, const int depth)
  {
    if (depths.add(v, depth)) {
      order.append(v);
    }
  }
};

void walk_depths(BMElem *ele_init,
                 const OtherEdgesFn cb,
                 const int depth_max,
                 FunctionRef<bool(const VertDepths &)> should_stop,
                 VertDepths *r_vert_depths)
{
  Vector<BMEdge *> stack_old = edges_from_elem(ele_init);
  Vector<BMEdge *> stack_new;
  Set<BMEdge *> stack_visit;
  for (BMEdge *e : stack_old) {
    stack_visit.add(e);
  }

  int depth = 0;
  while (!stack_old.is_empty()) {
    if (depth_max >= 0 && depth > depth_max) {
      break;
    }
    if (should_stop && should_stop(*r_vert_depths)) {
      break;
    }
    for (BMEdge *e : stack_old) {
      for (BMVert *v : verts_from_elem(reinterpret_cast<BMElem *>(e))) {
        r_vert_depths->set_default(v, depth);
      }
      cb(e, [&](BMEdge *e_other) {
        if (stack_visit.add(e_other)) {
          stack_new.append(e_other);
        }
      });
    }
    std::swap(stack_old, stack_new);
    stack_new.clear();
    depth++;
  }
}

/* `elems_depth_search()`. */
Vector<BMElem *> elems_depth_search(BMElem *ele_init,
                                    const Span<int> depths,
                                    const OtherEdgesFn cb,
                                    const Vector<BMElem *> *results_init)
{
  const int depth_max = *std::max_element(depths.begin(), depths.end());
  const int depth_min = *std::min_element(depths.begin(), depths.end());
  Vector<int> depths_sorted(depths);
  std::sort(depths_sorted.begin(), depths_sorted.end());

  VertDepths vert_depths;
  walk_depths(ele_init, cb, depth_max, nullptr, &vert_depths);

  /* Candidatos: los elementos del mismo tipo que `ele_init` colgados de un vertice que se
   * alcanzo a profundidad suficiente. */
  Vector<BMElem *> test_ele;
  Set<BMElem *> test_ele_seen;
  const char htype = ele_init->head.htype;
  for (BMVert *v : vert_depths.order) {
    if (vert_depths.depths.lookup(v) < depth_min) {
      continue;
    }
    if (htype == BM_FACE) {
      BMIter iter;
      BMLoop *l;
      BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
        BMElem *ele = reinterpret_cast<BMElem *>(l->f);
        if (test_ele_seen.add(ele)) {
          test_ele.append(ele);
        }
      }
    }
    else if (htype == BM_EDGE) {
      BMIter iter;
      BMEdge *e;
      BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
        if (BM_edge_is_wire(e)) {
          continue;
        }
        BMElem *ele = reinterpret_cast<BMElem *>(e);
        if (test_ele_seen.add(ele)) {
          test_ele.append(ele);
        }
      }
    }
    else {
      BMElem *ele = reinterpret_cast<BMElem *>(v);
      if (test_ele_seen.add(ele)) {
        test_ele.append(ele);
      }
    }
  }

  Vector<BMElem *> result_ele;
  Set<BMElem *> result_seen;
  Vector<int> depths_test;

  for (BMElem *ele : test_ele) {
    const Vector<BMVert *> verts_test = verts_from_elem(ele);
    if (verts_test.size() != depths.size()) {
      continue;
    }
    if (results_init != nullptr && !results_init->contains(ele)) {
      continue;
    }
    if (result_seen.contains(ele)) {
      continue;
    }

    depths_test.clear();
    bool ok = true;
    for (BMVert *v : verts_test) {
      const int *depth = vert_depths.depths.lookup_ptr(v);
      if (depth == nullptr) {
        ok = false;
        break;
      }
      depths_test.append(*depth);
    }
    if (!ok) {
      continue;
    }
    Vector<int> sorted_test(depths_test);
    std::sort(sorted_test.begin(), sorted_test.end());
    if (sorted_test.as_span() == depths_sorted.as_span()) {
      result_seen.add(ele);
      result_ele.append(ele);
    }
  }

  return result_ele;
}

/* `elems_depth_measure()`: profundidades de los vertices de `ele_dst` vistas desde
 * `ele_src`, alineadas con el orden de vertices de `ele_dst`. Devuelve falso si no se
 * alcanzaron todos. */
bool elems_depth_measure(BMElem *ele_dst,
                         BMElem *ele_src,
                         const OtherEdgesFn cb,
                         Vector<int> *r_depths)
{
  const Vector<BMVert *> ele_dst_verts = verts_from_elem(ele_dst);
  Set<BMVert *> all_dst;
  for (BMVert *v : ele_dst_verts) {
    all_dst.add(v);
  }

  Vector<BMEdge *> stack_old = edges_from_elem(ele_src);
  Vector<BMEdge *> stack_new;
  Set<BMEdge *> stack_visit;
  for (BMEdge *e : stack_old) {
    stack_visit.add(e);
  }

  Map<BMVert *, int> vert_depths;
  int depth = 0;
  while (!stack_old.is_empty() && !all_dst.is_empty()) {
    for (BMEdge *e : stack_old) {
      for (BMVert *v : verts_from_elem(reinterpret_cast<BMElem *>(e))) {
        if (all_dst.remove(v)) {
          vert_depths.add_overwrite(v, depth);
        }
      }
      cb(e, [&](BMEdge *e_other) {
        if (stack_visit.add(e_other)) {
          stack_new.append(e_other);
        }
      });
    }
    std::swap(stack_old, stack_new);
    stack_new.clear();
    depth++;
  }

  if (!all_dst.is_empty()) {
    return false;
  }
  r_depths->clear();
  for (BMVert *v : ele_dst_verts) {
    r_depths->append(vert_depths.lookup(v));
  }
  return true;
}

/* `find_next()`. */
Vector<BMElem *> find_next(BMElem *ele_dst, BMElem *ele_src)
{
  Vector<int> depth_src_a, depth_src_b;
  if (!elems_depth_measure(ele_dst, ele_src, other_edges_over_edge, &depth_src_a) ||
      !elems_depth_measure(ele_dst, ele_src, other_edges_over_face, &depth_src_b))
  {
    return {};
  }

  Vector<BMElem *> candidates = elems_depth_search(
      ele_dst, depth_src_a, other_edges_over_edge, nullptr);
  candidates = elems_depth_search(ele_dst, depth_src_b, other_edges_over_face, &candidates);
  candidates.remove_if([&](BMElem *ele) { return ele == ele_src || ele == ele_dst; });
  if (candidates.is_empty()) {
    return {};
  }

  /* Se elige el candidato con mayor variacion de profundidad respecto al origen: es el
   * que tiene mas probabilidad de caer en el elemento opuesto. Al cuadrado, para que unos
   * pocos valores altos ganen a muchos bajos. */
  int diff_best = 0;
  BMElem *ele_best = nullptr;
  Vector<BMElem *> ele_best_ls;
  for (BMElem *ele_test : candidates) {
    Vector<int> depth_test_a, depth_test_b;
    if (!elems_depth_measure(ele_dst, ele_test, other_edges_over_edge, &depth_test_a) ||
        !elems_depth_measure(ele_dst, ele_test, other_edges_over_face, &depth_test_b))
    {
      continue;
    }
    int diff_test = 0;
    for (const int i : depth_src_a.index_range()) {
      const int da = std::abs(depth_src_a[i] - depth_test_a[i]);
      const int db = std::abs(depth_src_b[i] - depth_test_b[i]);
      diff_test += da * da + db * db;
    }
    if (diff_test > diff_best) {
      diff_best = diff_test;
      ele_best = ele_test;
      ele_best_ls.clear();
      ele_best_ls.append(ele_best);
    }
    else if (diff_test == diff_best) {
      if (ele_best == nullptr) {
        ele_best = ele_test;
      }
      ele_best_ls.append(ele_test);
    }
  }

  if (ele_best_ls.size() > 1) {
    const Vector<BMElem *> ele_best_ls_init = ele_best_ls;
    ele_best_ls.clear();
    int depth_accum_max = -1;
    for (BMElem *ele_test : ele_best_ls_init) {
      Vector<int> depth_test_a, depth_test_b;
      if (!elems_depth_measure(ele_src, ele_test, other_edges_over_edge, &depth_test_a) ||
          !elems_depth_measure(ele_src, ele_test, other_edges_over_face, &depth_test_b))
      {
        continue;
      }
      int depth_accum_test = 0;
      for (const int d : depth_test_a) {
        depth_accum_test += d;
      }
      for (const int d : depth_test_b) {
        depth_accum_test += d;
      }
      if (depth_accum_test > depth_accum_max) {
        depth_accum_max = depth_accum_test;
        ele_best = ele_test;
        ele_best_ls.clear();
        ele_best_ls.append(ele_best);
      }
      else if (depth_accum_test == depth_accum_max) {
        /* Varios igual de buenos: no se devuelve ninguno. */
        ele_best_ls.append(ele_test);
      }
    }
  }

  return ele_best_ls;
}

/* `ele_uuid()`: huella topologica del entorno del elemento, ordenada. */
Vector<int> ele_uuid(BMElem *ele)
{
  Vector<int> ret;
  BMIter iter;
  if (ele->head.htype == BM_FACE) {
    BMFace *f_self = reinterpret_cast<BMFace *>(ele);
    BMLoop *l;
    BM_ITER_ELEM (l, &iter, f_self, BM_LOOPS_OF_FACE) {
      BMIter fiter;
      BMFace *f;
      BM_ITER_ELEM (f, &fiter, l->e, BM_FACES_OF_EDGE) {
        if (f != f_self) {
          ret.append(f->len);
        }
      }
    }
  }
  else if (ele->head.htype == BM_EDGE) {
    BMLoop *l;
    BM_ITER_ELEM (l, &iter, reinterpret_cast<BMEdge *>(ele), BM_LOOPS_OF_EDGE) {
      ret.append(l->f->len);
    }
  }
  else {
    BMLoop *l;
    BM_ITER_ELEM (l, &iter, reinterpret_cast<BMVert *>(ele), BM_LOOPS_OF_VERT) {
      ret.append(l->f->len);
    }
  }
  std::sort(ret.begin(), ret.end());
  return ret;
}

Vector<int> uuid_as_set(const Vector<int> &uuid)
{
  Vector<int> out;
  for (const int v : uuid) {
    if (!out.contains(v)) {
      out.append(v);
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

/* Los cuatro criterios de `ele_uuid_filter()`, del mas exigente al mas laxo. */
enum class UuidMode { Exact, AsSet, SumOfSet, Length };

bool uuid_equal(const Vector<int> &a, const Vector<int> &b, const UuidMode mode)
{
  switch (mode) {
    case UuidMode::Exact:
      return a.as_span() == b.as_span();
    case UuidMode::AsSet:
      return uuid_as_set(a).as_span() == uuid_as_set(b).as_span();
    case UuidMode::SumOfSet: {
      int sa = 0, sb = 0;
      for (const int v : uuid_as_set(a)) {
        sa += v;
      }
      for (const int v : uuid_as_set(b)) {
        sb += v;
      }
      return sa == sb;
    }
    case UuidMode::Length:
      return a.size() == b.size();
  }
  return false;
}

Vector<BMElem *> ele_uuid_filter(BMElem *ele_cmp, const Vector<BMElem *> &ele_pair_next)
{
  const Vector<int> uuid_cmp = ele_uuid(ele_cmp);
  Vector<BMElem *> current = ele_pair_next;
  Vector<Vector<int>> current_uuid;
  for (BMElem *ele : current) {
    current_uuid.append(ele_uuid(ele));
  }

  const UuidMode modes[] = {
      UuidMode::Exact, UuidMode::AsSet, UuidMode::SumOfSet, UuidMode::Length};
  for (const UuidMode mode : modes) {
    Vector<BMElem *> matched;
    Vector<Vector<int>> matched_uuid;
    for (const int i : current.index_range()) {
      if (uuid_equal(uuid_cmp, current_uuid[i], mode)) {
        matched.append(current[i]);
        matched_uuid.append(current_uuid[i]);
      }
    }
    if (matched.size() > 1) {
      /* Aun hay empate: se sigue afinando sobre los que quedan. */
      current = matched;
      current_uuid = matched_uuid;
    }
    else if (matched.size() == 1) {
      return matched;
    }
    /* Si no queda ninguno, se prueba el siguiente criterio sobre la lista anterior. */
  }
  return {};
}

BMElem *select_history_nth_from_end(BMesh *bm, const int n)
{
  BMEditSelection *ese = static_cast<BMEditSelection *>(bm->selected.last);
  for (int i = 0; i < n && ese != nullptr; i++) {
    ese = ese->prev;
  }
  return (ese != nullptr) ? ese->ele : nullptr;
}

/* `select_next()` de find_adjacent.py. */
bool find_adjacent_select_next(BMesh *bm, wmOperator *op)
{
  BMElem *ele_dst = select_history_nth_from_end(bm, 0);
  BMElem *ele_src = select_history_nth_from_end(bm, 1);

  if (ele_src == nullptr) {
    BKE_report(op->reports, RPT_INFO, "Selection pair not found");
    return false;
  }

  Vector<BMElem *> ele_pair_next = find_next(ele_dst, ele_src);

  if (ele_pair_next.size() > 1) {
    ele_pair_next = ele_uuid_filter(ele_dst, ele_pair_next);
  }

  if (ele_pair_next.size() != 1) {
    BKE_report(op->reports, RPT_INFO, "No single next item found");
    return false;
  }

  BMElem *ele = ele_pair_next[0];
  if (BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) {
    BKE_report(op->reports, RPT_INFO, "Next element is hidden");
    return false;
  }

  BM_elem_select_set(bm, ele, false);
  BM_elem_select_set(bm, ele, true);
  BM_select_history_remove(bm, ele);
  BM_select_history_store(bm, ele);
  if (ele->head.htype == BM_FACE) {
    bm->act_face = reinterpret_cast<BMFace *>(ele);
  }
  return true;
}

/* `select_prev()` de find_adjacent.py. */
bool find_adjacent_select_prev(BMesh *bm, wmOperator *op)
{
  BMElem *ele = select_history_nth_from_end(bm, 0);
  if (ele == nullptr) {
    BKE_report(op->reports, RPT_INFO, "Last selected not found");
    return false;
  }

  BM_elem_select_set(bm, ele, false);

  /* `BM_elem_select_set()` no toca el historial, asi que el elemento en posicion 1 sigue
   * siendo el mismo que antes de deseleccionar. */
  BMElem *ele_prev = select_history_nth_from_end(bm, 1);
  if (ele_prev != nullptr && ele_prev->head.htype == BM_FACE) {
    bm->act_face = reinterpret_cast<BMFace *>(ele_prev);
  }
  return true;
}

bool edit_mesh_poll(bContext *C)
{
  /* `context.mode == 'EDIT_MESH'`, literal. */
  return CTX_data_mode_enum(C) == CTX_MODE_EDIT_MESH;
}

wmOperatorStatus select_item_exec(bContext *C, wmOperator *op, const bool next)
{
  Object *obedit = CTX_data_active_object(C);
  Mesh *mesh = static_cast<Mesh *>(obedit->data);
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  if (em == nullptr) {
    return OPERATOR_CANCELLED;
  }
  BMesh *bm = em->bm;

  const bool changed = next ? find_adjacent_select_next(bm, op) :
                              find_adjacent_select_prev(bm, op);
  if (changed) {
    BM_mesh_select_mode_flush(bm);
    const EDBMUpdate_Params params = {
        /*calc_looptris*/ false, /*calc_normals*/ false, /*is_destructive*/ false};
    EDBM_update(mesh, &params);
  }
  return OPERATOR_FINISHED;
}

wmOperatorStatus select_next_item_exec(bContext *C, wmOperator *op)
{
  return select_item_exec(C, op, true);
}

wmOperatorStatus select_prev_item_exec(bContext *C, wmOperator *op)
{
  return select_item_exec(C, op, false);
}

}  // namespace

void MESH_OT_select_next_item(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Next Element";
  ot->idname = "MESH_OT_select_next_item";
  ot->description = "Select the next element (using selection order)";

  /* API callbacks. */
  ot->exec = select_next_item_exec;
  ot->poll = edit_mesh_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void MESH_OT_select_prev_item(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Previous Element";
  ot->idname = "MESH_OT_select_prev_item";
  ot->description = "Select the previous element (using selection order)";

  /* API callbacks. */
  ot->exec = select_prev_item_exec;
  ot->poll = edit_mesh_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
