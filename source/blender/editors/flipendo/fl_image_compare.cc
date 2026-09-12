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


/* -------------------------------------------------------------------- */
/** \name Estadisticas de una imagen (fase 2 del pelo)
 *
 * Comparar dos capturas dice CUANTO cambio. No dice si lo que salio tiene sentido
 * fisico, y esa es justo la pregunta de un modelo de sombreado nuevo:
 *
 * - **¿Hay NaN o valores negativos?** En un PNG de 8 bits ya no se ve: el NaN se ha
 *   convertido en un numero cualquiera al guardar. Por eso esta funcion lee tambien
 *   imagenes en coma flotante (OpenEXR), donde el NaN y el negativo SI se pueden
 *   contar. Es la unica forma honesta de firmar «no salen NaN».
 * - **¿Se dispara la energia?** La luminancia media de la imagen, con la misma
 *   escena y la misma luz, tiene que BAJAR o quedarse igual cuando sube la
 *   rugosidad: un lobulo normalizado reparte la misma energia en mas angulo. Si
 *   sube, el modelo esta creando luz.
 * - **¿Donde esta el brillo?** El centroide de los pixeles mas luminosos, en
 *   coordenadas de imagen. Al girar la luz alrededor del mechon ese centroide tiene
 *   que MOVERSE; si no se mueve, no hay lobulo, hay una constante.
 *
 * Sale con fallo si el fichero no se puede leer: «no pude medir» nunca puede dar 0.
 * \{ */

bool stats(const char *path, const float bright_quantile)
{
  /* Se pide coma flotante Y bytes: para un EXR llega el float y se pueden contar
   * NaN de verdad; para un PNG llega el byte y el float lo sintetiza imbuf. */
  ImBuf *im = IMB_load_image_from_filepath(path, IB_byte_data | IB_float_data);
  if (im == nullptr) {
    fprintf(stderr, "fl-stats-png: no se pudo leer '%s'.\n", path);
    return false;
  }
  const int64_t pixels = int64_t(im->x) * int64_t(im->y);
  if (pixels == 0) {
    fprintf(stderr, "fl-stats-png: '%s' no tiene pixeles.\n", path);
    IMB_freeImBuf(im);
    return false;
  }

  const float *fp = im->float_buffer.data;
  const uint8_t *bp = im->byte_buffer.data;
  if (fp == nullptr && bp == nullptr) {
    fprintf(stderr, "fl-stats-png: '%s' no trae datos legibles.\n", path);
    IMB_freeImBuf(im);
    return false;
  }
  const bool has_float = (fp != nullptr);

  int64_t nan_count = 0;
  int64_t negative_count = 0;
  int64_t black_count = 0;
  int64_t saturated_count = 0;
  double sum_luma = 0.0;
  float max_luma = 0.0f;
  float min_luma = 1e30f;

  /* Luminancia Rec.709. */
  auto luma_of = [&](const int64_t i, float rgb[3]) {
    if (has_float) {
      rgb[0] = fp[i * 4 + 0];
      rgb[1] = fp[i * 4 + 1];
      rgb[2] = fp[i * 4 + 2];
    }
    else {
      rgb[0] = float(bp[i * 4 + 0]) / 255.0f;
      rgb[1] = float(bp[i * 4 + 1]) / 255.0f;
      rgb[2] = float(bp[i * 4 + 2]) / 255.0f;
    }
    return 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2];
  };

  for (int64_t i = 0; i < pixels; i++) {
    float rgb[3];
    const float luma = luma_of(i, rgb);
    bool is_nan = false;
    for (int c = 0; c < 3; c++) {
      if (std::isnan(rgb[c]) || std::isinf(rgb[c])) {
        is_nan = true;
      }
      else if (rgb[c] < 0.0f) {
        negative_count++;
      }
    }
    if (is_nan) {
      nan_count++;
      continue;
    }
    if (luma <= 0.0f) {
      black_count++;
    }
    if (rgb[0] >= 1.0f && rgb[1] >= 1.0f && rgb[2] >= 1.0f) {
      saturated_count++;
    }
    sum_luma += double(luma);
    max_luma = std::max(max_luma, luma);
    min_luma = std::min(min_luma, luma);
  }
  if (min_luma > 1e29f) {
    min_luma = 0.0f;
  }
  const double mean_luma = sum_luma / double(pixels);

  /* El centroide del BRILLO: solo los pixeles por encima de `bright_quantile` veces
   * el maximo cuentan. Con el umbral alto se sigue el nucleo del lobulo y no la
   * silueta entera del objeto, que no se mueve al girar la luz. */
  const float bright_threshold = max_luma * bright_quantile;
  double weight = 0.0;
  double cx = 0.0;
  double cy = 0.0;
  int64_t bright_count = 0;
  for (int64_t i = 0; i < pixels; i++) {
    float rgb[3];
    const float luma = luma_of(i, rgb);
    if (std::isnan(luma) || luma < bright_threshold || luma <= 0.0f) {
      continue;
    }
    const double x = double(i % int64_t(im->x));
    /* imbuf guarda de abajo a arriba; se informa en coordenadas de imagen normales. */
    const double y = double(int64_t(im->y) - 1 - (i / int64_t(im->x)));
    cx += x * double(luma);
    cy += y * double(luma);
    weight += double(luma);
    bright_count++;
  }
  if (weight > 0.0) {
    cx /= weight;
    cy /= weight;
  }

  printf("FL_IMG_STATS %s\n", path);
  printf("  tamano=%dx%d pixeles=%lld float=%d\n",
         im->x,
         im->y,
         (long long)pixels,
         has_float ? 1 : 0);
  printf("  luminancia media=%.6f min=%.6f max=%.6f\n",
         mean_luma,
         double(min_luma),
         double(max_luma));
  printf("  nan=%lld negativos=%lld negros=%lld (%.2f%%) saturados=%lld (%.2f%%)\n",
         (long long)nan_count,
         (long long)negative_count,
         (long long)black_count,
         100.0 * double(black_count) / double(pixels),
         (long long)saturated_count,
         100.0 * double(saturated_count) / double(pixels));
  printf("  brillo umbral=%.6f (%.2f del maximo) pixeles=%lld centroide=(%.2f,%.2f)\n",
         double(bright_threshold),
         double(bright_quantile),
         (long long)bright_count,
         cx,
         cy);

  IMB_freeImBuf(im);
  return true;
}

/** \} */

}  // namespace flipendo::image_compare
