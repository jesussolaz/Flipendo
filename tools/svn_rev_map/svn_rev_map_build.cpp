/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Conversor de un solo uso: rev_to_sha1.py -> svn_rev_map.bin.
 *
 * Por que parte del .py y no de git
 * --------------------------------
 * El generador original (tools/git/git_sh1_to_svn_rev.py) emparejaba revisiones con
 * commits POR MARCA DE TIEMPO al segundo, y se quedaba con el ultimo commit de cada
 * segundo. Contra la historia que hoy alcanza `git log --all` en este arbol eso ya
 * no funciona: hay colisiones de segundo de sobra para perder miles de pares en
 * silencio. Regenerar desde git produciria una tabla incompleta que parece correcta.
 *
 * Asi que la fuente de verdad es la tabla que ya existe. Esto es una conversion de
 * formato 1:1, verificable por conteo y por comparacion par a par.
 *
 * Uso:
 *   svn_rev_map_build <rev_to_sha1.py> <salida.bin> [--verificar <sha1_to_rev.py>]
 */

#include "svn_rev_map.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace flipendo;

static bool hex_to_sha1(const std::string &s, uint8_t out[20])
{
  if (s.size() != 40) {
    return false;
  }
  for (int i = 0; i < 20; i++) {
    unsigned int byte = 0;
    for (int half = 0; half < 2; half++) {
      const char c = s[i * 2 + half];
      unsigned int v;
      if (c >= '0' && c <= '9') v = unsigned(c - '0');
      else if (c >= 'a' && c <= 'f') v = unsigned(c - 'a' + 10);
      else return false;
      byte = (byte << 4) | v;
    }
    out[i] = uint8_t(byte);
  }
  return true;
}

/* Parte "18402" o "18402.1" en (major, minor). Las 7 revisiones de rama de la tabla
 * llegan aqui como decimal; sin el minor colisionarian con su revision entera. */
static bool parse_rev(const std::string &s, uint32_t *r_major, uint8_t *r_minor)
{
  const size_t dot = s.find('.');
  try {
    if (dot == std::string::npos) {
      *r_major = uint32_t(std::stoul(s));
      *r_minor = 0;
    }
    else {
      *r_major = uint32_t(std::stoul(s.substr(0, dot)));
      const unsigned long minor = std::stoul(s.substr(dot + 1));
      if (minor > 255) {
        return false;
      }
      *r_minor = uint8_t(minor);
    }
  }
  catch (...) {
    return false;
  }
  return true;
}

/* Lee las lineas `<rev>: "<sha1>",` del diccionario literal. */
static bool parse_rev_to_sha1(const std::string &path, std::vector<SvnRevEntry> &out)
{
  std::ifstream f(path);
  if (!f) {
    std::fprintf(stderr, "error: no se pudo abrir %s\n", path.c_str());
    return false;
  }
  std::string line;
  size_t lineno = 0;
  while (std::getline(f, line)) {
    lineno++;
    const size_t colon = line.find(':');
    if (colon == std::string::npos || colon == 0) {
      continue;
    }
    const std::string rev = line.substr(0, colon);
    if (rev.find_first_not_of("0123456789.") != std::string::npos) {
      continue;
    }
    const size_t q1 = line.find('"', colon);
    if (q1 == std::string::npos) {
      continue;
    }
    const size_t q2 = line.find('"', q1 + 1);
    if (q2 == std::string::npos) {
      continue;
    }
    SvnRevEntry e;
    if (!parse_rev(rev, &e.rev_major, &e.rev_minor)) {
      std::fprintf(stderr, "error: revision ilegible en %s:%zu: '%s'\n",
                   path.c_str(), lineno, rev.c_str());
      return false;
    }
    if (!hex_to_sha1(line.substr(q1 + 1, q2 - q1 - 1), e.sha1)) {
      std::fprintf(stderr, "error: sha1 ilegible en %s:%zu\n", path.c_str(), lineno);
      return false;
    }
    out.push_back(e);
  }
  return true;
}

/* Lee las lineas `"<sha1>": <rev>,` del diccionario inverso, para contrastar. */
static bool parse_sha1_to_rev(const std::string &path,
                              std::unordered_map<std::string, std::string> &out)
{
  std::ifstream f(path);
  if (!f) {
    std::fprintf(stderr, "error: no se pudo abrir %s\n", path.c_str());
    return false;
  }
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] != '"') {
      continue;
    }
    const size_t q2 = line.find('"', 1);
    if (q2 == std::string::npos) {
      continue;
    }
    const size_t colon = line.find(':', q2);
    if (colon == std::string::npos) {
      continue;
    }
    std::string rev = line.substr(colon + 1);
    while (!rev.empty() && (rev.front() == ' ' || rev.front() == '\t')) rev.erase(rev.begin());
    while (!rev.empty() && (rev.back() == ',' || rev.back() == ' ' || rev.back() == '\r')) rev.pop_back();
    if (rev.empty() || rev.find_first_not_of("0123456789.") != std::string::npos) {
      continue;
    }
    out[line.substr(1, q2 - 1)] = rev;
  }
  return true;
}

int main(int argc, char **argv)
{
  if (argc < 3) {
    std::fprintf(stderr,
                 "uso: svn_rev_map_build <rev_to_sha1.py> <salida.bin> "
                 "[--verificar <sha1_to_rev.py>]\n");
    return 1;
  }
  const std::string src = argv[1];
  const std::string dst = argv[2];
  std::string inverse_src;
  for (int i = 3; i + 1 < argc; i++) {
    if (std::strcmp(argv[i], "--verificar") == 0) {
      inverse_src = argv[i + 1];
    }
  }

  std::vector<SvnRevEntry> entries;
  if (!parse_rev_to_sha1(src, entries)) {
    return 1;
  }
  std::printf("leidos %zu pares de %s\n", entries.size(), src.c_str());

  SvnRevMap map;
  map.set_entries(std::move(entries));

  /* Una clave duplicada significaria que la conversion pierde datos en silencio. */
  for (size_t i = 1; i < map.size(); i++) {
    const SvnRevEntry &a = map.entries()[i - 1];
    const SvnRevEntry &b = map.entries()[i];
    if (a.rev_major == b.rev_major && a.rev_minor == b.rev_minor) {
      std::fprintf(stderr, "error: revision duplicada tras ordenar: %s\n",
                   b.rev_text().c_str());
      return 1;
    }
  }

  std::string err;
  if (!map.write(dst, &err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  std::printf("escrito %s (%zu registros)\n", dst.c_str(), map.size());

  /* Releer y comprobar que el asset devuelve exactamente lo mismo que el .py. */
  SvnRevMap check;
  if (!check.load(dst, &err)) {
    std::fprintf(stderr, "error al releer: %s\n", err.c_str());
    return 1;
  }
  if (check.size() != map.size()) {
    std::fprintf(stderr, "error: releido %zu registros, escritos %zu\n",
                 check.size(), map.size());
    return 1;
  }
  size_t fallos = 0;
  for (const SvnRevEntry &e : map.entries()) {
    /* Ida: cada revision devuelve su sha1. Es 1:1 y debe cuadrar siempre. */
    const SvnRevEntry *a = check.find_by_rev(e.rev_text());
    if (!a || a->sha1_text() != e.sha1_text()) {
      fallos++;
    }
    /* Vuelta: 23 sha1 los comparten dos revisiones, asi que no se puede exigir volver
     * a la misma; lo que se exige es que la entrada devuelta lleve ese sha1. Cual de
     * las dos es la buena lo decide el contraste con sha1_to_rev.py de mas abajo. */
    const SvnRevEntry *b = check.find_by_sha1(e.sha1_text());
    if (!b || b->sha1_text() != e.sha1_text()) {
      fallos++;
    }
  }
  if (fallos) {
    std::fprintf(stderr, "error: %zu consultas no coinciden\n", fallos);
    return 1;
  }
  std::printf("ida y vuelta correcta para los %zu pares\n", check.size());

  if (!inverse_src.empty()) {
    std::unordered_map<std::string, std::string> inverse;
    if (!parse_sha1_to_rev(inverse_src, inverse)) {
      return 1;
    }
    size_t malas = 0;
    for (const auto &kv : inverse) {
      const SvnRevEntry *e = check.find_by_sha1(kv.first);
      if (!e || e->rev_text() != kv.second) malas++;
    }
    std::printf("contraste con %s: %zu pares, %zu discrepancias\n",
                inverse_src.c_str(), inverse.size(), malas);
    if (malas) {
      return 1;
    }
  }

  return 0;
}
