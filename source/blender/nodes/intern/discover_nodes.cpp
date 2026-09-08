/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Generador de registro de nodos — reemplazo en C++ de discover_nodes.py.
 *
 * Doctrina (politicas/LENGUAJE-CPP.md): todo el codigo propio de Flipendo es C++.
 * Este script era la ultima pieza de Python que el BUILD ejecutaba: mientras
 * existiera, Flipendo no se podia compilar sin un interprete de Python instalado.
 *
 * Que hace: recorre los .cc de un modulo de nodos buscando invocaciones de la
 * macro NOD_REGISTER_NODE(nombre), lleva la cuenta del namespace en el que
 * aparece cada una, y emite un .cc que declara y llama a cada <nombre>_discover().
 * Asi cada nodo se registra solo, sin una lista central escrita a mano.
 *
 * Uso (identico al del script al que sustituye):
 *   discover_nodes <raiz/fuentes> <salida.cc> <nombre_funcion> <fuente.cc>...
 *                  [--use-makefile-workaround]
 *
 * La salida es byte a byte la misma que producia el script. */

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <utime.h>
#include <vector>

namespace {

constexpr const char *MACRO_NAME = "NOD_REGISTER_NODE";
constexpr const char *DISCOVER_SUFFIX = "_discover";

bool is_ident_char(char c)
{
  /* Equivalente a \w de Python (ASCII): letras, digitos y guion bajo. */
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

/* Equivalente a [\w:]+ — el juego de caracteres de un nombre de namespace anidado. */
bool is_namespace_char(char c)
{
  return is_ident_char(c) || c == ':';
}

/* Lee el mayor prefijo de `s` desde `pos` formado por caracteres de namespace.
 * Devuelve la longitud consumida (0 si no hay ninguno). */
size_t scan_namespace_name(const std::string &s, size_t pos, std::string &out)
{
  size_t end = pos;
  while (end < s.size() && is_namespace_char(s[end])) {
    end++;
  }
  out = s.substr(pos, end - pos);
  return end - pos;
}

std::vector<std::string> split_namespace(const std::string &name)
{
  std::vector<std::string> parts;
  size_t start = 0;
  while (true) {
    const size_t sep = name.find("::", start);
    if (sep == std::string::npos) {
      parts.push_back(name.substr(start));
      break;
    }
    parts.push_back(name.substr(start, sep - start));
    start = sep + 2;
  }
  return parts;
}

std::string join_namespace(const std::vector<std::string> &parts)
{
  std::string out;
  for (size_t i = 0; i < parts.size(); i++) {
    if (i != 0) {
      out += "::";
    }
    out += parts[i];
  }
  return out;
}

bool read_file(const std::string &path, std::string &out)
{
  std::ifstream fh(path, std::ios::binary);
  if (!fh) {
    return false;
  }
  std::ostringstream ss;
  ss << fh.rdbuf();
  out = ss.str();
  return true;
}

/* El script solo tocaba la fecha del fichero generado si era mas antiguo que
 * alguna de sus entradas (workaround del generador Unix Makefiles). */
bool file_is_older(const std::string &path, const std::vector<std::string> &others)
{
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    return false;
  }
  const time_t mtime = st.st_mtime;
  for (const std::string &other : others) {
    struct stat other_st;
    if (stat(other.c_str(), &other_st) == 0 && mtime < other_st.st_mtime) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main(int argc, char **argv)
{
  std::vector<std::string> args;
  bool use_makefile_workaround = false;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--use-makefile-workaround") == 0) {
      use_makefile_workaround = true;
      continue;
    }
    args.push_back(argv[i]);
  }

  if (args.size() < 3) {
    std::fprintf(stderr,
                 "uso: discover_nodes <raiz/fuentes> <salida.cc> <nombre_funcion> "
                 "<fuente.cc>... [--use-makefile-workaround]\n");
    return 1;
  }

  const std::string source_root = args[0];
  const std::string output_cc_file = args[1];
  const std::string function_to_generate = args[2];

  std::vector<std::string> source_cc_files;
  for (size_t i = 3; i < args.size(); i++) {
    const std::string &path = args[i];
    if (path.size() >= 3 && path.compare(path.size() - 3, 3, ".cc") == 0) {
      source_cc_files.push_back(source_root + "/" + path);
    }
  }

  std::vector<std::string> decl_lines;
  std::vector<std::string> func_lines;

  /* Declaracion adelantada, para no arrastrar un aviso del compilador. */
  func_lines.push_back("void " + function_to_generate + "();");
  func_lines.push_back("void " + function_to_generate + "()");
  func_lines.push_back("{");

  const std::string macro_open = std::string(MACRO_NAME) + "(";

  for (const std::string &path : source_cc_files) {
    std::string code;
    if (!read_file(path, code)) {
      std::fprintf(stderr, "discover_nodes: no se pudo leer %s\n", path.c_str());
      return 1;
    }

    /* El namespace en el que estamos ahora mismo. */
    std::vector<std::string> namespace_parts;

    /* Se recorre linea a linea porque los tres patrones del script eran
     * ^namespace X {, ^}  // namespace X y la macro; el orden de aparicion es
     * lo que determina el namespace de cada macro. */
    size_t line_start = 0;
    while (line_start <= code.size()) {
      size_t line_end = code.find('\n', line_start);
      if (line_end == std::string::npos) {
        line_end = code.size();
      }
      const std::string line = code.substr(line_start, line_end - line_start);

      /* ^namespace ([\w:]+) \{ */
      static const std::string ns_open = "namespace ";
      static const std::string ns_close = "}  // namespace ";
      bool handled = false;

      if (line.compare(0, ns_open.size(), ns_open) == 0) {
        std::string name;
        const size_t len = scan_namespace_name(line, ns_open.size(), name);
        if (len > 0 && line.compare(ns_open.size() + len, 2, " {") == 0) {
          for (const std::string &part : split_namespace(name)) {
            namespace_parts.push_back(part);
          }
          handled = true;
        }
      }
      else if (line.compare(0, ns_close.size(), ns_close) == 0) {
        std::string name;
        const size_t len = scan_namespace_name(line, ns_close.size(), name);
        if (len > 0) {
          const size_t n = split_namespace(name).size();
          if (n <= namespace_parts.size()) {
            namespace_parts.resize(namespace_parts.size() - n);
          }
          handled = true;
        }
      }

      if (!handled) {
        /* NOD_REGISTER_NODE\((\w+)\) — puede haber varias en la misma linea. */
        size_t pos = 0;
        while ((pos = line.find(macro_open, pos)) != std::string::npos) {
          const size_t name_start = pos + macro_open.size();
          size_t name_end = name_start;
          while (name_end < line.size() && is_ident_char(line[name_end])) {
            name_end++;
          }
          if (name_end > name_start && name_end < line.size() && line[name_end] == ')') {
            const std::string function_name = line.substr(name_start, name_end - name_start);
            const std::string namespace_str = join_namespace(namespace_parts);
            const std::string auto_run_name = function_name + DISCOVER_SUFFIX;

            /* No puede declararse en un namespace anonimo: la haria estatica. */
            if (!namespace_str.empty()) {
              decl_lines.push_back("namespace " + namespace_str + " {");
            }
            decl_lines.push_back("void " + auto_run_name + "();");
            if (!namespace_str.empty()) {
              decl_lines.push_back("}");
            }

            func_lines.push_back("  " + namespace_str + "::" + auto_run_name + "();");
          }
          pos = name_start;
        }
      }

      if (line_end == code.size()) {
        break;
      }
      line_start = line_end + 1;
    }
  }

  func_lines.push_back("}");

  /* Union con "\n", sin salto final: exactamente lo que escribia el script. */
  std::string new_generated_code;
  {
    std::vector<std::string> all;
    all.insert(all.end(), decl_lines.begin(), decl_lines.end());
    all.push_back("");
    all.insert(all.end(), func_lines.begin(), func_lines.end());
    for (size_t i = 0; i < all.size(); i++) {
      if (i != 0) {
        new_generated_code += "\n";
      }
      new_generated_code += all[i];
    }
  }

  std::string old_generated_code;
  read_file(output_cc_file, old_generated_code);

  if (old_generated_code != new_generated_code) {
    std::ofstream fh(output_cc_file, std::ios::binary | std::ios::trunc);
    if (!fh) {
      std::fprintf(stderr, "discover_nodes: no se pudo escribir %s\n", output_cc_file.c_str());
      return 1;
    }
    fh << new_generated_code;
  }
  else if (use_makefile_workaround && file_is_older(output_cc_file, source_cc_files)) {
    /* Sin esto el fichero se regeneraria en cada pasada del generador Makefiles. */
    utime(output_cc_file.c_str(), nullptr);
  }

  return 0;
}
