/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Lectura y escritura de presets como datos. Ver FL_preset.hpp.
 *
 * Aqui viven tres cosas: la gramatica de valores (compartida entre el formato
 * nativo y el subconjunto de Python heredado), el lector/escritor de `.fpreset`
 * y el lector nativo de los `.py` que el editor generaba hasta ahora.
 */

#include "FL_preset.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

namespace flipendo::preset {

Value Value::make_bool(bool v)
{
  Value out;
  out.kind = ValueKind::Bool;
  out.b = v;
  return out;
}
Value Value::make_int(long long v)
{
  Value out;
  out.kind = ValueKind::Int;
  out.i = v;
  return out;
}
Value Value::make_float(double v, bool single)
{
  Value out;
  out.kind = ValueKind::Float;
  out.f = v;
  out.f_single = single;
  return out;
}
Value Value::make_string(std::string v)
{
  Value out;
  out.kind = ValueKind::String;
  out.s = std::move(v);
  return out;
}
Value Value::make_none()
{
  return Value();
}

/* -------------------------------------------------------------------------- */
/** \name Formato de valores
 *
 * Los flotantes se imprimen con la representacion MAS CORTA que vuelve a leerse
 * como el mismo numero. Para una propiedad RNA de precision simple se exige que
 * vuelva el mismo `float`, no el mismo `double`: asi `0.20000000298023224`, que
 * es lo que escribia el `repr()` de Python al promover un `float` a `double`,
 * queda como `0.2` sin perder ni un bit.
 * \{ */

static std::string format_number(double v, bool single)
{
  if (std::isnan(v)) {
    return "nan";
  }
  if (std::isinf(v)) {
    return v < 0.0 ? "-inf" : "inf";
  }
  char buf[64];
  char fallback[64] = {0};
  const int max_prec = single ? 9 : 17;
  bool found = false;
  for (int prec = 1; prec <= max_prec; prec++) {
    std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
    const bool round_trips = single ? (float(std::strtod(buf, nullptr)) == float(v)) :
                                      (std::strtod(buf, nullptr) == v);
    if (!round_trips) {
      continue;
    }
    /* Se prefiere la forma decimal a la exponencial aunque sea mas larga: `100.0`
     * se lee mejor que `1e+02`, y las dos son el mismo numero. Solo se cae en la
     * exponencial si ninguna precision da una forma sin `e`. */
    if (std::strchr(buf, 'e') == nullptr) {
      found = true;
      break;
    }
    if (fallback[0] == '\0') {
      std::snprintf(fallback, sizeof(fallback), "%s", buf);
    }
  }
  if (!found) {
    std::snprintf(buf, sizeof(buf), "%s", fallback[0] ? fallback : buf);
  }
  /* Sin punto ni exponente se leeria como entero. */
  std::string out(buf);
  if (out.find('.') == std::string::npos && out.find('e') == std::string::npos &&
      out.find('E') == std::string::npos && out.find("inf") == std::string::npos &&
      out.find("nan") == std::string::npos)
  {
    out += ".0";
  }
  return out;
}

static std::string format_string(const std::string &s)
{
  std::string out;
  out.reserve(s.size() + 2);
  out += '"';
  for (const char c : s) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  out += '"';
  return out;
}

std::string format_value(const Value &value)
{
  switch (value.kind) {
    case ValueKind::None:
      return "none";
    case ValueKind::Bool:
      return value.b ? "true" : "false";
    case ValueKind::Int: {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%lld", value.i);
      return buf;
    }
    case ValueKind::Float:
      return format_number(value.f, value.f_single);
    case ValueKind::String:
      return format_string(value.s);
    case ValueKind::Array:
    case ValueKind::EnumSet: {
      const bool set = value.kind == ValueKind::EnumSet;
      std::string out(1, set ? '{' : '[');
      for (size_t i = 0; i < value.items.size(); i++) {
        if (i != 0) {
          out += ", ";
        }
        out += format_value(value.items[i]);
      }
      out += set ? '}' : ']';
      return out;
    }
  }
  return "none";
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Analizador de literales
 *
 * Uno solo para los dos dialectos. `python` cambia tres cosas: las constantes se
 * escriben `True`/`False`/`None`, las cadenas admiten comillas simples, y se
 * acepta el producto de enteros (`224 * 8`), que aparece escrito a mano en los
 * presets de FFmpeg heredados.
 * \{ */

namespace {

struct Scanner {
  const std::string &src;
  size_t pos = 0;
  bool python = false;
  std::string error;

  Scanner(const std::string &s, bool py) : src(s), python(py) {}

  void skip_ws()
  {
    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t')) {
      pos++;
    }
  }
  bool at_end()
  {
    skip_ws();
    return pos >= src.size();
  }
  bool word(const char *w)
  {
    const size_t len = std::strlen(w);
    if (src.compare(pos, len, w) != 0) {
      return false;
    }
    const size_t after = pos + len;
    if (after < src.size() && (std::isalnum(uchar(src[after])) || src[after] == '_')) {
      return false;
    }
    pos = after;
    return true;
  }
  static unsigned char uchar(char c)
  {
    return static_cast<unsigned char>(c);
  }
  bool fail(const std::string &msg)
  {
    if (error.empty()) {
      error = msg;
    }
    return false;
  }
  bool parse_value(Value &r_value);
  bool parse_string(Value &r_value);
  bool parse_number(Value &r_value);
  bool parse_sequence(char close, ValueKind kind, Value &r_value);
};

bool Scanner::parse_string(Value &r_value)
{
  const char quote = src[pos];
  pos++;
  std::string out;
  while (pos < src.size() && src[pos] != quote) {
    char c = src[pos++];
    if (c == '\\' && pos < src.size()) {
      const char esc = src[pos++];
      switch (esc) {
        case 'n':
          out += '\n';
          break;
        case 'r':
          out += '\r';
          break;
        case 't':
          out += '\t';
          break;
        case '\\':
          out += '\\';
          break;
        case '"':
          out += '"';
          break;
        case '\'':
          out += '\'';
          break;
        case 'x': {
          if (pos + 1 < src.size()) {
            const std::string hex = src.substr(pos, 2);
            out += char(std::strtol(hex.c_str(), nullptr, 16));
            pos += 2;
          }
          break;
        }
        default:
          out += esc;
          break;
      }
    }
    else {
      out += c;
    }
  }
  if (pos >= src.size()) {
    return fail("cadena sin cerrar");
  }
  pos++; /* Comilla de cierre. */
  r_value = Value::make_string(out);
  return true;
}

bool Scanner::parse_number(Value &r_value)
{
  const size_t start = pos;
  if (pos < src.size() && (src[pos] == '-' || src[pos] == '+')) {
    pos++;
  }
  if (word("inf") || word("Infinity")) {
    r_value = Value::make_float(src[start] == '-' ? -INFINITY : INFINITY, false);
    return true;
  }
  if (word("nan")) {
    r_value = Value::make_float(NAN, false);
    return true;
  }
  bool is_float = false;
  while (pos < src.size()) {
    const char c = src[pos];
    if (std::isdigit(uchar(c))) {
      pos++;
    }
    else if (c == '.') {
      is_float = true;
      pos++;
    }
    else if (c == 'e' || c == 'E') {
      is_float = true;
      pos++;
      if (pos < src.size() && (src[pos] == '-' || src[pos] == '+')) {
        pos++;
      }
    }
    else {
      break;
    }
  }
  if (pos == start) {
    return fail("se esperaba un valor");
  }
  const std::string text = src.substr(start, pos - start);
  if (is_float) {
    r_value = Value::make_float(std::strtod(text.c_str(), nullptr), false);
  }
  else {
    r_value = Value::make_int(std::strtoll(text.c_str(), nullptr, 10));
  }
  return true;
}

bool Scanner::parse_sequence(char close, ValueKind kind, Value &r_value)
{
  pos++; /* Delimitador de apertura. */
  r_value = Value();
  r_value.kind = kind;
  skip_ws();
  if (pos < src.size() && src[pos] == close) {
    pos++;
    return true;
  }
  while (true) {
    Value item;
    if (!parse_value(item)) {
      return false;
    }
    r_value.items.push_back(item);
    skip_ws();
    if (pos < src.size() && src[pos] == ',') {
      pos++;
      skip_ws();
      /* Tupla de un solo elemento en Python: `(1,)`. */
      if (pos < src.size() && src[pos] == close) {
        pos++;
        return true;
      }
      continue;
    }
    if (pos < src.size() && src[pos] == close) {
      pos++;
      return true;
    }
    return fail("falta ',' o el cierre de la secuencia");
  }
}

bool Scanner::parse_value(Value &r_value)
{
  skip_ws();
  if (pos >= src.size()) {
    return fail("se esperaba un valor");
  }
  const char c = src[pos];
  bool ok = false;
  if (c == '[') {
    ok = parse_sequence(']', ValueKind::Array, r_value);
  }
  else if (c == '(') {
    if (!python) {
      return fail("'(' no es un valor del formato");
    }
    ok = parse_sequence(')', ValueKind::Array, r_value);
  }
  else if (c == '{') {
    ok = parse_sequence('}', ValueKind::EnumSet, r_value);
  }
  else if (c == '"' || (python && c == '\'')) {
    ok = parse_string(r_value);
  }
  else if (python && word("True")) {
    r_value = Value::make_bool(true);
    ok = true;
  }
  else if (python && word("False")) {
    r_value = Value::make_bool(false);
    ok = true;
  }
  else if (python && word("None")) {
    r_value = Value::make_none();
    ok = true;
  }
  else if (!python && word("true")) {
    r_value = Value::make_bool(true);
    ok = true;
  }
  else if (!python && word("false")) {
    r_value = Value::make_bool(false);
    ok = true;
  }
  else if (!python && word("none")) {
    r_value = Value::make_none();
    ok = true;
  }
  else {
    ok = parse_number(r_value);
  }
  if (!ok) {
    return false;
  }
  /* Producto de enteros escrito a mano (`224 * 8`). Solo en el dialecto Python:
   * el formato nativo guarda el resultado, no la cuenta. */
  if (python) {
    while (true) {
      const size_t save = pos;
      skip_ws();
      if (pos < src.size() && src[pos] == '*') {
        pos++;
        Value rhs;
        if (!parse_value(rhs)) {
          return false;
        }
        if (r_value.kind == ValueKind::Int && rhs.kind == ValueKind::Int) {
          r_value = Value::make_int(r_value.i * rhs.i);
        }
        else {
          const double a = r_value.kind == ValueKind::Int ? double(r_value.i) : r_value.f;
          const double b = rhs.kind == ValueKind::Int ? double(rhs.i) : rhs.f;
          r_value = Value::make_float(a * b, false);
        }
        continue;
      }
      pos = save;
      break;
    }
  }
  return true;
}

/** Lee una ruta respetando corchetes y comillas: `foo["a b"].bar` es una sola. */
bool scan_path(const std::string &line, size_t &pos, std::string &r_path)
{
  while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
    pos++;
  }
  const size_t start = pos;
  int depth = 0;
  char quote = 0;
  while (pos < line.size()) {
    const char c = line[pos];
    if (quote) {
      if (c == '\\') {
        pos++;
      }
      else if (c == quote) {
        quote = 0;
      }
    }
    else if (c == '"' || c == '\'') {
      quote = c;
    }
    else if (c == '[') {
      depth++;
    }
    else if (c == ']') {
      depth--;
    }
    else if (depth == 0 && (c == ' ' || c == '\t')) {
      break;
    }
    pos++;
  }
  r_path = line.substr(start, pos - start);
  return !r_path.empty();
}

std::string trim(const std::string &s)
{
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) {
    a++;
  }
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) {
    b--;
  }
  return s.substr(a, b - a);
}

std::vector<std::string> split_lines(const std::string &text)
{
  std::vector<std::string> out;
  std::string cur;
  for (const char c : text) {
    if (c == '\n') {
      out.push_back(cur);
      cur.clear();
    }
    else {
      cur += c;
    }
  }
  out.push_back(cur);
  return out;
}

}  // namespace

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Lector del formato nativo
 * \{ */

bool read_fpreset_text(const std::string &text,
                       const char *origin,
                       Preset &r_preset,
                       std::string &r_error)
{
  r_preset = Preset();
  const std::vector<std::string> lines = split_lines(text);
  bool seen_magic = false;
  int depth = 0;
  int when_depth = 0;

  for (size_t ln = 0; ln < lines.size(); ln++) {
    const std::string line = trim(lines[ln]);
    const int lineno = int(ln) + 1;
    if (line.empty() || line[0] == '#') {
      continue;
    }
    size_t pos = 0;
    std::string word;
    scan_path(line, pos, word);

    auto err = [&](const std::string &msg) {
      char buf[64];
      std::snprintf(buf, sizeof(buf), ":%d: ", lineno);
      r_error = std::string(origin) + buf + msg;
      return false;
    };

    if (!seen_magic) {
      if (word != "fpreset") {
        return err("un preset nativo empieza por 'fpreset <version>'");
      }
      std::string ver;
      scan_path(line, pos, ver);
      r_preset.version = std::atoi(ver.c_str());
      if (r_preset.version < 1 || r_preset.version > FORMAT_VERSION) {
        return err("version de formato no soportada: " + ver);
      }
      seen_magic = true;
      continue;
    }

    if (word == "subdir") {
      scan_path(line, pos, r_preset.subdir);
      continue;
    }
    if (word == "set" || word == "add" || word == "clear") {
      Op op;
      op.line = lineno;
      if (!scan_path(line, pos, op.path)) {
        return err("falta la ruta despues de '" + word + "'");
      }
      if (op.path.compare(0, 8, "context.") != 0 && op.path[0] != '.') {
        return err("la ruta debe empezar por 'context.' o por '.': " + op.path);
      }
      if (op.path[0] == '.' && depth == 0) {
        return err("ruta relativa fuera de un bloque 'add': " + op.path);
      }
      if (word == "set") {
        op.kind = OpKind::Set;
        const std::string rest = line.substr(pos);
        Scanner sc(rest, false);
        if (!sc.parse_value(op.value)) {
          return err(sc.error);
        }
        if (!sc.at_end()) {
          return err("sobra texto despues del valor");
        }
      }
      else if (word == "add") {
        op.kind = OpKind::CollectionAdd;
        depth++;
      }
      else {
        op.kind = OpKind::Clear;
      }
      r_preset.ops.push_back(op);
      continue;
    }
    if (word == "when") {
      Op op;
      op.kind = OpKind::When;
      op.line = lineno;
      if (!scan_path(line, pos, op.path)) {
        return err("falta la ruta despues de 'when'");
      }
      if (op.path.compare(0, 8, "context.") != 0) {
        return err("la ruta de 'when' debe empezar por 'context.': " + op.path);
      }
      while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
        pos++;
      }
      if (line.compare(pos, 2, "==") == 0) {
        op.compare = CompareOp::Equal;
      }
      else if (line.compare(pos, 2, "!=") == 0) {
        op.compare = CompareOp::NotEqual;
      }
      else {
        return err("'when' solo admite '==' o '!='");
      }
      pos += 2;
      const std::string rest = line.substr(pos);
      Scanner sc(rest, false);
      if (!sc.parse_value(op.value)) {
        return err(sc.error);
      }
      if (!sc.at_end()) {
        return err("sobra texto despues del valor de 'when'");
      }
      when_depth++;
      r_preset.ops.push_back(op);
      continue;
    }
    if (word == "otherwise") {
      if (when_depth == 0) {
        return err("'otherwise' sin un 'when' abierto");
      }
      Op op;
      op.kind = OpKind::Otherwise;
      op.line = lineno;
      r_preset.ops.push_back(op);
      continue;
    }
    if (word == "endwhen") {
      if (when_depth == 0) {
        return err("'endwhen' sin un 'when' abierto");
      }
      when_depth--;
      Op op;
      op.kind = OpKind::WhenEnd;
      op.line = lineno;
      r_preset.ops.push_back(op);
      continue;
    }
    if (word == "end") {
      if (depth == 0) {
        return err("'end' sin un 'add' abierto");
      }
      depth--;
      Op op;
      op.kind = OpKind::CollectionEnd;
      op.line = lineno;
      r_preset.ops.push_back(op);
      continue;
    }
    return err("orden desconocida: '" + word + "'");
  }

  if (!seen_magic) {
    r_error = std::string(origin) + ": fichero vacio o sin cabecera 'fpreset'";
    return false;
  }
  if (depth != 0) {
    r_error = std::string(origin) + ": queda un bloque 'add' sin cerrar";
    return false;
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Lector nativo del Python heredado
 *
 * Compatibilidad: los presets que el usuario ya tiene guardados en su carpeta de
 * configuracion son `.py` que escribio el propio Blender. Se leen aqui SIN
 * interprete, aceptando exactamente ese subconjunto. Lo que no encaje se rechaza
 * con fichero, linea y motivo: preferimos un error visible a un preset aplicado
 * a medias.
 * \{ */

namespace {

struct LegacyParser {
  const char *origin;
  std::string error;
  /* Alias -> ruta canonica. Un alias que apunta a un bloque `add` abierto se
   * guarda como "@<profundidad>". */
  std::map<std::string, std::string> aliases;
  std::vector<std::string> open_blocks; /* Nombres de alias de bloques abiertos. */
  Preset *preset = nullptr;

  bool fail(int line, const std::string &msg)
  {
    char buf[64];
    std::snprintf(buf, sizeof(buf), ":%d: ", line);
    error = std::string(origin) + buf + msg;
    return false;
  }

  /** Corta el primer componente de una expresion de ruta. */
  static std::string head(const std::string &expr)
  {
    size_t i = 0;
    while (i < expr.size() && (std::isalnum(static_cast<unsigned char>(expr[i])) ||
                               expr[i] == '_'))
    {
      i++;
    }
    return expr.substr(0, i);
  }

  /** `bpy.context.x.y` o `<alias>.y` -> ruta canonica del formato. */
  bool expand(const std::string &expr, int line, std::string &r_path)
  {
    if (expr.compare(0, 12, "bpy.context.") == 0) {
      r_path = "context." + expr.substr(12);
      return true;
    }
    if (expr == "bpy.context") {
      r_path = "context";
      return true;
    }
    const std::string h = head(expr);
    if (h.empty()) {
      return fail(line, "no es una ruta sino una expresion: '" + expr + "'");
    }
    const auto it = aliases.find(h);
    if (it == aliases.end()) {
      return fail(line, "nombre desconocido en la ruta: '" + h + "'");
    }
    const std::string tail = expr.substr(h.size());
    if (it->second[0] == '@') {
      /* Referencia a un bloque `add`. Se cierra todo lo mas profundo. */
      const int want = std::atoi(it->second.c_str() + 1);
      while (int(open_blocks.size()) > want + 1) {
        close_block();
      }
      r_path = tail.empty() ? "." : tail;
      return true;
    }
    while (!open_blocks.empty()) {
      close_block();
    }
    r_path = it->second + tail;
    return true;
  }

  void close_block()
  {
    Op op;
    op.kind = OpKind::CollectionEnd;
    preset->ops.push_back(op);
    aliases.erase(open_blocks.back());
    open_blocks.pop_back();
  }

  void close_all()
  {
    while (!open_blocks.empty()) {
      close_block();
    }
  }
};

}  // namespace

bool read_legacy_python_text(const std::string &text,
                             const char *origin,
                             Preset &r_preset,
                             std::string &r_error)
{
  r_preset = Preset();
  LegacyParser p;
  p.origin = origin;
  p.preset = &r_preset;

  const std::vector<std::string> lines = split_lines(text);
  bool in_docstring = false;
  std::string doc_delim;

  for (size_t ln = 0; ln < lines.size(); ln++) {
    const std::string line = trim(lines[ln]);
    const int lineno = int(ln) + 1;

    if (in_docstring) {
      if (line.find(doc_delim) != std::string::npos) {
        in_docstring = false;
      }
      continue;
    }
    if (line.empty() || line[0] == '#') {
      continue;
    }
    if (line.compare(0, 3, "\"\"\"") == 0 || line.compare(0, 3, "'''") == 0) {
      doc_delim = line.substr(0, 3);
      /* Una sola linea si vuelve a cerrar. */
      if (line.size() < 6 || line.find(doc_delim, 3) == std::string::npos) {
        in_docstring = true;
      }
      continue;
    }
    if (line == "import bpy") {
      continue;
    }
    if (line.compare(0, 7, "import ") == 0 || line.compare(0, 5, "from ") == 0) {
      r_error = "";
      p.fail(lineno, "importa un modulo que no es bpy: no es un preset de datos");
      r_error = p.error;
      return false;
    }

    /* `<ruta>.clear()` */
    if (line.size() > 8 && line.compare(line.size() - 8, 8, ".clear()") == 0) {
      const std::string expr = line.substr(0, line.size() - 8);
      Op op;
      op.kind = OpKind::Clear;
      op.line = lineno;
      if (!p.expand(expr, lineno, op.path)) {
        r_error = p.error;
        return false;
      }
      r_preset.ops.push_back(op);
      continue;
    }

    /* Asignacion. */
    const size_t eq = line.find('=');
    if (eq == std::string::npos) {
      p.fail(lineno, "no es una asignacion: '" + line + "'");
      r_error = p.error;
      return false;
    }
    if (eq + 1 < line.size() && line[eq + 1] == '=') {
      p.fail(lineno, "comparacion, no asignacion: '" + line + "'");
      r_error = p.error;
      return false;
    }
    const std::string lhs = trim(line.substr(0, eq));
    const std::string rhs = trim(line.substr(eq + 1));
    if (lhs.empty() || rhs.empty()) {
      p.fail(lineno, "asignacion incompleta");
      r_error = p.error;
      return false;
    }

    const bool lhs_is_name = lhs.find_first_of(".[") == std::string::npos;

    /* `NAME = <ruta>.add()` */
    if (lhs_is_name && rhs.size() > 6 && rhs.compare(rhs.size() - 6, 6, ".add()") == 0) {
      const std::string owner = rhs.substr(0, rhs.size() - 6);
      Op op;
      op.kind = OpKind::CollectionAdd;
      op.line = lineno;
      if (!p.expand(owner, lineno, op.path)) {
        r_error = p.error;
        return false;
      }
      r_preset.ops.push_back(op);
      char buf[16];
      std::snprintf(buf, sizeof(buf), "@%d", int(p.open_blocks.size()));
      p.aliases[lhs] = buf;
      p.open_blocks.push_back(lhs);
      continue;
    }

    if (lhs_is_name) {
      /* Alias (`scene = bpy.context.scene`). Es un `preset_defines`. */
      std::string path;
      if (!p.expand(rhs, lineno, path)) {
        r_error = p.error;
        return false;
      }
      p.aliases[lhs] = path;
      continue;
    }

    Op op;
    op.kind = OpKind::Set;
    op.line = lineno;
    if (!p.expand(lhs, lineno, op.path)) {
      r_error = p.error;
      return false;
    }
    Scanner sc(rhs, true);
    if (!sc.parse_value(op.value)) {
      p.fail(lineno, sc.error.empty() ? "valor no reconocido: " + rhs : sc.error);
      r_error = p.error;
      return false;
    }
    if (!sc.at_end()) {
      p.fail(lineno, "el valor no es un literal simple: '" + rhs + "'");
      r_error = p.error;
      return false;
    }
    r_preset.ops.push_back(op);
  }

  if (in_docstring) {
    r_error = std::string(origin) + ": cadena de documentacion sin cerrar";
    return false;
  }
  p.close_all();
  return true;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Fichero
 * \{ */

static bool file_read_all(const std::string &filepath, std::string &r_text, std::string &r_error)
{
  std::ifstream in(filepath, std::ios::binary);
  if (!in) {
    r_error = "no se puede abrir: " + filepath;
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  r_text = ss.str();
  return true;
}

bool read_file(const std::string &filepath, Preset &r_preset, std::string &r_error)
{
  std::string text;
  if (!file_read_all(filepath, text, r_error)) {
    return false;
  }
  const size_t dot = filepath.find_last_of('.');
  const std::string ext = dot == std::string::npos ? "" : filepath.substr(dot);
  if (ext == ".py") {
    return read_legacy_python_text(text, filepath.c_str(), r_preset, r_error);
  }
  return read_fpreset_text(text, filepath.c_str(), r_preset, r_error);
}

bool write_file(const std::string &filepath, const Preset &preset, std::string &r_error)
{
  std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
  if (!out) {
    r_error = "no se puede escribir: " + filepath;
    return false;
  }
  out << "fpreset " << preset.version << "\n";
  if (!preset.subdir.empty()) {
    out << "subdir " << preset.subdir << "\n";
  }
  int depth = 0;
  for (const Op &op : preset.ops) {
    if (op.kind == OpKind::CollectionEnd) {
      depth--;
    }
    for (int i = 0; i < depth; i++) {
      out << "  ";
    }
    switch (op.kind) {
      case OpKind::Set:
        out << "set " << op.path << " " << format_value(op.value) << "\n";
        break;
      case OpKind::Clear:
        out << "clear " << op.path << "\n";
        break;
      case OpKind::CollectionAdd:
        out << "add " << op.path << "\n";
        depth++;
        break;
      case OpKind::CollectionEnd:
        out << "end\n";
        break;
    }
  }
  out.flush();
  if (!out) {
    r_error = "error al escribir: " + filepath;
    return false;
  }
  return true;
}

/** \} */

}  // namespace flipendo::preset
