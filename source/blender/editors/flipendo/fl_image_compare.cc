/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edflipendo
 *
 * Comparador de CAPTURAS, en C++.
 *
 * Por que existe
 * --------------
 * Todo lo que el carril PELO tiene que demostrar se ve o no se ve: el pelo llega al
 * juego, el sombreado cambia, la simulacion mueve las guias, el nivel de detalle se
 * nota o no se nota. La unica evidencia que vale para eso es comparar dos capturas
 * del Player y dar cifras. Hasta hoy esa cuenta se hacia a ojo o con herramientas de
 * fuera del arbol; ahora la hace el binario, que es donde la doctrina quiere el
 * codigo (politicas/LENGUAJE-CPP.md).
 *
 * La leccion que trae puesta: EL SUELO DE RUIDO
 * ---------------------------------------------
 * Dos ejecuciones del MISMO binario sobre la MISMA escena **no dan el mismo PNG**:
 * EEVEE acumula muestras en el tiempo y el ruido temporal cambia entre arranques.
 * El carril de VideoTexture lo midio primero y el de la interfaz nativa lo repitio:
 * el «resto de la pantalla» difiere consigo mismo en un 2-3 % de los pixeles.
 *
 * Por eso una diferencia solo significa algo **por encima de ese suelo**, y por eso
 * este comparador esta pensado para usarse TRES veces: A contra A' (el suelo), B
 * contra B' (el suelo del otro binario) y A contra B (lo que se quiere medir). Si la
 * tercera cifra no destaca sobre las dos primeras, no hay diferencia que contar, y
 * hay que decirlo asi.
 *
 * Que informa
 * -----------
 * - pixeles distintos y su porcentaje, con un umbral por canal (`--fl-compare-png
 *   a.png b.png [umbral]`, por defecto 0 = exacto);
 * - delta medio y delta maximo por canal (0..255);
 * - la CAJA donde estan las diferencias, que es el «donde» de la pregunta, y su
 *   centro. Sin la caja, «cambian 40.000 pixeles» no distingue «salio el pelo» de
 *   «cambio el ruido de toda la pantalla».
 *
 * Sale con fallo si falta un argumento, si un fichero no se puede leer o si las dos
 * imagenes no tienen el mismo tamano: son las tres formas de «no pudo comparar» de
 * politicas/ARNES-A-PRUEBA.md, y ninguna puede salir con 0.
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "BLI_utildefines.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "FL_image_compare.hh"

namespace flipendo::image_compare {

bool compare(const char *path_a, const char *path_b, const int threshold)
{
  ImBuf *a = IMB_load_image_from_filepath(path_a, IB_byte_data);
  if (a == nullptr) {
    fprintf(stderr, "fl-compare-png: no se pudo leer '%s'.\n", path_a);
    return false;
  }
  ImBuf *b = IMB_load_image_from_filepath(path_b, IB_byte_data);
  if (b == nullptr) {
    fprintf(stderr, "fl-compare-png: no se pudo leer '%s'.\n", path_b);
    IMB_freeImBuf(a);
    return false;
  }
  if (a->x != b->x || a->y != b->y) {
    fprintf(stderr,
            "fl-compare-png: tamanos distintos, %dx%d contra %dx%d; no hay nada que comparar.\n",
            a->x,
            a->y,
            b->x,
            b->y);
    IMB_freeImBuf(a);
    IMB_freeImBuf(b);
    return false;
  }
  const uint8_t *pa = a->byte_buffer.data;
  const uint8_t *pb = b->byte_buffer.data;
  if (pa == nullptr || pb == nullptr) {
    fprintf(stderr, "fl-compare-png: alguna de las dos imagenes no trae datos de 8 bits.\n");
    IMB_freeImBuf(a);
    IMB_freeImBuf(b);
    return false;
  }

  const int64_t total = int64_t(a->x) * int64_t(a->y);
  int64_t diff_px = 0;
  int64_t sum_delta = 0;
  int max_delta = 0;
  int min_x = a->x, max_x = -1, min_y = a->y, max_y = -1;
  int64_t sum_x = 0, sum_y = 0;

  for (int y = 0; y < a->y; y++) {
    for (int x = 0; x < a->x; x++) {
      const int64_t i = (int64_t(y) * int64_t(a->x) + int64_t(x)) * 4;
      int d = 0;
      for (int c = 0; c < 3; c++) { /* RGB; el alfa de una captura es siempre opaco */
        d = std::max(d, std::abs(int(pa[i + c]) - int(pb[i + c])));
      }
      if (d > threshold) {
        diff_px++;
        sum_delta += d;
        max_delta = std::max(max_delta, d);
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
        sum_x += x;
        sum_y += y;
      }
    }
  }

  printf("FL_COMPARE_PNG %s %s umbral=%d\n", path_a, path_b, threshold);
  printf("  tamano=%dx%d pixeles=%lld\n", a->x, a->y, (long long)total);
  printf("  distintos=%lld (%.3f %%)\n",
         (long long)diff_px,
         total ? 100.0 * double(diff_px) / double(total) : 0.0);
  if (diff_px > 0) {
    printf("  delta medio=%.2f maximo=%d\n", double(sum_delta) / double(diff_px), max_delta);
    /* El origen del ImBuf es abajo-izquierda; se informa en las dos convenciones
     * para que la caja se pueda leer sobre el PNG sin tener que pensarlo. */
    printf("  caja(x,y desde abajo)=(%d,%d)-(%d,%d) ancho=%d alto=%d\n",
           min_x,
           min_y,
           max_x,
           max_y,
           max_x - min_x + 1,
           max_y - min_y + 1);
    printf("  centro=(%.1f,%.1f desde abajo) = (%.1f,%.1f desde arriba)\n",
           double(sum_x) / double(diff_px),
           double(sum_y) / double(diff_px),
           double(sum_x) / double(diff_px),
           double(a->y - 1) - double(sum_y) / double(diff_px));
  }

  IMB_freeImBuf(a);
  IMB_freeImBuf(b);
  return true;
}

}  // namespace flipendo::image_compare
