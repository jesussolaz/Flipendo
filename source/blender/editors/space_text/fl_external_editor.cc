/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sptext
 *
 * Abrir un fichero en el editor de texto externo, en C++.
 * Ver FL_external_editor.hh.
 */

#include "FL_external_editor.hh"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#include <sys/wait.h>
#include <unistd.h>

#include "BLT_translation.hh"

#include "DNA_userdef_types.h"

namespace flipendo::text {

/* -------------------------------------------------------------------------- */
/** \name `shlex.split()` en modo POSIX
 * \{ */

bool shlex_split(const std::string &text, std::vector<std::string> &r_args, std::string &r_error)
{
  r_args.clear();
  r_error.clear();

  std::string token;
  bool has_token = false;

  const size_t len = text.size();
  size_t i = 0;
  while (i < len) {
    const char c = text[i];

    /* Espacio en blanco: cierra el token si habia uno. */
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      if (has_token) {
        r_args.push_back(token);
        token.clear();
        has_token = false;
      }
      i++;
      continue;
    }

    if (c == '\'') {
      /* Comilla simple: todo literal hasta la siguiente. `'` no esta en
       * `escapedquotes`, asi que la barra invertida NO escapa aqui dentro. */
      has_token = true;
      i++;
      bool closed = false;
      while (i < len) {
        if (text[i] == '\'') {
          closed = true;
          i++;
          break;
        }
        token.push_back(text[i]);
        i++;
      }
      if (!closed) {
        r_error = "ValueError('No closing quotation')";
        return false;
      }
      continue;
    }

    if (c == '"') {
      /* Comilla doble: `"` SI esta en `escapedquotes`, asi que la barra
       * invertida escapa, pero SOLO `"` y `\`. Ante cualquier otro caracter la
       * barra se queda. Esto es lo que mas se equivoca al transliterar. */
      has_token = true;
      i++;
      bool closed = false;
      while (i < len) {
        const char d = text[i];
        if (d == '"') {
          closed = true;
          i++;
          break;
        }
        if (d == '\\') {
          if (i + 1 >= len) {
            r_error = "ValueError('No escaped character')";
            return false;
          }
          const char e = text[i + 1];
          if (e == '"' || e == '\\') {
            token.push_back(e);
          }
          else {
            token.push_back('\\');
            token.push_back(e);
          }
          i += 2;
          continue;
        }
        token.push_back(d);
        i++;
      }
      if (!closed) {
        r_error = "ValueError('No closing quotation')";
        return false;
      }
      continue;
    }

    if (c == '\\') {
      /* Fuera de comillas la barra invertida escapa cualquier caracter. */
      if (i + 1 >= len) {
        r_error = "ValueError('No escaped character')";
        return false;
      }
      has_token = true;
      token.push_back(text[i + 1]);
      i += 2;
      continue;
    }

    has_token = true;
    token.push_back(c);
    i++;
  }

  if (has_token) {
    r_args.push_back(token);
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name `string.Template.substitute()`
 * \{ */

static bool template_ident_start(char c)
{
  return (c == '_') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool template_ident_char(char c)
{
  return template_ident_start(c) || (c >= '0' && c <= '9');
}

/**
 * Reproduce el mensaje de `Template._invalid()`: la linea y la columna se
 * cuentan sobre `template[:i]`, donde `i` es la posicion **siguiente** al `$`
 * (el grupo `invalid` de la expresion regular es de anchura cero y empieza ahi).
 */
static std::string template_invalid_error(const std::string &text, size_t i)
{
  size_t lineno = 1;
  size_t last_line_start = 0;
  for (size_t k = 0; k < i; k++) {
    if (text[k] == '\n') {
      lineno++;
      last_line_start = k + 1;
    }
  }
  /* `colno = i - len(''.join(lines[:-1]))`, que es `i - inicio_de_la_ultima_linea`. */
  const size_t colno = i - last_line_start;
  char buf[128];
  snprintf(buf,
           sizeof(buf),
           "ValueError('Invalid placeholder in string: line %zu, col %zu')",
           lineno,
           colno);
  return buf;
}

bool template_substitute(const std::string &text,
                         const std::vector<TemplateVar> &vars,
                         std::string &r_out,
                         std::string &r_error)
{
  r_out.clear();
  r_error.clear();

  const size_t len = text.size();
  size_t i = 0;
  while (i < len) {
    const char c = text[i];
    if (c != '$') {
      r_out.push_back(c);
      i++;
      continue;
    }

    /* `i + 1` es donde empieza el grupo `invalid` de Python. */
    if (i + 1 >= len) {
      r_error = template_invalid_error(text, i + 1);
      return false;
    }

    const char n = text[i + 1];
    if (n == '$') {
      r_out.push_back('$');
      i += 2;
      continue;
    }

    size_t name_start;
    size_t name_end;
    size_t next;
    if (n == '{') {
      name_start = i + 2;
      size_t k = name_start;
      if (k >= len || !template_ident_start(text[k])) {
        r_error = template_invalid_error(text, i + 1);
        return false;
      }
      k++;
      while (k < len && template_ident_char(text[k])) {
        k++;
      }
      if (k >= len || text[k] != '}') {
        r_error = template_invalid_error(text, i + 1);
        return false;
      }
      name_end = k;
      next = k + 1;
    }
    else if (template_ident_start(n)) {
      name_start = i + 1;
      size_t k = name_start + 1;
      while (k < len && template_ident_char(text[k])) {
        k++;
      }
      name_end = k;
      next = k;
    }
    else {
      r_error = template_invalid_error(text, i + 1);
      return false;
    }

    const std::string name = text.substr(name_start, name_end - name_start);
    const TemplateVar *found = nullptr;
    for (const TemplateVar &var : vars) {
      if (name == var.name) {
        found = &var;
        break;
      }
    }
    if (found == nullptr) {
      r_error = "KeyError('" + name + "')";
      return false;
    }
    r_out += found->value;
    i = next;
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name El operador
 * \{ */

/** Construye el `argv`, sin lanzar nada. Compartido con el volcador. */
static bool external_editor_argv(const std::string &editor,
                                 const std::string &editor_args,
                                 const std::string &filepath,
                                 int line,
                                 int column,
                                 std::vector<std::string> &r_argv,
                                 std::string &r_error)
{
  r_argv.clear();
  r_argv.push_back(editor);

  std::vector<std::string> split;
  if (!shlex_split(editor_args, split, r_error)) {
    return false;
  }

  const std::vector<TemplateVar> vars = {
      {"filepath", filepath},
      {"line", std::to_string(line + 1)},
      {"column", std::to_string(column + 1)},
      {"line0", std::to_string(line)},
      {"column0", std::to_string(column)},
  };

  for (const std::string &arg : split) {
    std::string out;
    if (!template_substitute(arg, vars, out, r_error)) {
      return false;
    }
    r_argv.push_back(out);
  }
  return true;
}

std::string open_external_editor(const char *filepath, int line, int column)
{
  const std::string editor = U.text_editor;
  const std::string editor_args = U.text_editor_args;

  /* El llamador ya comprueba que hay editor; el Python tenia aqui un `assert`. */
  if (editor.empty()) {
    return RPT_("No text editor set");
  }

  if (editor_args.empty()) {
    return RPT_(
        "Provide text editor argument format in File Paths/Applications Preferences, "
        "see input field tool-tip for more information");
  }

  if (editor_args.find("$filepath") == std::string::npos) {
    return RPT_("Text Editor Args Format must contain $filepath");
  }

  std::vector<std::string> argv;
  std::string error;
  if (!external_editor_argv(editor, editor_args, filepath, line, column, argv, error)) {
    return std::string(RPT_("Exception parsing template: ")) + error;
  }

  /* `subprocess.run(args, check=True)`. */
  std::vector<char *> c_argv;
  c_argv.reserve(argv.size() + 1);
  for (std::string &arg : argv) {
    c_argv.push_back(arg.data());
  }
  c_argv.push_back(nullptr);

  const pid_t pid = fork();
  if (pid == -1) {
    return std::string(RPT_("Exception running external editor: ")) + "OSError('unable to fork')";
  }
  if (pid == 0) {
    execvp(c_argv[0], c_argv.data());
    fflush(stdout);
    fflush(stderr);
    /* `_exit` y no `exit`: no queremos los `atexit` de Blender en el hijo. */
    _exit(errno);
  }

  int wstatus = 0;
  waitpid(pid, &wstatus, 0);
  if (WIFEXITED(wstatus)) {
    const int code = WEXITSTATUS(wstatus);
    if (code == 0) {
      return "";
    }
    if (code == ENOENT) {
      return std::string(RPT_("Exception running external editor: ")) +
             "FileNotFoundError(2, 'No such file or directory')";
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "CalledProcessError(%d)", code);
    return std::string(RPT_("Exception running external editor: ")) + buf;
  }
  return std::string(RPT_("Exception running external editor: ")) + "OSError('process did not exit')";
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Volcado y comparacion
 * \{ */

/** Una linea de caso: `argumentos<TAB>ruta<TAB>linea<TAB>columna`. */
static bool case_parse(const std::string &line,
                       std::string &r_args,
                       std::string &r_path,
                       int &r_line,
                       int &r_col)
{
  size_t a = line.find('\t');
  if (a == std::string::npos) {
    return false;
  }
  size_t b = line.find('\t', a + 1);
  if (b == std::string::npos) {
    return false;
  }
  size_t c = line.find('\t', b + 1);
  if (c == std::string::npos) {
    return false;
  }
  r_args = line.substr(0, a);
  r_path = line.substr(a + 1, b - a - 1);
  r_line = atoi(line.substr(b + 1, c - b - 1).c_str());
  r_col = atoi(line.substr(c + 1).c_str());
  return true;
}

/** Una linea de resultado, para poder compararla con la de Python. */
static std::string case_result(const std::string &args_text,
                               const std::string &filepath,
                               int line,
                               int column)
{
  std::vector<std::string> argv;
  std::string error;
  if (!external_editor_argv("EDITOR", args_text, filepath, line, column, argv, error)) {
    return "ERROR " + error;
  }
  std::string out = "OK";
  for (size_t i = 1; i < argv.size(); i++) { /* El [0] es siempre "EDITOR". */
    out += " |" + argv[i] + "|";
  }
  return out;
}

bool external_editor_dump(const char *cases_path, const char *out_path)
{
  std::ifstream in(cases_path);
  if (!in.is_open()) {
    fprintf(stderr, "editor externo: no puedo abrir '%s'\n", cases_path);
    return false;
  }
  std::ofstream out(out_path, std::ios::binary);
  if (!out.is_open()) {
    fprintf(stderr, "editor externo: no puedo escribir '%s'\n", out_path);
    return false;
  }
  out << "# --fl-dump-external-editor\n";

  std::string line;
  int n = 0;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::string args_text, filepath;
    int ln = 0, col = 0;
    if (!case_parse(line, args_text, filepath, ln, col)) {
      continue;
    }
    n++;
    out << line << "\t" << case_result(args_text, filepath, ln, col) << "\n";
  }
  out.close();
  printf("editor externo: %d casos volcados en '%s'\n", n, out_path);
  return true;
}

bool external_editor_check(const char *baseline_path)
{
  std::ifstream in(baseline_path);
  if (!in.is_open()) {
    fprintf(stderr, "editor externo: no puedo abrir la linea base '%s'\n", baseline_path);
    return false;
  }
  std::string line;
  int n = 0, same = 0, diff = 0;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const size_t last = line.rfind('\t');
    if (last == std::string::npos) {
      continue;
    }
    const std::string expect = line.substr(last + 1);
    const std::string head = line.substr(0, last);
    std::string args_text, filepath;
    int ln = 0, col = 0;
    if (!case_parse(head, args_text, filepath, ln, col)) {
      continue;
    }
    n++;
    const std::string got = case_result(args_text, filepath, ln, col);
    if (got == expect) {
      same++;
    }
    else {
      if (diff < 30) {
        printf("editor externo: DISTINTO  argumentos=<%s>\n  python: %s\n  c++   : %s\n",
               args_text.c_str(),
               expect.c_str(),
               got.c_str());
      }
      diff++;
    }
  }
  printf("editor externo: %d casos comparados, %d identicos, %d distintos\n", n, same, diff);
  return diff == 0;
}

/** \} */

}  // namespace flipendo::text
