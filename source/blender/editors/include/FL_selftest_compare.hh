/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * Comparador comun de los arneses `--fl-selftest-*` / `--fl-check-*` del carril C.
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
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <string>

#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

namespace flipendo::selftest {

/**
 * Compara `actual_path` con `baseline_path` e informa por `stderr` con el prefijo `tag`.
 * Devuelve true solo si no hay ni una linea distinta.
 */
inline bool compare_to_baseline(const char *tag, const char *actual_path,
                                const char *baseline_path)
{
  FILE *fb = fopen(baseline_path, "r");
  if (fb == nullptr) {
    fprintf(stderr, "%s: no se pudo leer la linea base '%s'\n", tag, baseline_path);
    return false;
  }
  FILE *fa = fopen(actual_path, "r");
  if (fa == nullptr) {
    fclose(fb);
    fprintf(stderr, "%s: no se pudo releer '%s'\n", tag, actual_path);
    return false;
  }

  char line_b[2048], line_a[2048];
  int line_no = 0;
  int total_values = 0, same_values = 0;
  int total_lines = 0, same_lines = 0;
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

    if (rb != nullptr && ra != nullptr && STREQ(sb, sa)) {
      same_lines++;
      if (is_value) {
        same_values++;
        if (!case_same.is_empty()) {
          case_same.last()++;
        }
      }
    }
    else if (shown < 10) {
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

  return (same_lines == total_lines);
}

}  // namespace flipendo::selftest
