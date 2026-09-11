/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Lector nativo de la tabla del manual en linea. Ver FL_manual_reference.hpp.
 *
 * Sustituye a `scripts/modules/rna_manual_reference.py` (4.272 lineas, de las
 * que 4.253 eran datos) y al emparejador de
 * `bl_operators/wm.py: WM_OT_doc_view_manual._find_reference`.
 */

#include "FL_manual_reference.hpp"

#include "WM_manual.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

#include "BLI_path_utils.hh"
#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "BKE_appdir.hh"
#include "BKE_blender_version.h"

#include "DNA_userdef_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

namespace flipendo::manual {

/* -------------------------------------------------------------------------- */
/** \name Emparejador glob, con la semantica de `fnmatch` de Python
 *
 * Python traduce el patron a una expresion regular con `fnmatch.translate()`
 * y la compila. Aqui se empareja directamente, que es lo que el propio
 * comentario de `wm.py` decia que habria que hacer («use `fnmatch` from C
 * which is significantly faster»).
 *
 * Detalles que hay que respetar para no cambiar ni un resultado:
 * - `*` casa con cualquier cosa, incluido el vacio y los saltos de linea
 *   (Python compila con `re.DOTALL`).
 * - `?` casa con exactamente un caracter.
 * - `[...]` es un conjunto; `[!...]` lo niega; un `]` justo tras el `[` o el
 *   `!` es literal; si no hay `]` de cierre, el `[` es literal.
 * - La barra invertida NO escapa nada: `fnmatch.translate` la pasa por
 *   `re.escape`, o sea que es un caracter mas.
 * \{ */

/** Empareja un conjunto `[...]`. Devuelve el indice tras el `]`, o -1. */
static int glob_set_end(const char *pattern, int i)
{
  /* `i` apunta al caracter siguiente al `[`. Mismo barrido que
   * `fnmatch.translate`: un `!` inicial y un `]` inmediato no cierran. */
  int j = i;
  if (pattern[j] == '!') {
    j++;
  }
  if (pattern[j] == ']') {
    j++;
  }
  while (pattern[j] != '\0' && pattern[j] != ']') {
    j++;
  }
  if (pattern[j] == '\0') {
    return -1;
  }
  return j;
}

static bool glob_set_match(const char *pattern, int start, int end, char c)
{
  int i = start;
  bool negate = false;
  if (pattern[i] == '!') {
    negate = true;
    i++;
  }
  bool found = false;
  while (i < end) {
    if (i + 2 < end && pattern[i + 1] == '-') {
      /* Rango `a-z`. */
      if ((unsigned char)(c) >= (unsigned char)(pattern[i]) && (unsigned char)(c) <= (unsigned char)(pattern[i + 2])) {
        found = true;
      }
      i += 3;
      continue;
    }
    if (pattern[i] == c) {
      found = true;
    }
    i++;
  }
  return negate ? !found : found;
}

bool glob_match(const char *pattern, const char *str)
{
  /* Emparejado iterativo con retroceso sobre el ultimo `*`. */
  int p = 0, s = 0;
  int star_p = -1, star_s = 0;

  while (str[s] != '\0') {
    const char pc = pattern[p];
    if (pc == '*') {
      star_p = p;
      p++;
      star_s = s;
      continue;
    }
    if (pc == '?') {
      p++;
      s++;
      continue;
    }
    if (pc == '[') {
      const int end = glob_set_end(pattern, p + 1);
      if (end != -1) {
        if (glob_set_match(pattern, p + 1, end, str[s])) {
          p = end + 1;
          s++;
          continue;
        }
      }
      else if (str[s] == '[') {
        /* Sin `]` de cierre: el `[` es literal. */
        p++;
        s++;
        continue;
      }
    }
    else if (pc != '\0' && pc == str[s]) {
      p++;
      s++;
      continue;
    }
    /* No casa: retrocede al ultimo `*` si lo hubo. */
    if (star_p == -1) {
      return false;
    }
    star_s++;
    s = star_s;
    p = star_p + 1;
  }

  /* Se acabo la cadena: el resto del patron solo puede ser `*`. */
  while (pattern[p] == '*') {
    p++;
  }
  return pattern[p] == '\0';
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Lectura de la tabla
 * \{ */

static int literal_prefix_len_calc(const std::string &pattern)
{
  int i = 0;
  while (pattern[i] != '\0' && !ELEM(pattern[i], '*', '?', '[')) {
    i++;
  }
  return i;
}

bool table_read(const char *filepath, Table &r_table)
{
  r_table = Table();
  r_table.source_path = filepath;

  std::ifstream f(filepath);
  if (!f.is_open()) {
    return false;
  }

  std::string line;
  bool header_seen = false;
  int declared_count = -1;
  int line_no = 0;

  while (std::getline(f, line)) {
    line_no++;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty() || line[0] == '#') {
      continue;
    }

    if (!header_seen) {
      int version = 0;
      if (sscanf(line.c_str(), "FLIPENDO-MANUAL-REFERENCE %d", &version) != 1) {
        fprintf(stderr, "manual: %s:%d: falta la cabecera del formato\n", filepath, line_no);
        return false;
      }
      if (version != FORMAT_VERSION) {
        fprintf(stderr,
                "manual: %s: formato version %d, esta build entiende la %d\n",
                filepath,
                version,
                FORMAT_VERSION);
        return false;
      }
      header_seen = true;
      continue;
    }

    if (line.compare(0, 7, "PREFIX ") == 0) {
      r_table.prefix_template = line.substr(7);
      continue;
    }
    if (line.compare(0, 6, "COUNT ") == 0) {
      declared_count = atoi(line.c_str() + 6);
      r_table.entries.reserve(size_t(std::max(declared_count, 0)));
      continue;
    }

    const size_t tab = line.find('\t');
    if (tab == std::string::npos) {
      fprintf(stderr, "manual: %s:%d: linea sin tabulador, ignorada\n", filepath, line_no);
      continue;
    }
    Entry entry;
    entry.pattern = line.substr(0, tab);
    entry.url_suffix = line.substr(tab + 1);
    entry.literal_prefix_len = literal_prefix_len_calc(entry.pattern);
    r_table.entries.push_back(std::move(entry));
  }

  if (declared_count >= 0 && size_t(declared_count) != r_table.entries.size()) {
    fprintf(stderr,
            "manual: %s: COUNT dice %d y hay %zu entradas\n",
            filepath,
            declared_count,
            r_table.entries.size());
    return false;
  }

  r_table.loaded = header_seen;
  return r_table.loaded;
}

const Table &table_ensure()
{
  static Table table;
  static bool tried = false;
  if (tried) {
    return table;
  }
  tried = true;

  const std::optional<std::string> dir = BKE_appdir_folder_id(BLENDER_DATAFILES, "manual");
  if (!dir.has_value()) {
    fprintf(stderr, "manual: no encuentro 'datafiles/manual'; el manual en linea no funcionara\n");
    return table;
  }
  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), dir->c_str(), "rna_manual_reference.txt");
  if (!table_read(filepath, table)) {
    fprintf(stderr, "manual: no pude leer '%s'\n", filepath);
  }
  return table;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Busqueda
 * \{ */

std::optional<std::string> find_url_suffix(const std::string &rna_id_in)
{
  const Table &table = table_ensure();
  if (!table.loaded) {
    return std::nullopt;
  }

  /* «XXX, for some reason all RNA ID's are stored lowercase»: el Python bajaba
   * la ruta a minusculas antes de comparar y aqui se hace igual. */
  std::string rna_id = rna_id_in;
  std::transform(rna_id.begin(), rna_id.end(), rna_id.begin(), [](unsigned char c) {
    return char(std::tolower(c));
  });

  for (const Entry &entry : table.entries) {
    /* TRAMPA, y es de las que cambian resultados: en Python la comprobacion
     * rapida era `re.match(r"^[^?\*\[]+", pattern)` y, **si no casaba**
     * (es decir, si el patron empieza por comodin), la entrada se saltaba con
     * `continue`. O sea que un patron que empiece por `*` nunca casa nada.
     * Se replica: prefijo literal de longitud cero -> entrada descartada.
     * (Hoy ninguna de las 4.253 entradas empieza por comodin, pero la tabla
     * viene autogenerada de upstream y esto no debe cambiar en silencio.) */
    if (entry.literal_prefix_len == 0) {
      continue;
    }
    if (rna_id.compare(0, size_t(entry.literal_prefix_len), entry.pattern, 0, size_t(entry.literal_prefix_len)) != 0)
    {
      continue;
    }
    if (glob_match(entry.pattern.c_str(), rna_id.c_str())) {
      return entry.url_suffix;
    }
  }
  return std::nullopt;
}

/**
 * Los 22 idiomas que tienen manual traducido. Copiado literal de
 * `bpy/utils/__init__.py: _manual_language_codes` (los comentados alli no
 * estan aqui: es la misma lista, no una parecida).
 */
static const std::map<std::string, std::string> &manual_language_codes()
{
  static const std::map<std::string, std::string> codes = {
      {"ar_EG", "ar"},        /* Arabe. */
      {"ca_AD", "ca"},        /* Catalan. */
      {"de_DE", "de"},        /* Aleman. */
      {"el_GR", "el"},        /* Griego. */
      {"es", "es"},           /* Espanol. */
      {"fi_FI", "fi"},        /* Finlandes. */
      {"fr_FR", "fr"},        /* Frances. */
      {"id_ID", "id"},        /* Indonesio. */
      {"it_IT", "it"},        /* Italiano. */
      {"ja_JP", "ja"},        /* Japones. */
      {"ko_KR", "ko"},        /* Coreano. */
      {"nl_NL", "nl"},        /* Neerlandes. */
      {"pt_PT", "pt"},        /* Portugues. */
      {"pt_BR", "pt"},        /* Portugues de Brasil: comparte manual con `pt`. */
      {"ru_RU", "ru"},        /* Ruso. */
      {"sk_SK", "sk"},        /* Eslovaco. */
      {"sr_RS", "sr"},        /* Serbio. */
      {"th_TH", "th"},        /* Tailandes. */
      {"uk_UA", "uk"},        /* Ucraniano. */
      {"vi_VN", "vi"},        /* Vietnamita. */
      {"zh_HANS", "zh-hans"}, /* Chino simplificado. */
      {"zh_HANT", "zh-hant"}, /* Chino tradicional. */
  };
  return codes;
}

std::string language_code(bContext *C)
{
  /* El Python leia `bpy.context.preferences.view.language`, que es el
   * IDENTIFICADOR del enum, no el indice ni el locale activo. Se lee por RNA
   * para que sea exactamente el mismo texto (la lista de idiomas la construye
   * `blt_lang.cc` leyendo `datafiles/locale/languages`). */
  std::string language;
  {
    PointerRNA ptr = RNA_pointer_create_discrete(nullptr, &RNA_PreferencesView, &U);
    PropertyRNA *prop = RNA_struct_find_property(&ptr, "language");
    const char *identifier = nullptr;
    if (prop != nullptr &&
        RNA_property_enum_identifier(C, &ptr, prop, RNA_property_enum_get(&ptr, prop), &identifier) &&
        identifier != nullptr)
    {
      language = identifier;
    }
    else {
      language = "DEFAULT";
    }
  }

  if (language == "DEFAULT") {
    /* `os.getenv("LANG", "").split(".")[0]`. */
    const char *env = getenv("LANG");
    language = (env != nullptr) ? env : "";
    const size_t dot = language.find('.');
    if (dot != std::string::npos) {
      language.resize(dot);
    }
  }

  const std::map<std::string, std::string> &codes = manual_language_codes();
  const auto it = codes.find(language);
  return (it != codes.end()) ? it->second : std::string("en");
}

std::string url_prefix(bContext *C)
{
  const Table &table = table_ensure();
  std::string out = table.prefix_template;
  if (out.empty()) {
    out = "https://docs.blender.org/manual/{lang}/{version}/";
  }

  char version[16];
  SNPRINTF(version, "%d.%d", BLENDER_VERSION / 100, BLENDER_VERSION % 100);

  const auto replace = [&out](const char *token, const std::string &value) {
    const size_t at = out.find(token);
    if (at != std::string::npos) {
      out.replace(at, strlen(token), value);
    }
  };
  replace("{lang}", language_code(C));
  replace("{version}", version);
  return out;
}

std::optional<std::string> url_from_rna_id(bContext *C, const std::string &rna_id)
{
  const std::optional<std::string> suffix = find_url_suffix(rna_id);
  if (!suffix.has_value()) {
    return std::nullopt;
  }
  return url_prefix(C) + suffix.value();
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name El proveedor de URLs, sobre el punto de extension de `WM_manual.hpp`
 *
 * `wm_system_ops.cc` (carril B) tiene su propio proveedor integrado con **cinco**
 * entradas y una reserva que manda todo lo demas a `search.html?q=<ruta>`. Eso
 * no es el comportamiento del Python: el Python tenia **4.253** entradas exactas
 * y llevaba al parrafo concreto del manual, con su ancla. Aqui se registra la
 * tabla de datos como proveedor, y como los proveedores se consultan en orden
 * inverso al registro, este se consulta ANTES que el integrado: si la ruta esta
 * en la tabla se devuelve la URL exacta, y si no se devuelve `false` para que la
 * reserva del buscador siga funcionando. Ninguno de los dos se estorba.
 * \{ */

static bool data_table_provider(const blender::StringRef rna_id, std::string &r_url)
{
  const std::optional<std::string> suffix = find_url_suffix(std::string(rna_id));
  if (!suffix.has_value()) {
    return false;
  }
  /* `url_prefix()` no necesita contexto: el `itemf` del enum de idioma lo ignora
   * (`rna_lang_enum_properties_itemf` marca su `bContext *` como no usado). */
  r_url = url_prefix(nullptr) + suffix.value();
  return true;
}

void provider_register_builtin_table()
{
  provider_register(data_table_provider);
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Conversion desde el Python heredado
 * \{ */

bool convert_py_table(const char *py_path, const char *out_path)
{
  std::ifstream f(py_path);
  if (!f.is_open()) {
    fprintf(stderr, "manual: no puedo abrir '%s'\n", py_path);
    return false;
  }

  std::vector<std::pair<std::string, std::string>> rows;
  std::string line;
  int line_no = 0;
  int rejected = 0;
  bool in_table = false;

  while (std::getline(f, line)) {
    line_no++;
    if (!in_table) {
      if (line.compare(0, 20, "url_manual_mapping =") == 0) {
        in_table = true;
      }
      continue;
    }
    if (line.compare(0, 1, ")") == 0) {
      break;
    }
    /* Forma exacta del generador: `    ("PATRON", "URL"),` */
    const size_t q0 = line.find('"');
    if (q0 == std::string::npos) {
      continue;
    }
    const size_t q1 = line.find('"', q0 + 1);
    const size_t q2 = (q1 == std::string::npos) ? q1 : line.find('"', q1 + 1);
    const size_t q3 = (q2 == std::string::npos) ? q2 : line.find('"', q2 + 1);
    if (q3 == std::string::npos) {
      fprintf(stderr, "manual: %s:%d: no son dos cadenas, se rechaza\n", py_path, line_no);
      rejected++;
      continue;
    }
    std::string pattern = line.substr(q0 + 1, q1 - q0 - 1);
    std::string url = line.substr(q2 + 1, q3 - q2 - 1);
    if (pattern.find('\t') != std::string::npos || url.find('\t') != std::string::npos) {
      fprintf(stderr, "manual: %s:%d: tabulador dentro de un campo\n", py_path, line_no);
      rejected++;
      continue;
    }
    rows.emplace_back(std::move(pattern), std::move(url));
  }

  if (rows.empty() || rejected != 0) {
    fprintf(stderr, "manual: conversion abortada (%zu filas, %d rechazos)\n", rows.size(), rejected);
    return false;
  }

  std::ofstream out(out_path, std::ios::binary);
  if (!out.is_open()) {
    fprintf(stderr, "manual: no puedo escribir '%s'\n", out_path);
    return false;
  }
  out << "# Tabla del manual en linea de Flipendo: ruta RNA -> URL.\n";
  out << "# Generada desde scripts/modules/rna_manual_reference.py con\n";
  out << "# --fl-convert-manual-reference. No editar a mano.\n";
  out << "# El ORDEN importa: gana la primera entrada que casa, y la tabla viene\n";
  out << "# ordenada por longitud de patron descendente para que gane la mas concreta.\n";
  out << "FLIPENDO-MANUAL-REFERENCE " << FORMAT_VERSION << "\n";
  out << "PREFIX https://docs.blender.org/manual/{lang}/{version}/\n";
  out << "COUNT " << rows.size() << "\n";
  for (const auto &row : rows) {
    out << row.first << "\t" << row.second << "\n";
  }
  out.close();

  printf("manual: %zu entradas escritas en '%s'\n", rows.size(), out_path);
  return true;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Volcado y comparacion
 * \{ */

bool dump_urls(bContext *C, const char *ids_path, const char *out_path)
{
  std::ifstream in(ids_path);
  if (!in.is_open()) {
    fprintf(stderr, "manual: no puedo abrir la lista '%s'\n", ids_path);
    return false;
  }
  std::ofstream out(out_path, std::ios::binary);
  if (!out.is_open()) {
    fprintf(stderr, "manual: no puedo escribir '%s'\n", out_path);
    return false;
  }

  const Table &table = table_ensure();
  out << "# --fl-dump-manual\n";
  out << "PREFIX\t" << url_prefix(C) << "\n";
  out << "ENTRIES\t" << table.entries.size() << "\n";

  std::string rna_id;
  int n = 0, hits = 0;
  while (std::getline(in, rna_id)) {
    if (!rna_id.empty() && rna_id.back() == '\r') {
      rna_id.pop_back();
    }
    if (rna_id.empty() || rna_id[0] == '#') {
      continue;
    }
    n++;
    /* Se vuelca el SUFIJO, no la URL entera: el prefijo lleva dentro la version
     * de Blender y el idioma, y una linea base que los repita en 17.000 filas se
     * pone roja el dia que suba la version sin que haya ninguna regresion. El
     * prefijo se compara una sola vez, arriba. */
    const std::optional<std::string> suffix = find_url_suffix(rna_id);
    if (suffix.has_value()) {
      hits++;
      out << rna_id << "\t" << suffix.value() << "\n";
    }
    else {
      out << rna_id << "\t-\n";
    }
  }
  out.close();
  printf("manual: %d rutas, %d con URL, volcadas en '%s'\n", n, hits, out_path);
  return true;
}

bool check_urls(bContext *C, const char *baseline_path)
{
  std::ifstream in(baseline_path);
  if (!in.is_open()) {
    fprintf(stderr, "manual: no puedo abrir la linea base '%s'\n", baseline_path);
    return false;
  }

  const std::string prefix = url_prefix(C);
  int n = 0, same = 0, diff = 0;
  int prefix_checked = 0;
  /* El camino de verdad del operador: `flipendo::manual::url_lookup()`, que
   * recorre los proveedores registrados. Se comprueba aparte porque ahi es donde
   * puede fallar el REGISTRO y el ORDEN, no la tabla. */
  int lookup_same = 0, lookup_diff = 0, lookup_fallback = 0;
  std::string line;

  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const size_t tab = line.find('\t');
    if (tab == std::string::npos) {
      continue;
    }
    const std::string key = line.substr(0, tab);
    const std::string value = line.substr(tab + 1);

    if (key == "PREFIX") {
      prefix_checked = 1;
      if (value != prefix) {
        printf("manual: PREFIX distinto\n  python: %s\n  c++   : %s\n", value.c_str(), prefix.c_str());
        diff++;
      }
      else {
        same++;
      }
      continue;
    }
    if (key == "ENTRIES") {
      continue;
    }

    n++;
    const std::optional<std::string> suffix = find_url_suffix(key);
    const std::string got = suffix.has_value() ? suffix.value() : std::string("-");

    /* Por el camino del operador. */
    std::string lookup_url;
    const bool lookup_ok = url_lookup(key, lookup_url);
    if (value == "-") {
      /* El Python no daba URL y avisaba; el proveedor de reserva de
       * `wm_system_ops.cc` manda al buscador del manual. Es una mejora
       * deliberada, no una diferencia: se cuenta aparte. */
      lookup_fallback++;
    }
    else if (lookup_ok && lookup_url == prefix + value) {
      lookup_same++;
    }
    else {
      if (lookup_diff < 10) {
        printf("manual: LOOKUP DISTINTO %s\n  python: %s\n  c++   : %s\n",
               key.c_str(),
               (prefix + value).c_str(),
               lookup_ok ? lookup_url.c_str() : "(sin proveedor)");
      }
      lookup_diff++;
    }

    if (got == value) {
      same++;
    }
    else {
      if (diff < 40) {
        printf("manual: DISTINTO %s\n  python: %s\n  c++   : %s\n", key.c_str(), value.c_str(), got.c_str());
      }
      diff++;
    }
  }

  printf("manual: %d rutas comparadas, %d identicas, %d distintas%s\n",
         n,
         same,
         diff,
         prefix_checked ? " (prefijo incluido)" : " (SIN prefijo en la linea base)");
  printf("manual: por url_lookup() (el camino del operador): %d identicas, %d distintas, "
         "%d a la reserva del buscador\n",
         lookup_same,
         lookup_diff,
         lookup_fallback);
  return (diff == 0) && (lookup_diff == 0);
}

/** \} */

}  // namespace flipendo::manual
