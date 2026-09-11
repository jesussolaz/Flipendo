/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * Comparador comun de los arneses `--fl-selftest-*` / `--fl-check-*`.
 *
 * Todos siguen el mismo patron: el binario vuelca un fichero de texto con el estado
 * observable despues de invocar unos operadores por su idname, y ese fichero se compara
 * linea a linea con una linea base congelada que se capturo con el binario que todavia
 * tenia el Python. Aqui vive la parte aburrida: comparar, contar por caso y contar en
 * total.
 *
 * Convenio del formato de volcado, que esta funcion da por supuesto:
 *  - una linea que empieza por `case=` abre un caso nuevo;
 *  - una linea que empieza por dos espacios y un digito es un "elemento" (un dato
 *    numerado); son las que se cuentan en las cifras de "N/M identicos";
 *  - el resto son cabeceras.
 *
 * LO QUE SE ARREGLO EL 2026-09-11 (carril ARNES)
 * ----------------------------------------------
 * 1. **Cero lineas comparadas devolvia `true`.** Si la linea base y el volcado estaban
 *    los dos vacios, el bucle no se ejecutaba, `same_lines == total_lines == 0` y esto
 *    daba VERDE sin haber mirado nada. Ahora, comparar cero lineas es un FALLO con su
 *    motivo: no hay verificacion sin algo que verificar.
 *
 * 2. **Tolerancia declarada, no rondeo a ojo.** Hay volcados con un valor que no se
 *    reproduce a si mismo entre ejecuciones del mismo binario (REGLAMENTO, leccion de
 *    las 03:50: las normales de vertice se acumulan en paralelo y en coma flotante).
 *    Se declara junto a la linea base, en `<linea-base>.tolerancia`, y el comparador
 *    **dice en voz alta** cada linea que ha tenido que tolerar y con que desviacion.
 *    Sin ese fichero, la comparacion es exacta byte a byte, como siempre.
 *
 * 3. **Divergencias deliberadas declaradas.** Cuando el C++ corrige a proposito un
 *    fallo del Python original, la linea base se queda como esta —es la prueba de lo
 *    que hacia el Python— y la diferencia se declara en `<linea-base>.divergencias`
 *    con su motivo. Entonces la linea se compara contra el texto DECLARADO del C++:
 *    si vuelve al valor del Python es un FALLO (regresion), y si la divergencia deja
 *    de ocurrir tambien lo es (la declaracion sobra y hay que quitarla). Asi una
 *    correccion consciente no deja un rojo permanente, pero tampoco se convierte en
 *    una excusa para no mirar.
 */

#pragma once

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

namespace flipendo::selftest {

namespace detail {

/** Una divergencia deliberada declarada junto a la linea base. */
struct Divergence {
  int line_no = 0;
  std::string python;
  std::string cpp;
  std::string razon;
  bool honoured = false;
};

inline std::string chomp(const std::string &s)
{
  std::string r = s;
  while (!r.empty() && (r.back() == '\n' || r.back() == '\r')) {
    r.pop_back();
  }
  return r;
}

/** Lee `<baseline>.tolerancia`. Devuelve 0.0 si no hay fichero (comparacion exacta). */
inline double read_tolerance(const char *baseline_path, std::string &r_source)
{
  const std::string path = std::string(baseline_path) + ".tolerancia";
  FILE *fp = fopen(path.c_str(), "r");
  if (fp == nullptr) {
    return 0.0;
  }
  char line[1024];
  double tol = 0.0;
  while (fgets(line, sizeof(line), fp) != nullptr) {
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
      continue;
    }
    double value = 0.0;
    if (sscanf(line, "tolerancia-relativa %lf", &value) == 1) {
      tol = value;
    }
  }
  fclose(fp);
  r_source = path;
  return tol;
}

/** Lee `<baseline>.divergencias`. Formato: `linea N`, `python:<texto>`, `cpp:<texto>`,
 * `razon:<texto>`; `#` y lineas en blanco se ignoran. El valor va detras del primer
 * `:` SIN recortar, para que se conserven los espacios del principio del volcado. */
inline blender::Vector<Divergence> read_divergences(const char *baseline_path,
                                                    std::string &r_source)
{
  blender::Vector<Divergence> out;
  const std::string path = std::string(baseline_path) + ".divergencias";
  FILE *fp = fopen(path.c_str(), "r");
  if (fp == nullptr) {
    return out;
  }
  r_source = path;
  char line[4096];
  while (fgets(line, sizeof(line), fp) != nullptr) {
    const std::string raw = chomp(line);
    if (raw.empty() || raw[0] == '#') {
      continue;
    }
    int n = 0;
    if (sscanf(raw.c_str(), "linea %d", &n) == 1) {
      Divergence d;
      d.line_no = n;
      out.append(d);
      continue;
    }
    if (out.is_empty()) {
      continue;
    }
    const size_t colon = raw.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    const std::string key = raw.substr(0, colon);
    const std::string value = raw.substr(colon + 1);
    if (key == "python") {
      out.last().python = value;
    }
    else if (key == "cpp") {
      out.last().cpp = value;
    }
    else if (key == "razon") {
      out.last().razon = value;
    }
  }
  fclose(fp);
  return out;
}

/** Trocea en palabras separadas por espacios, conservando el orden. */
inline blender::Vector<std::string> split_tokens(const char *s)
{
  blender::Vector<std::string> out;
  const char *p = s;
  while (*p != '\0') {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
      p++;
    }
    const char *start = p;
    while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
      p++;
    }
    if (p > start) {
      out.append(std::string(start, size_t(p - start)));
    }
  }
  return out;
}

inline bool parse_number(const std::string &tok, double &r_value)
{
  char *end = nullptr;
  const double v = strtod(tok.c_str(), &end);
  if (end == tok.c_str() || *end != '\0' || !std::isfinite(v)) {
    return false;
  }
  r_value = v;
  return true;
}

/**
 * Compara dos lineas admitiendo una tolerancia RELATIVA en los numeros.
 *
 * Devuelve true solo si tienen las mismas palabras y cada par o es identico letra a
 * letra o son dos numeros que se diferencian en menos de `tol` en relativo. En
 * `r_worst` queda la peor desviacion relativa vista, para poder ensenarla.
 */
inline bool equal_within(const char *a, const char *b, const double tol, double &r_worst)
{
  const blender::Vector<std::string> ta = split_tokens(a);
  const blender::Vector<std::string> tb = split_tokens(b);
  if (ta.size() != tb.size()) {
    return false;
  }
  r_worst = 0.0;
  for (const int64_t i : ta.index_range()) {
    if (ta[i] == tb[i]) {
      continue;
    }
    double va, vb;
    if (!parse_number(ta[i], va) || !parse_number(tb[i], vb)) {
      return false;
    }
    const double scale = std::fmax(1.0, std::fmax(std::fabs(va), std::fabs(vb)));
    const double rel = std::fabs(va - vb) / scale;
    if (rel > tol) {
      return false;
    }
    r_worst = std::fmax(r_worst, rel);
  }
  return true;
}

}  // namespace detail

/**
 * Compara `actual_path` con `baseline_path` e informa por `stderr` con el prefijo `tag`.
 *
 * Devuelve true solo si:
 *  - se ha comparado al menos una linea (comparar nada NO es un aprobado),
 *  - todas las lineas coinciden, salvo las que entran en la tolerancia declarada,
 *  - y todas las divergencias declaradas se han cumplido exactamente.
 */
inline bool compare_to_baseline(const char *tag, const char *actual_path,
                                const char *baseline_path)
{
  FILE *fb = fopen(baseline_path, "r");
  if (fb == nullptr) {
    fprintf(stderr, "%s: no se pudo leer la linea base '%s'\n", tag, baseline_path);
    fprintf(stderr, "%s: sin linea base no hay comparacion posible: FALLO\n", tag);
    return false;
  }
  FILE *fa = fopen(actual_path, "r");
  if (fa == nullptr) {
    fclose(fb);
    fprintf(stderr, "%s: no se pudo releer el volcado '%s'\n", tag, actual_path);
    fprintf(stderr, "%s: sin volcado no hay comparacion posible: FALLO\n", tag);
    return false;
  }

  std::string tol_source;
  const double tolerance = detail::read_tolerance(baseline_path, tol_source);
  std::string div_source;
  blender::Vector<detail::Divergence> divergences = detail::read_divergences(baseline_path,
                                                                            div_source);
  if (tolerance > 0.0) {
    fprintf(stderr,
            "%s: TOLERANCIA DECLARADA %g (relativa), segun '%s'\n",
            tag,
            tolerance,
            tol_source.c_str());
  }
  if (!divergences.is_empty()) {
    fprintf(stderr,
            "%s: %d DIVERGENCIA(S) DELIBERADA(S) declarada(s) en '%s'\n",
            tag,
            int(divergences.size()),
            div_source.c_str());
  }

  char line_b[4096], line_a[4096];
  int line_no = 0;
  int total_values = 0, same_values = 0;
  int total_lines = 0, same_lines = 0;
  int tolerated_lines = 0;
  int broken_divergences = 0;
  int shown = 0;
  blender::Vector<std::string> case_labels;
  blender::Vector<int> case_total;
  blender::Vector<int> case_same;

  while (true) {
    const char *rb = fgets(line_b, sizeof(line_b), fb);
    const char *ra = fgets(line_a, sizeof(line_a), fa);
    if (rb == nullptr && ra == nullptr) {
      break;
    }
    line_no++;
    const char *sb = (rb != nullptr) ? line_b : "(falta linea en la linea base)\n";
    const char *sa = (ra != nullptr) ? line_a : "(falta linea en el volcado)\n";

    if (strncmp(sb, "case=", 5) == 0) {
      char label[256];
      BLI_strncpy(label, sb, sizeof(label));
      if (char *nl = strchr(label, '\n')) {
        *nl = '\0';
      }
      case_labels.append(label);
      case_total.append(0);
      case_same.append(0);
    }

    total_lines++;
    const bool is_value = (sb[0] == ' ' && sb[1] == ' ' && sb[2] >= '0' && sb[2] <= '9');
    if (is_value) {
      total_values++;
      if (!case_total.is_empty()) {
        case_total.last()++;
      }
    }

    /* Divergencia deliberada declarada para esta linea. */
    detail::Divergence *declared = nullptr;
    for (detail::Divergence &d : divergences) {
      if (d.line_no == line_no) {
        declared = &d;
        break;
      }
    }

    bool accepted = false;
    if (declared != nullptr) {
      const std::string actual = detail::chomp(sa);
      if (actual == declared->cpp) {
        declared->honoured = true;
        accepted = true;
        fprintf(stderr,
                "%s: linea %d DIVERGENCIA DELIBERADA cumplida\n  python: %s\n  cpp:    %s\n"
                "  razon:  %s\n",
                tag,
                line_no,
                declared->python.c_str(),
                declared->cpp.c_str(),
                declared->razon.c_str());
      }
      else if (actual == declared->python) {
        broken_divergences++;
        fprintf(stderr,
                "%s: linea %d REGRESION: ha vuelto al valor del Python que el C++ corregia\n"
                "  esperado (C++): %s\n  obtenido:       %s\n  razon de la correccion: %s\n",
                tag,
                line_no,
                declared->cpp.c_str(),
                actual.c_str(),
                declared->razon.c_str());
      }
      else {
        broken_divergences++;
        fprintf(stderr,
                "%s: linea %d declarada como divergencia, pero no da NI el valor del Python\n"
                "  NI el declarado del C++\n  esperado (C++): %s\n  obtenido:       %s\n",
                tag,
                line_no,
                declared->cpp.c_str(),
                actual.c_str());
      }
    }
    else if (rb != nullptr && ra != nullptr && STREQ(sb, sa)) {
      accepted = true;
    }
    else if (rb != nullptr && ra != nullptr && tolerance > 0.0) {
      double worst = 0.0;
      if (detail::equal_within(sb, sa, tolerance, worst)) {
        accepted = true;
        tolerated_lines++;
        fprintf(stderr,
                "%s: linea %d TOLERADA (desviacion relativa %.3g <= %g declarado)\n"
                "  base: %s  real: %s",
                tag,
                line_no,
                worst,
                tolerance,
                sb,
                sa);
      }
    }

    if (accepted) {
      same_lines++;
      if (is_value) {
        same_values++;
        if (!case_same.is_empty()) {
          case_same.last()++;
        }
      }
    }
    else if (declared == nullptr && shown < 10) {
      fprintf(stderr, "%s: linea %d distinta\n  base: %s  real: %s", tag, line_no, sb, sa);
      shown++;
    }
  }

  fclose(fb);
  fclose(fa);

  for (const int i : case_labels.index_range()) {
    fprintf(stderr,
            "%s: %-58s %d/%d elementos identicos\n",
            tag,
            case_labels[i].c_str(),
            case_same[i],
            case_total[i]);
  }
  fprintf(stderr,
          "%s: TOTAL %d/%d elementos identicos, %d/%d lineas\n",
          tag,
          same_values,
          total_values,
          same_lines,
          total_lines);
  if (tolerated_lines != 0) {
    fprintf(stderr,
            "%s: %d linea(s) dentro de la tolerancia declarada %g, no identicas byte a byte\n",
            tag,
            tolerated_lines,
            tolerance);
  }
  else if (tolerance > 0.0) {
    fprintf(stderr,
            "%s: la tolerancia declarada %g no hizo falta en esta pasada (0 lineas toleradas)\n",
            tag,
            tolerance);
  }

  /* Una declaracion que ya no se cumple sobra, y quedarse callado la convierte en una
   * excepcion perpetua que nadie revisa. */
  for (const detail::Divergence &d : divergences) {
    if (!d.honoured && d.line_no <= total_lines) {
      continue; /* Ya se conto arriba como divergencia rota. */
    }
    if (!d.honoured) {
      broken_divergences++;
      fprintf(stderr,
              "%s: la divergencia declarada para la linea %d no se pudo comprobar: el volcado\n"
              "  no llega a esa linea (%d lineas). Sobra la declaracion o falta volcado.\n",
              tag,
              d.line_no,
              total_lines);
    }
  }
  if (!divergences.is_empty() && broken_divergences == 0) {
    fprintf(stderr,
            "%s: %d divergencia(s) deliberada(s) cumplida(s) exactamente\n",
            tag,
            int(divergences.size()));
  }

  /* Comparar CERO lineas no es un aprobado: es no haber comprobado nada. Esta era la
   * ultima puerta por la que se colaba un verde vacio. */
  if (total_lines == 0) {
    fprintf(stderr,
            "%s: FALLO: no se comparo ni una linea. La linea base '%s' y el volcado '%s'\n"
            "  estan los dos vacios, asi que no se ha verificado nada.\n",
            tag,
            baseline_path,
            actual_path);
    return false;
  }

  return (same_lines == total_lines) && (broken_divergences == 0);
}

}  // namespace flipendo::selftest
