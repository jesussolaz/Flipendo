/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Mapa revision SVN <-> SHA1 de git de la era SVN de Blender (r2 de 2002-10-12 a
 * r61240 de 2013-11-12, cuando Blender abandono SVN). 54.358 pares.
 *
 * Para que sirve: el codigo heredado tiene comentarios que citan "rev NNNNN"; esto
 * traduce esa cita al commit real, y a la inversa.
 *
 * Antes eran dos ficheros .py de 2,7 MB cada uno con un diccionario literal — la
 * mayor concentracion de Python del arbol (108.736 lineas, el 30% del total) para
 * cero logica. Ahora son un asset binario y este lector. La doctrina
 * (politicas/LENGUAJE-CPP.md) dice que los datos serializados no son codigo: lo que
 * no puede quedarse es el .py, no la tabla.
 *
 * Formato de svn_rev_map.bin (little-endian):
 *   cabecera 16 B : magic 'FLSV', u32 version, u32 count, u32 reservado
 *   count x 28 B  : u32 rev_major, u8 rev_minor, u8 sha1[20], u8 pad[3]
 *                   ordenados por (rev_major, rev_minor)
 *   count x u32   : indices de registro ordenados por sha1, para la busqueda inversa
 *
 * El campo rev_minor NO es decoracion: 7 de las 54.358 claves son revisiones de
 * rama escritas como decimal (18402.1, 19226.1, 19226.2, 20350.1, 24062.1,
 * 29506.2, 36866.1). Un u32 a secas las perderia o las haria colisionar con su
 * revision entera.
 */

#ifndef __FL_SVN_REV_MAP_HPP__
#define __FL_SVN_REV_MAP_HPP__

#include <cstdint>
#include <string>
#include <vector>

namespace flipendo {

struct SvnRevEntry {
  uint32_t rev_major = 0;
  uint8_t rev_minor = 0; /* 0 = revision entera; 1..2 = revision de rama */
  uint8_t sha1[20] = {};

  /* "18402" o "18402.1", como aparecia como clave en el .py. */
  std::string rev_text() const;
  /* Los 40 caracteres hexadecimales en minusculas. */
  std::string sha1_text() const;
};

class SvnRevMap {
 public:
  /* Carga el asset. Devuelve false y deja un motivo en `r_error`. */
  bool load(const std::string &path, std::string *r_error = nullptr);

  /* Construye en memoria (lo usa el conversor antes de escribir). */
  void set_entries(std::vector<SvnRevEntry> entries);

  /* Escribe el asset en el formato de arriba. */
  bool write(const std::string &path, std::string *r_error = nullptr) const;

  size_t size() const { return entries_.size(); }
  const std::vector<SvnRevEntry> &entries() const { return entries_; }

  /* "18402" o "18402.1" -> entrada, o nullptr si no esta. */
  const SvnRevEntry *find_by_rev(const std::string &rev_text) const;
  /* 40 hex -> entrada, o nullptr. */
  const SvnRevEntry *find_by_sha1(const std::string &sha1_text) const;

 private:
  void rebuild_index();

  std::vector<SvnRevEntry> entries_;   /* ordenadas por (rev_major, rev_minor) */
  std::vector<uint32_t> by_sha1_;      /* indices, ordenados por sha1 */
};

}  // namespace flipendo

#endif /* __FL_SVN_REV_MAP_HPP__ */
