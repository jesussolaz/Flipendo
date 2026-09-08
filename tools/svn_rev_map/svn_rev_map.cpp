/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Lector/escritor del mapa revision SVN <-> SHA1. Ver svn_rev_map.hpp. */

#include "svn_rev_map.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace flipendo {

static const char MAGIC[4] = {'F', 'L', 'S', 'V'};
static const uint32_t FORMAT_VERSION = 1;
static const size_t HEADER_SIZE = 16;
static const size_t RECORD_SIZE = 28;

/* ------------------------------------------------------------------ */

std::string SvnRevEntry::rev_text() const
{
  std::string s = std::to_string(rev_major);
  if (rev_minor != 0) {
    s += '.';
    s += std::to_string(rev_minor);
  }
  return s;
}

std::string SvnRevEntry::sha1_text() const
{
  static const char *hex = "0123456789abcdef";
  std::string s(40, '0');
  for (int i = 0; i < 20; i++) {
    s[i * 2] = hex[sha1[i] >> 4];
    s[i * 2 + 1] = hex[sha1[i] & 0xF];
  }
  return s;
}

/* ------------------------------------------------------------------ */

static bool rev_less(const SvnRevEntry &a, const SvnRevEntry &b)
{
  if (a.rev_major != b.rev_major) {
    return a.rev_major < b.rev_major;
  }
  return a.rev_minor < b.rev_minor;
}

static int sha1_cmp(const uint8_t a[20], const uint8_t b[20])
{
  return std::memcmp(a, b, 20);
}

static void put_u32(uint8_t *p, uint32_t v)
{
  p[0] = uint8_t(v);
  p[1] = uint8_t(v >> 8);
  p[2] = uint8_t(v >> 16);
  p[3] = uint8_t(v >> 24);
}

static uint32_t get_u32(const uint8_t *p)
{
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}

void SvnRevMap::rebuild_index()
{
  by_sha1_.resize(entries_.size());
  for (size_t i = 0; i < entries_.size(); i++) {
    by_sha1_[i] = uint32_t(i);
  }
  /* 23 SHA1 de la tabla estan compartidos por dos revisiones consecutivas (dos
   * revisiones SVN que produjeron el mismo commit). El diccionario inverso original
   * (sha1_to_rev.py) se quedaba con la ULTIMA que escribia, o sea la mayor, y por eso
   * tiene 54.335 entradas y no 54.358. Para responder igual, en caso de empate de
   * sha1 ordenamos la revision mayor primero: la busqueda binaria la encuentra a ella. */
  std::sort(by_sha1_.begin(), by_sha1_.end(), [this](uint32_t a, uint32_t b) {
    const int c = sha1_cmp(entries_[a].sha1, entries_[b].sha1);
    if (c != 0) {
      return c < 0;
    }
    return rev_less(entries_[b], entries_[a]);
  });
}

void SvnRevMap::set_entries(std::vector<SvnRevEntry> entries)
{
  entries_ = std::move(entries);
  std::sort(entries_.begin(), entries_.end(), rev_less);
  rebuild_index();
}

/* ------------------------------------------------------------------ */

bool SvnRevMap::write(const std::string &path, std::string *r_error) const
{
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    if (r_error) *r_error = "no se pudo escribir " + path;
    return false;
  }

  const uint32_t count = uint32_t(entries_.size());

  uint8_t header[HEADER_SIZE] = {};
  std::memcpy(header, MAGIC, 4);
  put_u32(header + 4, FORMAT_VERSION);
  put_u32(header + 8, count);
  put_u32(header + 12, 0);
  f.write(reinterpret_cast<const char *>(header), HEADER_SIZE);

  for (const SvnRevEntry &e : entries_) {
    uint8_t rec[RECORD_SIZE] = {};
    put_u32(rec, e.rev_major);
    rec[4] = e.rev_minor;
    std::memcpy(rec + 5, e.sha1, 20);
    /* rec[25..27] queda a cero: relleno para alinear el registro a 4 bytes. */
    f.write(reinterpret_cast<const char *>(rec), RECORD_SIZE);
  }

  for (uint32_t idx : by_sha1_) {
    uint8_t buf[4];
    put_u32(buf, idx);
    f.write(reinterpret_cast<const char *>(buf), 4);
  }

  if (!f.good()) {
    if (r_error) *r_error = "error al escribir " + path;
    return false;
  }
  return true;
}

bool SvnRevMap::load(const std::string &path, std::string *r_error)
{
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    if (r_error) *r_error = "no se pudo abrir " + path;
    return false;
  }

  uint8_t header[HEADER_SIZE];
  f.read(reinterpret_cast<char *>(header), HEADER_SIZE);
  if (!f || std::memcmp(header, MAGIC, 4) != 0) {
    if (r_error) *r_error = path + ": no es un svn_rev_map (magic incorrecto)";
    return false;
  }
  const uint32_t version = get_u32(header + 4);
  if (version != FORMAT_VERSION) {
    if (r_error) {
      *r_error = path + ": version de formato " + std::to_string(version) + ", se esperaba " +
                 std::to_string(FORMAT_VERSION);
    }
    return false;
  }
  const uint32_t count = get_u32(header + 8);

  entries_.resize(count);
  for (uint32_t i = 0; i < count; i++) {
    uint8_t rec[RECORD_SIZE];
    f.read(reinterpret_cast<char *>(rec), RECORD_SIZE);
    if (!f) {
      if (r_error) *r_error = path + ": fichero truncado en el registro " + std::to_string(i);
      return false;
    }
    entries_[i].rev_major = get_u32(rec);
    entries_[i].rev_minor = rec[4];
    std::memcpy(entries_[i].sha1, rec + 5, 20);
  }

  by_sha1_.resize(count);
  for (uint32_t i = 0; i < count; i++) {
    uint8_t buf[4];
    f.read(reinterpret_cast<char *>(buf), 4);
    if (!f) {
      if (r_error) *r_error = path + ": indice truncado en la entrada " + std::to_string(i);
      return false;
    }
    by_sha1_[i] = get_u32(buf);
  }

  return true;
}

/* ------------------------------------------------------------------ */

const SvnRevEntry *SvnRevMap::find_by_rev(const std::string &rev_text) const
{
  SvnRevEntry key;
  const size_t dot = rev_text.find('.');
  try {
    if (dot == std::string::npos) {
      key.rev_major = uint32_t(std::stoul(rev_text));
      key.rev_minor = 0;
    }
    else {
      key.rev_major = uint32_t(std::stoul(rev_text.substr(0, dot)));
      key.rev_minor = uint8_t(std::stoul(rev_text.substr(dot + 1)));
    }
  }
  catch (...) {
    return nullptr;
  }

  auto it = std::lower_bound(entries_.begin(), entries_.end(), key, rev_less);
  if (it == entries_.end() || it->rev_major != key.rev_major || it->rev_minor != key.rev_minor) {
    return nullptr;
  }
  return &*it;
}

const SvnRevEntry *SvnRevMap::find_by_sha1(const std::string &sha1_text) const
{
  if (sha1_text.size() != 40) {
    return nullptr;
  }
  uint8_t key[20];
  for (int i = 0; i < 20; i++) {
    unsigned int byte = 0;
    for (int half = 0; half < 2; half++) {
      const char c = sha1_text[i * 2 + half];
      unsigned int v;
      if (c >= '0' && c <= '9') v = unsigned(c - '0');
      else if (c >= 'a' && c <= 'f') v = unsigned(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') v = unsigned(c - 'A' + 10);
      else return nullptr;
      byte = (byte << 4) | v;
    }
    key[i] = uint8_t(byte);
  }

  auto it = std::lower_bound(
      by_sha1_.begin(), by_sha1_.end(), key, [this](uint32_t idx, const uint8_t *k) {
        return sha1_cmp(entries_[idx].sha1, k) < 0;
      });
  if (it == by_sha1_.end() || sha1_cmp(entries_[*it].sha1, key) != 0) {
    return nullptr;
  }
  return &entries_[*it];
}

}  // namespace flipendo
