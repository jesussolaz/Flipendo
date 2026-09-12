/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edflipendo
 *
 * Comparador de capturas. El razonamiento entero —y sobre todo por que hay que
 * medir el SUELO DE RUIDO antes de afirmar una diferencia— esta en
 * `fl_image_compare.cc`.
 *
 * Uso:  Blender -b --factory-startup --fl-compare-png <a.png> <b.png> [umbral]
 */

#ifndef __FL_IMAGE_COMPARE_HH__
#define __FL_IMAGE_COMPARE_HH__

namespace flipendo::image_compare {

/**
 * Compara dos capturas del mismo tamano e informa por `stdout`: pixeles distintos,
 * porcentaje, delta medio y maximo, y la caja donde estan las diferencias.
 *
 * Devuelve `false` —gritando el motivo— si falta un fichero, no se puede leer o los
 * tamanos no coinciden: las tres formas de «no pudo comparar».
 */
bool compare(const char *path_a, const char *path_b, int threshold);

/**
 * Informa por `stdout` de la ESTADISTICA de una imagen: luminancia media, minima y
 * maxima, cuantos pixeles son NaN, negativos, negros o saturados, y el centroide de
 * los pixeles por encima de `bright_quantile` veces el maximo.
 *
 * Es la evidencia de cordura fisica de un modelo de sombreado: que no salgan NaN ni
 * negros, que la energia no suba al subir la rugosidad, y que al girar la luz el
 * centroide del brillo se MUEVA. Lee tambien OpenEXR, que es donde el NaN se puede
 * contar de verdad (en un PNG de 8 bits ya se ha perdido).
 */
bool stats(const char *path, float bright_quantile);

}  // namespace flipendo::image_compare

#endif /* __FL_IMAGE_COMPARE_HH__ */
