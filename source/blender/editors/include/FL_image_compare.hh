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

}  // namespace flipendo::image_compare

#endif /* __FL_IMAGE_COMPARE_HH__ */
