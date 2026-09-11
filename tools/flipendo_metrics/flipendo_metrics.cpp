// flipendo_metrics — el estado de Flipendo, medido y generado desde el árbol.
//
// Por qué existe esta herramienta y por qué creció el 2026-09-11:
//
//   `politicas/METRICAS.md` publicó el 8 de septiembre una cifra de Python medida el
//   5. Entre medias habían caído 109.442 líneas que nadie anotó, y el backlog seguía
//   describiendo un `addons_core` que ya estaba podado. Un documento escrito a mano
//   envejece en horas cuando hay siete carriles tocando el árbol a la vez. La
//   solución no es escribir mejor: es GENERAR el estado, medido, con un comando.
//
// Qué mide (todo del árbol, nada copiado de ningún documento):
//   - composición por lenguaje, separando lo propio de los terceros vendorizados
//     (`extern/`, `lib/`) que marca `.gitattributes` como `linguist-vendored`;
//   - el Python por zonas, que es donde se ve lo que falta de verdad;
//   - qué ficheros COMPILA el binario y cuáles no, leyendo `build.ninja`: la
//     diferencia entre «queda trabajo» y «queda código muerto»;
//   - la evolución, sacada del historial de git y no de una tabla a mano;
//   - los puentes que quedan de C++ a Python (`BPY_run_*`, `PyImport_ImportModule`,
//     los `#ifdef WITH_PYTHON` que esconden capacidad);
//   - la batería de verificadores `--fl-check-*` / `--fl-selftest-*` del binario,
//     ejecutada de verdad, con sus cifras y separada en los que valen en
//     `--background` y los que necesitan modo gráfico.
//
// Doctrina (politicas/LENGUAJE-CPP.md): esto es C++ y solo C++. No hay una línea de
// Python ni de shell en el proceso: los subprocesos (`git`, el binario de Flipendo)
// se lanzan con `fork`+`execvp` y argv explícito, sin pasar por `/bin/sh`.
//
// Uso:
//   flipendo-metrics [raiz]                  tabla de composición (medida con git)
//   flipendo-metrics --estado [raiz]         informe completo -> politicas/ESTADO.md
//   flipendo-metrics --bateria [raiz]        solo los verificadores, por pantalla
//
//   --rev <commit>     medir a otro commit (por defecto HEAD)
//   --sin-bateria      generar el informe sin ejecutar los verificadores
//   --sin-grafico      no ejecutar el grupo que necesita modo gráfico
//   --disco            recorrer el disco en vez de git (método viejo, ver §aviso)
//   --build <dir>      árbol de compilación (por defecto <raiz>/../build)
//   --binario <ruta>   binario a verificar
//   --salida <ruta>    dónde escribir el informe
//
// AVISO sobre `--disco`: recorrer el disco ve ficheros sin versionar y no ve lo que
// otro carril acaba de borrar; con varios carriles vivos NO es reproducible ni por
// quien la hizo. Por eso el modo por defecto mide con git a un commit nombrado.
//
// No está en ningún CMakeLists.txt (herramienta suelta, no entra en el binario):
//   c++ -std=c++17 -O2 -o ~/Flipendo/bin/flipendo-metrics \
//       ~/Flipendo/dev/upbge/tools/flipendo_metrics/flipendo_metrics.cpp

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

/* -------------------------------------------------------------------------- */
/* Utilidades de cadena                                                        */

static std::string sfmt(const char *fmt, ...)
{
  char buf[4096];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  return std::string(buf);
}

/* Miles con punto, como se escriben los números en el resto de `politicas/`. */
static std::string mil(long long v)
{
  bool neg = v < 0;
  unsigned long long a = neg ? -(unsigned long long)v : (unsigned long long)v;
  std::string d = std::to_string(a), o;
  int c = 0;
  for (int i = (int)d.size() - 1; i >= 0; --i) {
    o.push_back(d[(size_t)i]);
    if (++c % 3 == 0 && i > 0) o.push_back('.');
  }
  if (neg) o.push_back('-');
  std::reverse(o.begin(), o.end());
  return o;
}

static std::string pct(long long part, long long total)
{
  if (total <= 0) return "0,0 %";
  std::string s = sfmt("%.1f %%", 100.0 * (double)part / (double)total);
  for (auto &c : s) if (c == '.') c = ',';
  return s;
}

static bool starts_with(const std::string &s, const std::string &t)
{
  return s.size() >= t.size() && s.compare(0, t.size(), t) == 0;
}
static bool ends_with(const std::string &s, const std::string &t)
{
  return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
}
static bool contains(const std::string &s, const std::string &t)
{
  return s.find(t) != std::string::npos;
}

static std::string trim(const std::string &s)
{
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

static std::vector<std::string> split_ws(const std::string &s)
{
  std::vector<std::string> out;
  size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && isspace((unsigned char)s[i])) ++i;
    size_t a = i;
    while (i < s.size() && !isspace((unsigned char)s[i])) ++i;
    if (i > a) out.push_back(s.substr(a, i - a));
  }
  return out;
}

static std::vector<std::string> split_lines(const std::string &s)
{
  std::vector<std::string> out;
  size_t a = 0;
  while (a <= s.size()) {
    size_t b = s.find('\n', a);
    if (b == std::string::npos) { if (a < s.size()) out.push_back(s.substr(a)); break; }
    out.push_back(s.substr(a, b - a));
    a = b + 1;
  }
  return out;
}

/* Cuenta apariciones de `pat` en un bloque de bytes. */
static long long count_occ(const char *data, size_t n, const std::string &pat)
{
  if (pat.empty() || n < pat.size()) return 0;
  long long c = 0;
  const char *end = data + n;
  const char *p = data;
  while (true) {
    const char *h = (const char *)memmem(p, (size_t)(end - p), pat.data(), pat.size());
    if (!h) break;
    ++c;
    p = h + 1;
    if (p >= end) break;
  }
  return c;
}

/* -------------------------------------------------------------------------- */
/* Lanzar subprocesos SIN shell                                                */
/*                                                                             */
/* `fork` + `execvp` con argv explícito. No se construye ninguna línea de       */
/* comando ni interviene `/bin/sh`: la doctrina prohíbe shell nuevo, y además   */
/* así no hay que preocuparse de comillas ni de rutas con espacios.             */

struct ProcResult {
  int rc = -1;
  bool timed_out = false;
  bool launched = false;
  std::string out;   /* stdout (+ stderr si merge_stderr) */
  double secs = 0.0;
};

static double now_secs()
{
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static ProcResult run_process(const std::vector<std::string> &argv,
                              const std::string &cwd,
                              const std::string &stdin_data,
                              int timeout_s,
                              bool merge_stderr = true,
                              const std::vector<std::string> *env = nullptr)
{
  ProcResult r;
  if (argv.empty()) return r;
  int inp[2], outp[2];
  if (pipe(inp) != 0) return r;
  if (pipe(outp) != 0) { close(inp[0]); close(inp[1]); return r; }

  const double t0 = now_secs();
  pid_t pid = fork();
  if (pid < 0) { close(inp[0]); close(inp[1]); close(outp[0]); close(outp[1]); return r; }

  if (pid == 0) {
    /* Hijo: grupo propio para poder matar al árbol entero si se pasa de tiempo. */
    setsid();
    if (env) {
      for (const std::string &kv : *env) {
        size_t eq = kv.find('=');
        if (eq == std::string::npos) continue;
        setenv(kv.substr(0, eq).c_str(), kv.c_str() + eq + 1, 1);
      }
    }
    if (!cwd.empty()) { if (chdir(cwd.c_str()) != 0) _exit(127); }
    dup2(inp[0], STDIN_FILENO);
    dup2(outp[1], STDOUT_FILENO);
    if (merge_stderr) {
      dup2(outp[1], STDERR_FILENO);
    }
    else {
      int devnull = open("/dev/null", O_WRONLY);
      if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
    }
    close(inp[0]); close(inp[1]); close(outp[0]); close(outp[1]);
    std::vector<char *> cargv;
    cargv.reserve(argv.size() + 1);
    for (const auto &s : argv) cargv.push_back(const_cast<char *>(s.c_str()));
    cargv.push_back(nullptr);
    execvp(cargv[0], cargv.data());
    _exit(127);
  }

  r.launched = true;
  close(inp[0]);
  close(outp[1]);

  size_t written = 0;
  bool stdin_open = true;
  if (stdin_data.empty()) { close(inp[1]); stdin_open = false; }

  const double deadline = timeout_s > 0 ? t0 + (double)timeout_s : 0.0;
  bool out_open = true;
  std::string buf;
  buf.reserve(1 << 20);
  char chunk[1 << 16];

  while (out_open) {
    struct pollfd pf[2];
    int nf = 0;
    int i_out = -1, i_in = -1;
    if (out_open) { pf[nf].fd = outp[0]; pf[nf].events = POLLIN; i_out = nf++; }
    if (stdin_open) { pf[nf].fd = inp[1]; pf[nf].events = POLLOUT; i_in = nf++; }
    int wait_ms = 1000;
    if (deadline > 0.0) {
      double left = deadline - now_secs();
      if (left <= 0) {
        kill(-pid, SIGKILL);
        r.timed_out = true;
        break;
      }
      if (left * 1000.0 < wait_ms) wait_ms = (int)(left * 1000.0) + 1;
    }
    int n = poll(pf, (nfds_t)nf, wait_ms);
    if (n < 0) { if (errno == EINTR) continue; break; }
    if (n == 0) continue;
    if (i_in >= 0 && (pf[i_in].revents & (POLLOUT | POLLERR | POLLHUP))) {
      size_t left = stdin_data.size() - written;
      ssize_t w = left ? write(inp[1], stdin_data.data() + written, left > (1u << 16) ? (1u << 16) : left) : 0;
      if (w > 0) written += (size_t)w;
      if (w <= 0 || written >= stdin_data.size()) { close(inp[1]); stdin_open = false; }
    }
    if (i_out >= 0 && (pf[i_out].revents & (POLLIN | POLLERR | POLLHUP))) {
      ssize_t rd = read(outp[0], chunk, sizeof(chunk));
      if (rd > 0) buf.append(chunk, (size_t)rd);
      else out_open = false;
    }
  }
  if (stdin_open) close(inp[1]);
  close(outp[0]);

  int status = 0;
  if (r.timed_out) {
    kill(-pid, SIGKILL);
  }
  waitpid(pid, &status, 0);
  r.rc = WIFEXITED(status) ? WEXITSTATUS(status) : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1);
  r.out = std::move(buf);
  r.secs = now_secs() - t0;
  return r;
}

/* -------------------------------------------------------------------------- */
/* git                                                                         */

struct Git {
  std::string root;

  ProcResult run(const std::vector<std::string> &args,
                 const std::string &in = "",
                 bool merge_stderr = true,
                 int timeout_s = 900) const
  {
    std::vector<std::string> argv = {"git", "-C", root};
    argv.insert(argv.end(), args.begin(), args.end());
    return run_process(argv, root, in, timeout_s, merge_stderr);
  }

  std::string out(const std::vector<std::string> &args) const
  {
    ProcResult r = run(args, "", false);
    return r.rc == 0 ? r.out : std::string();
  }

  std::string line(const std::vector<std::string> &args) const { return trim(out(args)); }

  std::string show(const std::string &rev, const std::string &path) const
  {
    return out({"show", rev + ":" + path});
  }
};

/* -------------------------------------------------------------------------- */
/* Clasificación por extensión                                                 */

struct Lang {
  const char *ext;
  const char *name;
};

static const Lang kLangs[] = {
    {".c", "C"},           {".cc", "C++"},        {".cpp", "C++"},       {".cxx", "C++"},
    {".h", ".h heredada"}, {".hh", ".hh"},        {".hpp", ".hpp"},      {".hxx", ".hpp"},
    {".py", "Python"},     {".mm", "Objective-C++"}, {".m", "Objective-C"},
    {".glsl", "GLSL"},     {".msl", "MSL"},       {".metal", "Metal"},
    {".sh", "shell"},      {".bash", "shell"},    {".zsh", "shell"},
};

static const char *lang_of(const std::string &path)
{
  size_t dot = path.find_last_of('.');
  if (dot == std::string::npos) return nullptr;
  std::string e = path.substr(dot);
  for (auto &c : e) c = (char)tolower((unsigned char)c);
  for (const Lang &l : kLangs) {
    if (e == l.ext) return l.name;
  }
  return nullptr;
}

/* Lenguajes que el compilador convierte en objetos (`.o`). Para el resto,
 * «compilado» no significa nada y preguntarlo da una respuesta falsa. */
static bool lang_makes_objects(const std::string &lang)
{
  return lang == "C++" || lang == "C" || lang == "Objective-C++" || lang == "Objective-C";
}

/* Orden de presentación: primero el estándar del árbol, luego la deuda. */
static int lang_rank(const std::string &l)
{
  static const std::vector<std::string> order = {
      "C++", ".hh", ".hpp", ".h heredada", "Python", "GLSL", "Objective-C++",
      "Objective-C", "MSL", "Metal", "C", "shell"};
  for (size_t i = 0; i < order.size(); ++i) if (order[i] == l) return (int)i;
  return 99;
}

/* -------------------------------------------------------------------------- */
/* Instantánea del árbol a un commit                                           */

struct FileRec {
  std::string path;
  std::string lang;
  long long lines = 0;
  bool vendored = false;
};

struct Snapshot {
  std::string rev;        /* hash completo */
  std::string shortrev;   /* hash corto */
  std::string date;       /* fecha del commit, ISO local */
  std::string subject;
  std::vector<FileRec> files;
  long long tracked_total = 0;   /* ficheros versionados en total */
  std::vector<std::string> vendored_prefixes;
};

/* Prefijos vendorizados leídos de `.gitattributes` (no escritos a mano aquí):
 * cualquier patron marcado `linguist-vendored`. Hoy son los de `extern` y `lib`,
 * que es justo la excepción doctrinal de LENGUAJE-CPP.md. */
static std::vector<std::string> vendored_prefixes(const Git &git, const std::string &rev)
{
  std::vector<std::string> out;
  std::string ga = git.show(rev, ".gitattributes");
  for (const std::string &ln : split_lines(ga)) {
    std::string t = trim(ln);
    if (t.empty() || t[0] == '#') continue;
    if (!contains(t, "linguist-vendored")) continue;
    std::vector<std::string> tok = split_ws(t);
    if (tok.empty()) continue;
    std::string pat = tok[0];
    while (!pat.empty() && (pat.back() == '*' || pat.back() == '/')) pat.pop_back();
    if (!pat.empty()) out.push_back(pat + "/");
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

using ScanFn = void (*)(const std::string &path, bool vendored, const char *data, size_t n, void *ud);

/* Lee el árbol a `rev` con `git ls-tree -r -z` + un solo `git cat-file --batch`.
 * Cuenta líneas como `wc -l` (número de '\n'), que es lo que fija el §1 de
 * METRICAS.md; la versión anterior de esta herramienta sumaba una línea de más
 * por fichero no vacío. */
static Snapshot take_snapshot(const Git &git,
                              const std::string &rev,
                              ScanFn scan = nullptr,
                              void *scan_ud = nullptr,
                              const std::vector<std::string> *force_prefixes = nullptr)
{
  Snapshot s;
  s.rev = git.line({"rev-parse", rev});
  s.shortrev = git.line({"rev-parse", "--short", rev});
  s.date = git.line({"log", "-1", "--format=%cd", "--date=format:%Y-%m-%d %H:%M", rev});
  s.subject = git.line({"log", "-1", "--format=%s", rev});
  /* Para las series históricas se imponen los prefijos del commit de referencia: el
   * `.gitattributes` que marca `extern` y `lib` como vendorizados se añadió a media
   * historia, y sin imponerlos las filas viejas contarían las bibliotecas ajenas como
   * código de Flipendo y la serie no sería comparable consigo misma. */
  s.vendored_prefixes = force_prefixes ? *force_prefixes : vendored_prefixes(git, s.rev);

  ProcResult ls = git.run({"ls-tree", "-r", "-z", s.rev}, "", false);
  if (ls.rc != 0) return s;

  std::vector<std::string> hashes;
  std::vector<FileRec> recs;
  size_t a = 0;
  const std::string &b = ls.out;
  while (a < b.size()) {
    size_t z = b.find('\0', a);
    if (z == std::string::npos) z = b.size();
    std::string entry = b.substr(a, z - a);
    a = z + 1;
    size_t tab = entry.find('\t');
    if (tab == std::string::npos) continue;
    std::vector<std::string> head = split_ws(entry.substr(0, tab));
    if (head.size() < 3 || head[1] != "blob") continue;
    s.tracked_total++;
    std::string path = entry.substr(tab + 1);
    const char *lg = lang_of(path);
    if (!lg) continue;
    FileRec fr;
    fr.path = path;
    fr.lang = lg;
    for (const std::string &p : s.vendored_prefixes) {
      if (starts_with(path, p)) { fr.vendored = true; break; }
    }
    recs.push_back(fr);
    hashes.push_back(head[2]);
  }

  std::string stdin_data;
  stdin_data.reserve(hashes.size() * 41);
  for (const std::string &h : hashes) { stdin_data += h; stdin_data.push_back('\n'); }

  ProcResult cf = git.run({"cat-file", "--batch"}, stdin_data, /*merge_stderr=*/false);
  const std::string &o = cf.out;
  size_t p = 0, idx = 0;
  while (p < o.size() && idx < recs.size()) {
    size_t nl = o.find('\n', p);
    if (nl == std::string::npos) break;
    std::string hdr = o.substr(p, nl - p);
    p = nl + 1;
    std::vector<std::string> ht = split_ws(hdr);
    if (ht.size() < 3) { ++idx; continue; }   /* «missing» y demás */
    size_t size = (size_t)strtoull(ht[2].c_str(), nullptr, 10);
    if (p + size > o.size()) break;
    const char *data = o.data() + p;
    long long lines = 0;
    for (size_t i = 0; i < size; ++i) if (data[i] == '\n') ++lines;
    recs[idx].lines = lines;
    if (scan) scan(recs[idx].path, recs[idx].vendored, data, size, scan_ud);
    p += size + 1;   /* el '\n' que cat-file añade detrás del contenido */
    ++idx;
  }
  s.files = std::move(recs);
  return s;
}

struct LangTotals {
  std::map<std::string, std::pair<long long, long long>> own, vend;   /* lenguaje -> {ficheros, líneas} */
  long long own_files = 0, own_lines = 0, vend_files = 0, vend_lines = 0;
};

static LangTotals totals_of(const Snapshot &s)
{
  LangTotals t;
  for (const FileRec &f : s.files) {
    auto &m = f.vendored ? t.vend : t.own;
    auto &slot = m[f.lang];
    slot.first += 1;
    slot.second += f.lines;
    if (f.vendored) { t.vend_files++; t.vend_lines += f.lines; }
    else { t.own_files++; t.own_lines += f.lines; }
  }
  return t;
}

static long long lines_of(const LangTotals &t, const char *lang, bool own = true)
{
  const auto &m = own ? t.own : t.vend;
  auto it = m.find(lang);
  return it == m.end() ? 0 : it->second.second;
}
static long long files_of(const LangTotals &t, const char *lang, bool own = true)
{
  const auto &m = own ? t.own : t.vend;
  auto it = m.find(lang);
  return it == m.end() ? 0 : it->second.first;
}

/* -------------------------------------------------------------------------- */
/* Lo que el binario COMPILA de verdad: `build.ninja` + `CMakeCache.txt`        */

struct BuildFacts {
  bool have_ninja = false;
  bool have_cache = false;
  std::string ninja_path, cache_path;
  std::set<std::string> objects;      /* rutas absolutas que son entrada de una arista `.o` */
  std::set<std::string> mentioned;    /* rutas absolutas citadas en cualquier sitio del fichero */
  std::map<std::string, std::string> cache;
  long long object_edges = 0;
};

static bool looks_like_source(const std::string &p)
{
  return lang_of(p) != nullptr;
}

static BuildFacts read_build(const std::string &build_dir, const std::string &root_abs)
{
  BuildFacts b;
  b.ninja_path = build_dir + "/build.ninja";
  b.cache_path = build_dir + "/CMakeCache.txt";

  std::ifstream cf(b.cache_path);
  if (cf) {
    b.have_cache = true;
    std::string ln;
    while (std::getline(cf, ln)) {
      if (ln.empty() || ln[0] == '#' || ln[0] == '/') continue;
      size_t eq = ln.find('=');
      size_t colon = ln.find(':');
      if (eq == std::string::npos || colon == std::string::npos || colon > eq) continue;
      b.cache[ln.substr(0, colon)] = ln.substr(eq + 1);
    }
  }

  std::ifstream nf(b.ninja_path);
  if (!nf) return b;
  b.have_ninja = true;
  const std::string pref = root_abs + "/";
  std::string ln;
  while (std::getline(nf, ln)) {
    /* Cualquier mención a una ruta del árbol, venga de donde venga. Sirve para
     * decir «citado» sin confundirlo con «compilado»: un `-I<dir>` cita el
     * directorio pero no compila nada de dentro. */
    size_t pos = 0;
    while ((pos = ln.find(pref, pos)) != std::string::npos) {
      size_t e = pos;
      while (e < ln.size() && !isspace((unsigned char)ln[e]) && ln[e] != ':' && ln[e] != '"') ++e;
      std::string tok = ln.substr(pos, e - pos);
      if (looks_like_source(tok)) b.mentioned.insert(tok);
      pos = e;
    }
    if (!starts_with(ln, "build ")) continue;
    size_t colon = ln.find(": ");
    if (colon == std::string::npos) continue;
    std::string outs = ln.substr(6, colon - 6);
    if (!contains(outs, ".o")) continue;
    b.object_edges++;
    std::vector<std::string> tok = split_ws(ln.substr(colon + 2));
    for (size_t i = 1; i < tok.size(); ++i) {   /* [0] es el nombre de la regla */
      if (tok[i] == "||" || tok[i] == "|") break;
      if (tok[i][0] == '/' && looks_like_source(tok[i])) b.objects.insert(tok[i]);
    }
  }
  return b;
}

/* -------------------------------------------------------------------------- */
/* Zonas de Python                                                             */

/* La zona es el directorio que da sentido a la pieza, no una lista escrita a
 * mano: `scripts/startup/<zona>` a tres niveles (bl_ui y bl_operators son
 * frentes distintos) y dos niveles en el resto. Si una zona desaparece del
 * árbol, desaparece sola de la tabla. */
static std::string py_zone(const std::string &path)
{
  std::vector<std::string> parts;
  size_t a = 0;
  while (a <= path.size()) {
    size_t b = path.find('/', a);
    if (b == std::string::npos) { parts.push_back(path.substr(a)); break; }
    parts.push_back(path.substr(a, b - a));
    a = b + 1;
  }
  if (parts.size() >= 3 && parts[0] == "scripts" && parts[1] == "startup")
    return parts[0] + "/" + parts[1] + "/" + parts[2];
  if (parts.size() >= 2) return parts[0] + "/" + parts[1];
  return parts.empty() ? path : parts[0];
}

/* -------------------------------------------------------------------------- */
/* Puentes de C++ a Python                                                     */

struct BridgePattern {
  const char *pat;
  const char *what;
};

static const BridgePattern kBridges[] = {
    {"BPY_run_", "ejecutar código Python desde C++ (`BPY_run_*`)"},
    {"PyImport_ImportModule", "importar un módulo de Python desde C++"},
    {"PyRun_", "la API cruda de CPython (`PyRun_*`)"},
    {"Py_Initialize", "arrancar el intérprete"},
    {"BPY_python_start", "arrancar el intérprete (envoltorio de Blender)"},
};

struct BridgeScan {
  std::map<std::string, long long> occ;                      /* patrón -> ocurrencias */
  std::map<std::string, std::set<std::string>> pat_files;    /* patrón -> ficheros */
  std::map<std::string, long long> file_calls;               /* fichero -> llamadas reales */
  long long guard_lines = 0;                                 /* líneas `#if*` con WITH_PYTHON */
  std::set<std::string> guard_files;
  long long with_python_occ = 0;
};

/* El propio generador lleva los nombres de los puentes escritos como datos (la
 * tabla de arriba), así que si se escanea a sí mismo se cuenta nueve puentes que
 * no existen. Medido: 120 en vez de 111. Se excluye por su ruta. */
static bool is_this_generator(const std::string &path)
{
  return contains(path, "tools/flipendo_metrics/");
}

static void bridge_scan(const std::string &path, bool vendored, const char *data, size_t n, void *ud)
{
  if (vendored || is_this_generator(path)) return;
  const char *lg = lang_of(path);
  if (!lg) return;
  const std::string l = lg;
  if (l == "Python" || l == "GLSL" || l == "MSL" || l == "Metal" || l == "shell") return;

  BridgeScan &bs = *(BridgeScan *)ud;
  for (const BridgePattern &bp : kBridges) {
    long long c = count_occ(data, n, bp.pat);
    if (c) {
      bs.occ[bp.pat] += c;
      bs.pat_files[bp.pat].insert(path);
      bs.file_calls[path] += c;
    }
  }
  long long wp = count_occ(data, n, "WITH_PYTHON");
  if (!wp) return;
  bs.with_python_occ += wp;

  /* `#ifdef WITH_PYTHON` (con las variantes indentadas del preprocesador) es la
   * marca de «aquí hay capacidad que solo existe si se compila con Python». */
  size_t start = 0;
  while (start < n) {
    const char *nlp = (const char *)memchr(data + start, '\n', n - start);
    size_t end = nlp ? (size_t)(nlp - data) : n;
    std::string ln(data + start, end - start);
    start = end + 1;
    if (!contains(ln, "WITH_PYTHON")) continue;
    std::string t = trim(ln);
    if (t.empty() || t[0] != '#') continue;
    std::string rest = trim(t.substr(1));
    if (starts_with(rest, "if") || starts_with(rest, "elif")) {
      bs.guard_lines++;
      bs.guard_files.insert(path);
    }
  }
}

/* -------------------------------------------------------------------------- */
/* Evolución sacada del historial                                              */

struct HistPoint {
  std::string rev, shortrev, when;
  long long py_files = 0, py_lines = 0;
  long long cpp = 0, mm = 0, glsl = 0, h = 0, hpp = 0, c = 0;
  long long own_lines = 0;
};

struct Commit {
  std::string hash;
  long long ts = 0;
  std::string day;    /* YYYY-MM-DD */
  std::string hour;   /* YYYY-MM-DD HH */
};

static std::vector<Commit> commit_list(const Git &git, const std::string &rev)
{
  std::vector<Commit> out;
  std::string s = git.out({"log", "--first-parent", "--format=%H %ct %cd",
                           "--date=format:%Y-%m-%d %H", rev});
  for (const std::string &ln : split_lines(s)) {
    std::vector<std::string> t = split_ws(ln);
    if (t.size() < 4) continue;
    Commit c;
    c.hash = t[0];
    c.ts = strtoll(t[1].c_str(), nullptr, 10);
    c.day = t[2];
    c.hour = t[2] + " " + t[3];
    out.push_back(c);
  }
  return out;   /* del más nuevo al más viejo */
}

static HistPoint measure_point(const Git &git, const std::string &rev,
                               const std::vector<std::string> &prefixes)
{
  Snapshot s = take_snapshot(git, rev, nullptr, nullptr, &prefixes);
  LangTotals t = totals_of(s);
  HistPoint h;
  h.rev = s.rev;
  h.shortrev = s.shortrev;
  h.when = s.date;
  h.py_files = files_of(t, "Python");
  h.py_lines = lines_of(t, "Python");
  h.cpp = lines_of(t, "C++");
  h.mm = lines_of(t, "Objective-C++");
  h.glsl = lines_of(t, "GLSL");
  h.h = lines_of(t, ".h heredada");
  h.hpp = lines_of(t, ".hpp");
  h.c = lines_of(t, "C");
  h.own_lines = t.own_lines;
  return h;
}

/* -------------------------------------------------------------------------- */
/* Leer un fichero entero                                                      */

static bool read_file(const std::string &p, std::string &out)
{
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

/* -------------------------------------------------------------------------- */
/* Declaraciones que acompañan a una línea base                                */
/*                                                                             */
/* El arnés (carril ARNES, 2026-09-11) admite dos ficheros junto a cada línea   */
/* base, y esta herramienta TIENE QUE HONRARLOS o publicará rojos falsos:       */
/*                                                                             */
/*   <linea-base>.tolerancia    `tolerancia-relativa <numero>`                  */
/*   <linea-base>.divergencias  bloques `linea N` / `python:` / `cpp:` / `razon:`*/
/*                                                                             */
/* La semántica se copia de `source/blender/editors/include/FL_selftest_compare.hh`,
 * que es quien la implementa dentro del binario: misma normalización           */
/* (`max(1,|a|,|b|)`), mismo troceado por palabras, mismas tres salidas de una  */
/* divergencia (cumplida / regresión al valor del Python / cualquier otra cosa).*/
/* Si las dos implementaciones se separaran, el informe mentiría; por eso el    */
/* generador compara además su veredicto con el del comprobador hermano cuando  */
/* los dos existen.                                                             */

struct Divergence {
  int line_no = 0;
  std::string python, cpp, razon;
  bool honoured = false;
};

struct Declarations {
  double tolerance = 0.0;
  std::string tol_path, div_path;
  std::vector<Divergence> divergences;
  bool any() const { return tolerance > 0.0 || !divergences.empty(); }
};

static std::string chomp(const std::string &s)
{
  std::string r = s;
  while (!r.empty() && (r.back() == '\n' || r.back() == '\r')) r.pop_back();
  return r;
}

static Declarations read_declarations(const std::string &baseline_abs)
{
  Declarations d;
  std::string t;
  if (read_file(baseline_abs + ".tolerancia", t)) {
    d.tol_path = baseline_abs + ".tolerancia";
    for (const std::string &ln : split_lines(t)) {
      if (ln.empty() || ln[0] == '#') continue;
      double v = 0.0;
      if (sscanf(ln.c_str(), "tolerancia-relativa %lf", &v) == 1) d.tolerance = v;
    }
  }
  std::string g;
  if (read_file(baseline_abs + ".divergencias", g)) {
    d.div_path = baseline_abs + ".divergencias";
    for (const std::string &raw : split_lines(g)) {
      const std::string ln = chomp(raw);
      if (ln.empty() || ln[0] == '#') continue;
      int n = 0;
      if (sscanf(ln.c_str(), "linea %d", &n) == 1) {
        Divergence dv;
        dv.line_no = n;
        d.divergences.push_back(dv);
        continue;
      }
      if (d.divergences.empty()) continue;
      size_t colon = ln.find(':');
      if (colon == std::string::npos) continue;
      /* El valor va detrás del primer `:` SIN recortar: los volcados empiezan por
       * espacios y recortarlos rompería la comparación. */
      const std::string key = ln.substr(0, colon), val = ln.substr(colon + 1);
      if (key == "python") d.divergences.back().python = val;
      else if (key == "cpp") d.divergences.back().cpp = val;
      else if (key == "razon") d.divergences.back().razon = val;
    }
  }
  return d;
}

/* Igualdad palabra a palabra con tolerancia RELATIVA en los números, normalizando
 * por `max(1,|a|,|b|)` igual que `FL_selftest_compare.hh`. */
static bool equal_within(const std::string &a, const std::string &b, double tol, double &worst)
{
  std::vector<std::string> ta = split_ws(a), tb = split_ws(b);
  if (ta.size() != tb.size()) return false;
  worst = 0.0;
  for (size_t i = 0; i < ta.size(); ++i) {
    if (ta[i] == tb[i]) continue;
    char *ea = nullptr, *eb = nullptr;
    double va = strtod(ta[i].c_str(), &ea), vb = strtod(tb[i].c_str(), &eb);
    if (ea == ta[i].c_str() || *ea != '\0' || eb == tb[i].c_str() || *eb != '\0') return false;
    if (!std::isfinite(va) || !std::isfinite(vb)) return false;
    const double scale = std::fmax(1.0, std::fmax(std::fabs(va), std::fabs(vb)));
    const double rel = std::fabs(va - vb) / scale;
    if (rel > tol) return false;
    worst = std::fmax(worst, rel);
  }
  return true;
}

struct CompareResult {
  bool ok = false;
  long long total_lines = 0, same_lines = 0, tolerated = 0;
  long long div_total = 0, div_honoured = 0, div_broken = 0;
  double worst_tolerated = 0.0;
  std::string why;        /* primera diferencia, o el motivo del fallo */
  std::string regression; /* si una divergencia ha vuelto al valor del Python */
};

/* Compara el volcado con su línea base honrando las declaraciones. Espeja a
 * `flipendo::selftest::compare_to_baseline`, incluida su regla mas importante:
 * comparar CERO líneas no es un aprobado. */
static CompareResult compare_to_baseline(const std::string &actual_abs,
                                         const std::string &baseline_abs,
                                         const Declarations &decl)
{
  CompareResult r;
  std::string xa, xb;
  if (!read_file(actual_abs, xa)) { r.why = "no se pudo leer el volcado"; return r; }
  if (!read_file(baseline_abs, xb)) { r.why = "no existe la linea base"; return r; }

  std::vector<Divergence> divs = decl.divergences;
  r.div_total = (long long)divs.size();
  std::vector<std::string> la = split_lines(xa), lb = split_lines(xb);
  const size_t n = std::max(la.size(), lb.size());
  for (size_t i = 0; i < n; ++i) {
    const int line_no = (int)i + 1;
    const bool has_a = i < la.size(), has_b = i < lb.size();
    const std::string sa = has_a ? la[i] : "(falta linea en el volcado)";
    const std::string sb = has_b ? lb[i] : "(falta linea en la linea base)";
    r.total_lines++;

    Divergence *declared = nullptr;
    for (Divergence &d : divs) if (d.line_no == line_no) { declared = &d; break; }

    bool accepted = false;
    if (declared) {
      if (sa == declared->cpp) {
        declared->honoured = true;
        accepted = true;
      }
      else if (sa == declared->python) {
        r.div_broken++;
        r.regression = sfmt("linea %d: REGRESION, ha vuelto al valor del Python que el C++ "
                            "corregia a proposito (esperado `%s`, obtenido `%s`)",
                            line_no, declared->cpp.c_str(), sa.c_str());
      }
      else {
        r.div_broken++;
        r.regression = sfmt("linea %d: declarada como divergencia pero no da ni el valor del "
                            "Python ni el declarado del C++ (esperado `%s`, obtenido `%s`)",
                            line_no, declared->cpp.c_str(), sa.c_str());
      }
    }
    else if (has_a && has_b && sa == sb) {
      accepted = true;
    }
    else if (has_a && has_b && decl.tolerance > 0.0) {
      double worst = 0.0;
      if (equal_within(sb, sa, decl.tolerance, worst)) {
        accepted = true;
        r.tolerated++;
        r.worst_tolerated = std::fmax(r.worst_tolerated, worst);
      }
    }

    if (accepted) r.same_lines++;
    else if (!declared && r.why.empty()) {
      std::string a1 = sa, b1 = sb;
      if (a1.size() > 90) a1 = a1.substr(0, 90) + "...";
      if (b1.size() > 90) b1 = b1.substr(0, 90) + "...";
      r.why = sfmt("linea %d: obtenido `%s` / linea base `%s`", line_no, a1.c_str(), b1.c_str());
    }
  }
  for (const Divergence &d : divs) {
    if (d.honoured) { r.div_honoured++; continue; }
    if (d.line_no > r.total_lines) {
      r.div_broken++;
      if (r.regression.empty())
        r.regression = sfmt("la divergencia declarada para la linea %d no se pudo comprobar: "
                            "el volcado solo llega a %lld lineas",
                            d.line_no, r.total_lines);
    }
  }
  if (r.total_lines == 0) {
    r.why = "no se comparo ni una linea: el volcado y la linea base estan los dos vacios, "
            "asi que no se ha verificado nada";
    return r;
  }
  r.ok = (r.same_lines == r.total_lines) && (r.div_broken == 0);
  if (!r.ok && r.why.empty() && !r.regression.empty()) r.why = r.regression;
  return r;
}

/* -------------------------------------------------------------------------- */
/* La batería de verificadores del binario                                     */

struct Verifier {
  std::string flag;        /* --fl-check-keymap */
  std::string label;       /* lo que se imprime (puede llevar variante) */
  std::string kind;        /* "check" | "selftest" | "volcador" */
  std::string doc;         /* doc string tal cual está en creator_args.cc */
  std::vector<std::string> args;
  std::vector<std::string> baselines;   /* para los selftest: con qué comparar el volcado */
  std::vector<std::string> env;         /* NOMBRE=valor para el hijo */
  std::string out_file;                 /* volcado del selftest, si lo hay */
  bool gui = false;
  std::string gui_reason;
  std::string doc_baseline_missing;     /* la ayuda declara una línea base que no existe */

  /* resultado */
  bool ran = false;
  int rc = -1;
  bool timed_out = false;
  double secs = 0;
  long long passes = 0;
  std::string mode;       /* "background" | "grafico" */
  std::string verdict;    /* VERDE | ROJO | INESTABLE | SIN LINEA BASE | VOLCADOR | NO EJECUTADO */
  std::string detail;
  std::string baseline_used;

  /* declaraciones honradas: qué había declarado y qué hizo falta de verdad */
  Declarations decl;
  long long tol_passes_used = 0;   /* pasadas en las que la tolerancia hizo falta */
  long long div_honoured = 0;
  long long div_passes_used = 0;   /* pasadas en las que hizo falta alguna divergencia */
  double worst_tolerated = 0.0;
};

/* Extrae el doc string de un argumento: `arg_handle_fl_<x>_doc[] = "..." "...";` */
static std::string doc_of(const std::string &src, const std::string &flag)
{
  std::string name = flag.substr(2);                 /* fl-check-keymap */
  for (auto &c : name) if (c == '-') c = '_';        /* fl_check_keymap */
  std::string needle = "arg_handle_" + name + "_doc[]";
  size_t p = src.find(needle);
  if (p == std::string::npos) return "";
  size_t eq = src.find('=', p);
  if (eq == std::string::npos) return "";
  size_t end = src.find(';', eq);
  if (end == std::string::npos) return "";
  std::string body = src.substr(eq + 1, end - eq - 1);
  std::string out;
  bool in_str = false;
  for (size_t i = 0; i < body.size(); ++i) {
    char c = body[i];
    if (!in_str) { if (c == '"') in_str = true; continue; }
    if (c == '"') { in_str = false; continue; }
    if (c == '\\' && i + 1 < body.size()) {
      char n = body[++i];
      if (n == 'n') out.push_back('\n');
      else if (n == 't') out.push_back('\t');
      else out.push_back(n);
      continue;
    }
    out.push_back(c);
  }
  return out;
}

/* Receta para los verificadores cuyo doc string no basta para saber cómo se
 * invocan. Cada una está MEDIDA ejecutándola el 2026-09-11 contra el binario del
 * árbol, no deducida: el comentario dice qué se observó. `@out` es un fichero
 * temporal que escribe esta herramienta. */
struct Recipe {
  const char *flag;
  const char *args;        /* separados por '|', "@out" = temporal */
  const char *baselines;   /* separados por '|', el primero es el preferente */
  int gui;                 /* 1 = necesita modo gráfico */
  const char *gui_reason;
  const char *env;         /* NOMBRE=valor separados por '|'; "@scratch" = dir temporal */
};

static const Recipe kRecipes[] = {
    /* Escriben un informe; el veredicto lo da el código de salida. */
    {"--fl-check-presets", "scripts/presets|@out", "", 0, "", ""},
    {"--fl-check-keyconfig-io", "@out", "", 0, "", ""},
    {"--fl-check-keymap-menus", "", "", 0, "", ""},
    /* El catálogo de herramientas vive en `toolsystem/`, no en `tools/`: la
     * convención de nombres no lo encuentra y sin línea base este comprobador
     * SALE CON 0 aunque no haya comprobado nada (verde falso, medido). */
    {"--fl-check-tools", "@baseline", "tests/flipendo/toolsystem/baseline-python.txt", 0, "", ""},
    /* El keymap por defecto NO se carga en --background (REGLAMENTO): la
     * comparación de verdad va en modo gráfico. */
    {"--fl-check-keymap", "@baseline", "tests/flipendo/keymap/baseline-python.txt", 1,
     "el keymap por defecto no se carga en --background", ""},
    /* Los operadores de `wm` vuelcan a fichero y se comparan aquí. Las líneas
     * base se identificaron comparando el volcado byte a byte contra todos los
     * `.txt` de tests/flipendo. */
    {"--fl-selftest-wm-property-ops", "@out", "tests/flipendo/operators/properties-python.txt", 1,
     "en --background no escribe nada: necesita un editor real", ""},
    {"--fl-selftest-wm-system-ops", "@out", "tests/flipendo/operators/system-python.txt", 0, "", ""},
    {"--fl-selftest-wm-owner-ops", "@out", "tests/flipendo/operators/owner-python.txt", 0, "", ""},
    {"--fl-selftest-wm-properties-edit", "@out",
     "tests/flipendo/operators/properties-edit-native.txt", 0, "", ""},
    {"--fl-selftest-wm-batch-rename", "@out",
     "tests/flipendo/operators/batch-rename-native.txt", 0, "", ""},
    /* Informe de cifras, sin línea base congelada en el árbol. */
    {"--fl-selftest-keyconfig", "@out", "", 0, "", ""},
    /* El volcado de C++ NO es byte a byte igual al de Python: difiere en el
     * último dígito de varios flotantes (la acumulación va en paralelo, ver la
     * lección de las 03:50 del REGLAMENTO). La línea base verificada del árbol
     * es la segunda; si coincide con esa y no con la de Python, se dice. */
    {"--fl-selftest-object-ops", "@out",
     "tests/flipendo/objectops/baseline-python.txt|tests/flipendo/objectops/run-cpp-verified.txt",
     0, "", ""},
    /* --- Carril ARNES, 2026-09-11 --- */
    /* `--fl-selftest-context-ops` ya NO da veredicto: su propia ayuda dice «Solo
     * VUELCA: el veredicto lo da --fl-check-context-ops», que es el que lee las
     * divergencias declaradas. Se deja como volcador y el veredicto lo firma el
     * comprobador nuevo. */
    {"--fl-check-context-ops", "@baseline", "tests/flipendo/operators/execution-python.txt", 1,
     "lo dice su propia ayuda: necesita modo grafico", ""},
    /* Los operadores de presets escriben en la carpeta de scripts del usuario. Su
     * ayuda manda apuntar `BLENDER_USER_SCRIPTS` a una carpeta vacía: si no, el
     * arnés ensucia la configuración real de quien mide. */
    /* Ojo con estas dos, que van al revés de lo que sugiere el nombre y se
     * comprobó por el marcador de la primera línea de cada fichero:
     *   presetops/baseline-python.txt      -> `# FL-PRESET-OPTYPE-SURFACE v1`
     *   presetops/comportamiento-python.txt -> `# FL-PRESET-OPS v1`
     * o sea que la línea base «de siempre» es la de los TIPOS de operador y el
     * comportamiento va en la otra. */
    {"--fl-selftest-preset-ops", "@out", "tests/flipendo/presetops/comportamiento-python.txt", 0,
     "", "BLENDER_USER_SCRIPTS=@scratch"},
    {"--fl-check-preset-ops", "@baseline", "tests/flipendo/presetops/comportamiento-python.txt", 0,
     "", "BLENDER_USER_SCRIPTS=@scratch"},
    {"--fl-check-preset-optypes", "@baseline", "tests/flipendo/presetops/baseline-python.txt", 0,
     "", ""},
    {"--fl-check-lod-optypes", "@baseline", "tests/flipendo/lod/optypes-python.txt", 0, "", ""},
};

static const Recipe *recipe_for(const std::string &flag)
{
  for (const Recipe &r : kRecipes) if (flag == r.flag) return &r;
  return nullptr;
}

static std::vector<std::string> split_pipe(const std::string &s)
{
  std::vector<std::string> out;
  size_t a = 0;
  while (a <= s.size()) {
    size_t b = s.find('|', a);
    if (b == std::string::npos) { if (a < s.size()) out.push_back(s.substr(a)); break; }
    out.push_back(s.substr(a, b - a));
    a = b + 1;
  }
  return out;
}

/* Primera ruta `tests/flipendo/...` citada por el propio doc string, exista o no. */
static std::string baseline_named_in_doc(const std::string &doc)
{
  size_t p = doc.find("tests/flipendo/");
  if (p == std::string::npos) return "";
  size_t e = p;
  while (e < doc.size() && !isspace((unsigned char)doc[e]) && doc[e] != '`' && doc[e] != ',') ++e;
  std::string path = doc.substr(p, e - p);
  while (!path.empty() && (path.back() == '.' || path.back() == ')')) path.pop_back();
  return path;
}

/* La misma, pero solo si el fichero está de verdad en el árbol. */
static std::string baseline_in_doc(const std::string &doc, const std::string &root)
{
  const std::string path = baseline_named_in_doc(doc);
  if (path.empty()) return "";
  return fs::exists(root + "/" + path) ? path : std::string();
}

/* Convención: `--fl-check-mesh-ops` -> `tests/flipendo/meshops/`, y si no existe
 * se van soltando segmentos por la derecha (`rigidbody-ops` -> `rigidbody`). */
static std::string baseline_by_convention(const std::string &flag, const std::string &root)
{
  size_t dash = flag.find("check-");
  size_t off = dash != std::string::npos ? dash + 6 : flag.find("selftest-") + 9;
  std::string name = flag.substr(off);
  std::vector<std::string> segs;
  size_t a = 0;
  while (a <= name.size()) {
    size_t b = name.find('-', a);
    if (b == std::string::npos) { segs.push_back(name.substr(a)); break; }
    segs.push_back(name.substr(a, b - a));
    a = b + 1;
  }
  for (size_t keep = segs.size(); keep >= 1; --keep) {
    std::string dir;
    for (size_t i = 0; i < keep; ++i) dir += segs[i];
    std::string cand = "tests/flipendo/" + dir + "/baseline-python.txt";
    if (fs::exists(root + "/" + cand)) return cand;
  }
  return "";
}

/* Los verificadores se descubren en el árbol, no se escriben aquí: se leen las
 * cadenas `"--fl-..."` de creator_args.cc, que es donde `BLI_args_add` los
 * registra. Si mañana hay uno más, aparece solo. */
static std::vector<std::string> discover_flags(const std::string &src, const std::string &kind)
{
  std::set<std::string> found;
  std::string needle = "\"--fl-" + kind + "-";
  size_t p = 0;
  while ((p = src.find(needle, p)) != std::string::npos) {
    size_t a = p + 1;
    size_t e = src.find('"', a);
    if (e == std::string::npos) break;
    found.insert(src.substr(a, e - a));
    p = e;
  }
  return std::vector<std::string>(found.begin(), found.end());
}


/* Ruido que el binario escribe por su cuenta y que no es el parte del
 * verificador: el saludo, el audio, y las excepciones de los add-ons instalados
 * (que se cuelan DESPUÉS del parte y se llevaban el sitio de la cifra buena). */
static bool is_noise_line(const std::string &t)
{
  return starts_with(t, "Blender 4.5") || t == "Blender quit" || contains(t, "ALSA lib") ||
         starts_with(t, "AL lib") || contains(t, "Exception in module register()") ||
         starts_with(t, "Traceback") || starts_with(t, "File \"") || starts_with(t, "ModuleNotFoundError") ||
         starts_with(t, "Warning:") || contains(t, "Read prefs:");
}

/* El parte del verificador se reconoce solo: lo escriben todos con su nombre o
 * con las cifras de la comparación. */
static bool is_report_line(const std::string &t)
{
  return contains(t, "fl-check") || contains(t, "fl-selftest") || contains(t, "FL-") ||
         contains(t, "identic") || contains(t, "distint") || contains(t, "comparad") ||
         contains(t, "aplicados") || contains(t, "linea base") || contains(t, "TOTAL") ||
         contains(t, "divergencia") || contains(t, "tolerancia");
}

static std::string meaningful_tail(const std::string &out)
{
  std::vector<std::string> ls = split_lines(out);
  auto clip = [](std::string t) {
    if (t.size() > 150) t = t.substr(0, 150) + "...";
    return t;
  };
  for (size_t i = ls.size(); i-- > 0;) {
    std::string t = trim(ls[i]);
    if (t.empty() || is_noise_line(t)) continue;
    if (is_report_line(t)) return clip(t);
  }
  for (size_t i = ls.size(); i-- > 0;) {
    std::string t = trim(ls[i]);
    if (t.empty() || is_noise_line(t)) continue;
    return clip(t);
  }
  return "";
}

struct Battery {
  std::vector<Verifier> v;
  long long green = 0, red = 0, unstable = 0, nobase = 0, skipped = 0, volcadores = 0;
  long long dumpers = 0;        /* --fl-dump-*, no dan veredicto */
  long long converters = 0;     /* el resto de --fl-* */
  long long fl_args_total = 0;
  long long distinct_flags = 0; /* banderas check/selftest distintas (sin variantes) */
  long long n_checks = 0, n_selfs = 0;
  double secs = 0;
  bool ran_gui = false;
  std::string binary;
  /* De qué binario hablamos exactamente: el banner de arranque dice con qué
   * commit se construyó. La batería mide ESE binario, no el commit medido. */
  std::string bin_hash, bin_built;
  /* Y el binario sale del ÁRBOL DE TRABAJO, no del commit: si otro carril tiene
   * trabajo a medias sin commitear, está dentro del binario que se prueba. */
  std::vector<std::string> dirty;
};

/* Del banner «Blender 4.5.0 Alpha (hash 35309693a44c built 2026-09-11 18:47:11)». */
static void parse_banner(const std::string &out, std::string &hash, std::string &built)
{
  if (!hash.empty()) return;
  size_t p = out.find("(hash ");
  if (p == std::string::npos) return;
  size_t a = p + 6, e = out.find(' ', a);
  if (e == std::string::npos) return;
  hash = out.substr(a, e - a);
  size_t b = out.find("built ", e);
  if (b == std::string::npos) return;
  size_t f = out.find(')', b);
  if (f == std::string::npos) return;
  built = out.substr(b + 6, f - b - 6);
}

static Battery run_battery(const std::string &root,
                           const std::string &binary,
                           const std::string &creator_src,
                           bool do_gui)
{
  Battery bat;
  bat.binary = binary;

  std::vector<std::string> checks = discover_flags(creator_src, "check");
  std::vector<std::string> selfs = discover_flags(creator_src, "selftest");
  {
    std::set<std::string> all;
    size_t p = 0;
    while ((p = creator_src.find("\"--fl-", p)) != std::string::npos) {
      size_t a = p + 1, e = creator_src.find('"', a);
      if (e == std::string::npos) break;
      all.insert(creator_src.substr(a, e - a));
      p = e;
    }
    bat.fl_args_total = (long long)all.size();
    for (const std::string &f : all) {
      if (starts_with(f, "--fl-dump-")) bat.dumpers++;
      else if (!starts_with(f, "--fl-check-") && !starts_with(f, "--fl-selftest-")) bat.converters++;
    }
  }

  std::string tmpdir = sfmt("%s/flipendo-estado-%d",
                            getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
  while (!tmpdir.empty() && tmpdir.find("//") != std::string::npos)
    tmpdir.erase(tmpdir.find("//"), 1);
  std::error_code ec;
  fs::create_directories(tmpdir, ec);

  auto make = [&](const std::string &flag, const std::string &kind) {
    Verifier v;
    v.flag = flag;
    v.label = flag;
    v.kind = kind;
    v.doc = doc_of(creator_src, flag);
    const Recipe *r = recipe_for(flag);
    std::string tmpname = flag.substr(2);
    for (auto &c : tmpname) if (c == '-') c = '_';
    v.out_file = tmpdir + "/" + tmpname + ".txt";

    if (r) {
      if (r->gui) { v.gui = true; v.gui_reason = r->gui_reason; }
      for (const std::string &bl : split_pipe(r->baselines)) if (!bl.empty()) v.baselines.push_back(bl);
      for (const std::string &a : split_pipe(r->args)) {
        if (a == "@out") v.args.push_back(v.out_file);
        else if (a == "@baseline") v.args.push_back(root + "/" + (v.baselines.empty() ? "" : v.baselines[0]));
        else v.args.push_back(a);
      }
      for (const std::string &e : split_pipe(r->env)) {
        if (e.empty()) continue;
        std::string kv = e;
        size_t at = kv.find("@scratch");
        if (at != std::string::npos) {
          const std::string sub = tmpdir + "/" + tmpname + ".scripts";
          std::error_code sec;
          fs::create_directories(sub, sec);
          kv = kv.substr(0, at) + sub;
        }
        v.env.push_back(kv);
      }
    }
    else {
      std::string bl = baseline_in_doc(v.doc, root);
      /* Una ayuda que declara una línea base que no está en el árbol no es un
       * detalle: es un comprobador que no puede comprobar. Se anota y se dice. */
      if (bl.empty()) v.doc_baseline_missing = baseline_named_in_doc(v.doc);
      if (bl.empty()) bl = baseline_by_convention(flag, root);
      if (!bl.empty()) v.baselines.push_back(bl);
      if (kind == "check") {
        if (!bl.empty()) v.args.push_back(root + "/" + bl);
      }
      else {
        v.args.push_back(v.out_file);
      }
    }
    /* El propio doc string dice cuándo hace falta pantalla. */
    if (!v.gui && (contains(v.doc, "modo grafico") || contains(v.doc, "modo gráfico"))) {
      v.gui = true;
      v.gui_reason = "lo dice su propia ayuda en creator_args.cc";
    }
    /* Un selftest cuya ayuda dice «Solo VUELCA: el veredicto lo da --fl-check-X»
     * NO da veredicto: es un volcador y contarlo como verde o como rojo sería
     * inventarse una comprobación. Lo firma su comprobador hermano. */
    if (kind == "selftest" && (contains(v.doc, "Solo VUELCA") || contains(v.doc, "solo VUELCA"))) {
      v.kind = "volcador";
      v.baselines.clear();
    }
    /* Declaraciones que acompañan a la línea base preferente. */
    if (!v.baselines.empty()) v.decl = read_declarations(root + "/" + v.baselines[0]);
    return v;
  };

  for (const std::string &f : checks) bat.v.push_back(make(f, "check"));
  for (const std::string &f : selfs) bat.v.push_back(make(f, "selftest"));
  bat.n_checks = (long long)checks.size();
  bat.n_selfs = (long long)selfs.size();
  bat.distinct_flags = bat.n_checks + bat.n_selfs;

  /* `--fl-check-ui` admite las dos líneas base (registro y dibujo) y lo decide la
   * marca de la primera línea del fichero: son dos comprobaciones distintas, y la
   * de dibujo necesita pantalla. Se duplica la entrada para que el informe no
   * esconda la mitad. */
  for (size_t i = 0; i < bat.v.size(); ++i) {
    if (bat.v[i].flag != "--fl-check-ui") continue;
    const std::string layout = "tests/flipendo/ui/baseline-python-layout.txt";
    if (!fs::exists(root + "/" + layout)) break;
    Verifier extra = bat.v[i];
    bat.v[i].label = "--fl-check-ui (registro)";
    bat.v[i].baselines = {"tests/flipendo/ui/baseline-python.txt"};
    bat.v[i].args = {root + "/tests/flipendo/ui/baseline-python.txt"};
    extra.label = "--fl-check-ui (dibujo)";
    extra.baselines = {layout};
    extra.args = {root + "/" + layout};
    extra.gui = true;
    extra.gui_reason = "el volcado de dibujo necesita una ventana real";
    bat.v.push_back(extra);
    break;
  }

  std::sort(bat.v.begin(), bat.v.end(), [](const Verifier &a, const Verifier &b) {
    if (a.gui != b.gui) return !a.gui;
    if (a.kind != b.kind) return a.kind < b.kind;
    return a.label < b.label;
  });

  /* Ejecuta UNA vez y deja el veredicto en `v`. No cuenta nada: contar es del
   * bucle, porque un rojo se repite antes de darlo por rojo. */
  auto execute = [&](Verifier &v) {
    std::remove(v.out_file.c_str());
    std::vector<std::string> argv = {binary};
    if (!v.gui) argv.push_back("--background");
    argv.push_back("--factory-startup");
    argv.push_back(v.flag);
    for (const std::string &a : v.args) argv.push_back(a);

    ProcResult r = run_process(argv, root, "", 900, true, v.env.empty() ? nullptr : &v.env);
    parse_banner(r.out, bat.bin_hash, bat.bin_built);
    v.ran = true;
    v.passes++;
    v.rc = r.rc;
    v.timed_out = r.timed_out;
    v.secs = r.secs;
    v.mode = v.gui ? "grafico" : "background";
    v.detail = meaningful_tail(r.out);

    if (r.timed_out) {
      v.verdict = "ROJO";
      v.detail = "se pasó del tiempo máximo (900 s) y hubo que matarlo";
      return;
    }
    /* Trampa medida: un verificador al que le falta su argumento imprime el
     * error y SALE CON 0. Darlo por verde sería mentir. */
    if (contains(v.detail, "falta la linea base") || contains(v.detail, "falta el fichero")) {
      v.verdict = "SIN LINEA BASE";
      v.detail = "no se le pudo resolver la linea base: " + v.detail;
      return;
    }
    if (v.kind == "check") {
      /* El comprobador compara él mismo, honra sus propias declaraciones y sale
       * con EXIT_FAILURE si no cuadra. Aquí no se repite esa comparación: se
       * ESCUCHA lo que dice por su salida de error, que es la fuente. */
      v.baseline_used = v.baselines.empty() ? "" : v.baselines[0];
      if (contains(r.out, "TOLERADA")) v.tol_passes_used++;
      long long this_pass_div = 0;
      for (const std::string &ln : split_lines(r.out)) {
        if (contains(ln, "DIVERGENCIA DELIBERADA cumplida")) { v.div_honoured++; this_pass_div++; }
        double w = 0.0;
        if (sscanf(ln.c_str(), "%*[^(](desviacion relativa %lf", &w) == 1)
          v.worst_tolerated = std::fmax(v.worst_tolerated, w);
      }
      if (this_pass_div > 0) v.div_passes_used++;
      v.verdict = r.rc == 0 ? "VERDE" : "ROJO";
      return;
    }
    /* Un `--fl-selftest-*` es un VOLCADOR, no una prueba: escribe el fichero y
     * sale con 0 aunque no haya escrito nada (medido: sin argumento imprime
     * «falta el fichero de salida» y devuelve 0 igual). El veredicto se saca
     * comparando el volcado con su línea base, que es lo que hace su
     * `--fl-check-*` hermano cuando existe. */
    std::string dump;
    if (!(read_file(v.out_file, dump) && !dump.empty())) {
      v.verdict = "ROJO";
      if (v.detail.empty()) v.detail = "no escribió volcado";
      return;
    }
    long long dump_lines = 0;
    for (char c : dump) if (c == '\n') ++dump_lines;
    if (v.kind == "volcador") {
      /* Su propia ayuda dice que no da veredicto. Lo único exigible es que
       * vuelque algo; el juicio lo firma su comprobador hermano. */
      v.verdict = "VOLCADOR";
      v.detail = sfmt("volcado de %lld lineas; su ayuda dice que el veredicto lo da su "
                      "comprobador hermano", dump_lines);
      return;
    }
    if (v.baselines.empty()) {
      v.verdict = "SIN LINEA BASE";
      v.detail = sfmt("informe de %lld lineas; no hay linea base congelada en el arbol", dump_lines);
      return;
    }
    /* Antes de dar por bueno un rojo: ¿es siquiera la línea base que le toca?
     * Los volcados llevan un marcador de formato en la primera línea
     * (`# FL-LOD-OPS v1`). Si no coincide, lo que falla es el emparejamiento, no
     * el código, y decir «rojo» sería acusar a quien no es. */
    {
      std::string base_first;
      std::string bx;
      if (read_file(root + "/" + v.baselines[0], bx)) {
        std::vector<std::string> lb = split_lines(bx), la = split_lines(dump);
        if (!lb.empty() && !la.empty() && starts_with(trim(lb[0]), "# FL-") &&
            starts_with(trim(la[0]), "# FL-") && trim(lb[0]) != trim(la[0])) {
          v.verdict = "SIN LINEA BASE";
          v.detail = sfmt("la linea base no es de este volcado: el volcado dice `%s` y "
                          "`%s` dice `%s`",
                          trim(la[0]).c_str(), v.baselines[0].c_str(), trim(lb[0]).c_str());
          return;
        }
      }
    }
    std::string why;
    for (size_t i = 0; i < v.baselines.size(); ++i) {
      const std::string base_abs = root + "/" + v.baselines[i];
      const Declarations decl = (i == 0) ? v.decl : read_declarations(base_abs);
      CompareResult cr = compare_to_baseline(v.out_file, base_abs, decl);
      if (cr.ok) {
        v.verdict = "VERDE";
        v.baseline_used = v.baselines[i];
        v.detail = sfmt("%lld lineas identicas a %s", dump_lines, v.baselines[i].c_str());
        /* Lo tolerado y lo divergente se dice SIEMPRE en voz alta: una
         * declaración callada es una excusa. */
        if (cr.tolerated) {
          v.tol_passes_used++;
          v.worst_tolerated = std::fmax(v.worst_tolerated, cr.worst_tolerated);
          v.detail = sfmt("%lld/%lld lineas identicas y %lld TOLERADA(S) por la tolerancia "
                          "declarada %g (peor desviacion %.3g)",
                          cr.same_lines - cr.tolerated, cr.total_lines, cr.tolerated,
                          decl.tolerance, cr.worst_tolerated);
        }
        if (cr.div_honoured) {
          v.div_honoured = cr.div_honoured;
          v.div_passes_used++;
          v.detail += sfmt(", %lld divergencia(s) deliberada(s) cumplida(s)", cr.div_honoured);
        }
        if (i > 0)
          v.detail += sfmt(" (NO a la preferente %s: %s)", v.baselines[0].c_str(), why.c_str());
        return;
      }
      if (i == 0) {
        why = cr.why;
        if (cr.tolerated) {
          v.tol_passes_used++;
          v.worst_tolerated = std::fmax(v.worst_tolerated, cr.worst_tolerated);
        }
        v.div_honoured = cr.div_honoured;
      }
    }
    v.verdict = "ROJO";
    v.baseline_used = v.baselines[0];
    v.detail = why;
  };

  const double t0 = now_secs();
  for (Verifier &v : bat.v) {
    if (v.gui && !do_gui) {
      v.verdict = "NO EJECUTADO";
      v.detail = "grupo gráfico desactivado (--sin-grafico)";
      bat.skipped++;
      continue;
    }
    execute(v);
    /* «Si un volcado no se reproduce a sí mismo, no es una línea base»
     * (REGLAMENTO, lección de las 03:50: el cálculo de normales no es
     * determinista). Un rojo se repite antes de firmarlo: si la segunda pasada
     * sale verde, lo que hay no es un fallo, es un verificador INESTABLE, y eso
     * se dice con esas palabras en vez de esconderlo en un rojo o en un verde. */
    /* Un verificador lento tampoco puede firmar un rojo a la primera. Esta
     * excepción existía —«si tarda más de un minuto, el rojo se firma con una
     * sola pasada»— y el 11 de septiembre firmó como ROJO el volcado de diseño
     * (2 bloques de 2.004) cuando la medición manual inmediata daba CERO bloques
     * distintos: era un transitorio. Un rojo sin confirmar es tan malo como un
     * verde sin comprobar, y precisamente el comprobador más lento es el más
     * expuesto a transitorios.
     *
     * No se triplica, que convertiría la batería en media hora: se repite UNA
     * vez, que es cuanto hace falta para que un transitorio no firme nada. */
    if (v.verdict == "ROJO" && v.secs > 60.0) {
      const std::string primera = v.detail;
      const double primera_secs = v.secs;
      execute(v);
      v.secs += primera_secs;
      if (v.verdict == "ROJO") {
        v.detail = primera + sfmt("  [reproducido en 2 pasadas de %.0f s]", primera_secs);
      }
      else {
        v.verdict = "INESTABLE";
        v.detail = sfmt("la primera pasada dio rojo y la segunda verde con el mismo "
                        "binario: es un transitorio, no un fallo. Pasada en rojo: %s. "
                        "Pasada en verde: %s",
                        primera.c_str(), v.detail.c_str());
      }
    }
    else if (v.verdict == "ROJO") {
      const std::string first = v.detail;
      const double first_secs = v.secs;
      long long fails = 1;
      std::string last_green;
      for (int pass = 2; pass <= 3; ++pass) {
        execute(v);
        if (v.verdict == "ROJO") fails++;
        else last_green = v.detail;
      }
      v.secs += first_secs;
      if (fails == 3) {
        v.verdict = "ROJO";
        v.detail = first + "  [reproducido en las 3 pasadas]";
      }
      else {
        v.verdict = "INESTABLE";
        v.detail = sfmt("falla %lld de 3 pasadas con el mismo binario y la misma escena. "
                        "Pasada en rojo: %s. Pasada en verde: %s",
                        fails, first.c_str(),
                        last_green.empty() ? "(sin detalle)" : last_green.c_str());
      }
    }
    /* Un verificador CON DECLARACIÓN se pasa tres veces aunque haya salido verde.
     * No es desconfianza: es la única forma de contestar a «¿esta declaración
     * sigue haciendo falta?». Si en tres pasadas no se tolera ni una línea, la
     * declaración puede sobrar, y eso hay que decirlo igual que se dice un rojo:
     * una excepción que nadie revisa se convierte en una excusa permanente. */
    else if (v.decl.any() && v.verdict == "VERDE" && v.secs <= 60.0 && v.passes == 1) {
      const double first_secs = v.secs;
      const std::string first = v.detail;
      double acc = first_secs;
      bool any_red = false;
      std::string red_detail;
      for (int pass = 2; pass <= 3; ++pass) {
        execute(v);
        acc += v.secs;
        if (v.verdict != "VERDE") { any_red = true; red_detail = v.detail; }
      }
      v.secs = acc;
      if (any_red) {
        v.verdict = "INESTABLE";
        v.detail = sfmt("no se reproduce a si mismo ni con su declaracion: %s", red_detail.c_str());
      }
      else {
        v.verdict = "VERDE";
        v.detail = first;
      }
    }

    if (v.verdict == "VERDE") bat.green++;
    else if (v.verdict == "ROJO") bat.red++;
    else if (v.verdict == "INESTABLE") bat.unstable++;
    else if (v.verdict == "VOLCADOR") bat.volcadores++;
    else bat.nobase++;
  }
  bat.secs = now_secs() - t0;
  bat.ran_gui = do_gui;
  fs::remove_all(tmpdir, ec);
  return bat;
}

/* -------------------------------------------------------------------------- */
/* Inventario de declaraciones del árbol                                       */
/*                                                                             */
/* «Una declaración escondida es una excusa; una declarada y contada es una     */
/* decisión de ingeniería.» Por eso se cuentan TODAS las que hay en el árbol,   */
/* no solo las que se usaron, y se dice cuál no llegó a hacer falta.            */

struct DeclaredFile {
  std::string path;       /* ruta relativa del `.tolerancia` / `.divergencias` */
  std::string kind;       /* "tolerancia" | "divergencias" */
  std::string baseline;   /* la línea base a la que acompaña */
  bool baseline_exists = false;
  std::string summary;    /* qué declara, en una línea */
  std::string reason;     /* el `razon:` de la primera divergencia, recortado */
  std::string used_by;    /* verificadores que la ejercitaron en esta pasada */
  bool needed = false;    /* hizo falta de verdad en alguna pasada */
  /* «Hizo falta» se cuenta POR DECLARACIÓN, no por verificador: la misma
   * tolerancia la usan `--fl-check-mesh-ops` y `--fl-selftest-mesh-ops`, y el
   * valor inestable solo aparece en algunas pasadas. Si se contara por
   * verificador, el informe diría a la vez «hizo falta» y «puede sobrar». */
  long long passes_total = 0, passes_needed = 0;
};

static std::vector<DeclaredFile> scan_declarations(const std::string &root)
{
  std::vector<DeclaredFile> out;
  std::error_code ec;
  const std::string base = root + "/tests";
  if (!fs::exists(base, ec)) return out;
  for (auto it = fs::recursive_directory_iterator(base, fs::directory_options::skip_permission_denied, ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) break;
    if (!it->is_regular_file(ec)) continue;
    const std::string p = it->path().string();
    const bool tol = ends_with(p, ".tolerancia");
    const bool div = ends_with(p, ".divergencias");
    if (!tol && !div) continue;
    DeclaredFile d;
    d.path = p.substr(root.size() + 1);
    d.kind = tol ? "tolerancia" : "divergencias";
    d.baseline = d.path.substr(0, d.path.size() - (tol ? 11 : 13));
    d.baseline_exists = fs::exists(root + "/" + d.baseline);
    const Declarations decl = read_declarations(root + "/" + d.baseline);
    if (tol) {
      d.summary = sfmt("tolerancia relativa %g", decl.tolerance);
    }
    else {
      d.summary = sfmt("%zu divergencia(s) deliberada(s)", decl.divergences.size());
      for (const Divergence &dv : decl.divergences) {
        d.summary += sfmt(", linea %d", dv.line_no);
        if (d.reason.empty() && !dv.razon.empty()) {
          d.reason = dv.razon;
          if (d.reason.size() > 200) d.reason = d.reason.substr(0, 200) + "...";
        }
      }
    }
    out.push_back(d);
  }
  std::sort(out.begin(), out.end(),
            [](const DeclaredFile &a, const DeclaredFile &b) { return a.path < b.path; });
  return out;
}

/* -------------------------------------------------------------------------- */
/* Auditoría de lo que afirman los documentos                                  */

struct DocFinding {
  std::string doc, said, measured, how;
};

/* -------------------------------------------------------------------------- */
/* Modo antiguo: recorrer el disco                                             */

static int walk_disk(const std::string &root)
{
  std::map<std::string, std::pair<long long, long long>> maint, vend;
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) break;
    if (!it->is_regular_file(ec)) continue;
    const fs::path &p = it->path();
    const char *lg = lang_of(p.string());
    if (!lg) continue;
    std::string sp = p.string();
    bool is_vendored = contains(sp, "/extern/") || contains(sp, "/lib/");
    std::ifstream f(p, std::ios::binary);
    long long n = 0;
    char c;
    while (f.get(c)) if (c == '\n') ++n;
    auto &slot = (is_vendored ? vend : maint)[lg];
    slot.first += 1;
    slot.second += n;
  }
  auto dump = [](const char *title, std::map<std::string, std::pair<long long, long long>> &m) {
    long long tf = 0, tl = 0;
    std::printf("\n=== %s ===\n%-16s %8s %12s\n", title, "lenguaje", "ficheros", "lineas");
    for (auto &kv : m) {
      std::printf("%-16s %8lld %12lld\n", kv.first.c_str(), kv.second.first, kv.second.second);
      tf += kv.second.first;
      tl += kv.second.second;
    }
    std::printf("%-16s %8lld %12lld\n", "TOTAL", tf, tl);
  };
  dump("Flipendo mantenido (sin extern/ ni lib/)", maint);
  dump("Vendorizado (extern/ + lib/)", vend);
  return 0;
}

/* -------------------------------------------------------------------------- */
/* Informe                                                                     */

static std::string today_local()
{
  time_t t = time(nullptr);
  struct tm lt;
  localtime_r(&t, &lt);
  char buf[64];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &lt);
  return buf;
}

struct Options {
  std::string root = ".";
  std::string rev = "HEAD";
  std::string build;
  std::string binary;
  std::string out;
  bool estado = false, bateria_only = false, disco = false;
  bool run_bateria = true, run_gui = true;
};

static void emit_lang_table(std::string &r,
                            const std::map<std::string, std::pair<long long, long long>> &m,
                            long long total_lines)
{
  std::vector<std::pair<std::string, std::pair<long long, long long>>> rows(m.begin(), m.end());
  std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) {
    int ra = lang_rank(a.first), rb = lang_rank(b.first);
    if (ra != rb) return ra < rb;
    return a.second.second > b.second.second;
  });
  r += "| Lenguaje | Ficheros | Líneas | % |\n|---|---:|---:|---:|\n";
  for (const auto &kv : rows) {
    r += sfmt("| %s | %s | %s | %s |\n", kv.first.c_str(), mil(kv.second.first).c_str(),
              mil(kv.second.second).c_str(), pct(kv.second.second, total_lines).c_str());
  }
}

int main(int argc, char **argv)
{
  signal(SIGPIPE, SIG_IGN);
  Options op;
  std::vector<std::string> pos;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
    if (a == "--estado") op.estado = true;
    else if (a == "--bateria" || a == "--batería") { op.bateria_only = true; }
    else if (a == "--sin-bateria" || a == "--sin-batería") op.run_bateria = false;
    else if (a == "--sin-grafico" || a == "--sin-gráfico") op.run_gui = false;
    else if (a == "--disco") op.disco = true;
    else if (a == "--rev") op.rev = next();
    else if (a == "--build") op.build = next();
    else if (a == "--binario") op.binary = next();
    else if (a == "--salida") op.out = next();
    else if (a == "-h" || a == "--ayuda" || a == "--help") {
      std::printf(
          "flipendo-metrics — el estado de Flipendo, medido desde el arbol.\n\n"
          "  flipendo-metrics [raiz]              tabla de composicion (git, al commit)\n"
          "  flipendo-metrics --estado [raiz]     informe completo -> politicas/ESTADO.md\n"
          "  flipendo-metrics --bateria [raiz]    solo los verificadores del binario\n\n"
          "  --rev <commit>   medir a otro commit (por defecto HEAD)\n"
          "  --sin-bateria    no ejecutar los verificadores\n"
          "  --sin-grafico    no ejecutar el grupo que necesita pantalla\n"
          "  --disco          recorrer el disco en vez de git (no reproducible)\n"
          "  --build <dir>    arbol de compilacion (por defecto <raiz>/../build)\n"
          "  --binario <ruta> binario a verificar\n"
          "  --salida <ruta>  donde escribir el informe\n");
      return 0;
    }
    else pos.push_back(a);
  }
  if (!pos.empty()) op.root = pos[0];

  std::error_code ec;
  fs::path rootp = fs::canonical(op.root, ec);
  if (ec) { std::fprintf(stderr, "No existe la raiz '%s'\n", op.root.c_str()); return 2; }
  const std::string root = rootp.string();

  if (op.disco) return walk_disk(root);

  Git git{root};
  if (git.line({"rev-parse", "--git-dir"}).empty()) {
    std::fprintf(stderr, "'%s' no es un arbol de git; usa --disco si de verdad quieres el disco.\n",
                 root.c_str());
    return 2;
  }
  if (op.build.empty()) op.build = (rootp.parent_path() / "build").string();
  if (op.binary.empty())
    op.binary = op.build + "/bin/Blender.app/Contents/MacOS/Blender";
  if (op.out.empty()) op.out = root + "/politicas/ESTADO.md";

  /* ---- medición ---- */
  BridgeScan bs;
  Snapshot snap = take_snapshot(git, op.rev, bridge_scan, &bs);
  if (snap.files.empty()) { std::fprintf(stderr, "No se pudo leer el arbol a '%s'\n", op.rev.c_str()); return 2; }
  LangTotals tot = totals_of(snap);

  if (!op.estado && !op.bateria_only) {
    std::printf("\n=== Flipendo a %s (%s) ===\n", snap.shortrev.c_str(), snap.date.c_str());
    std::printf("\n--- codigo propio (sin los vendorizados) ---\n%-16s %8s %12s\n",
                "lenguaje", "ficheros", "lineas");
    for (const auto &kv : tot.own)
      std::printf("%-16s %8lld %12lld\n", kv.first.c_str(), kv.second.first, kv.second.second);
    std::printf("%-16s %8lld %12lld\n", "TOTAL", tot.own_files, tot.own_lines);
    std::printf("\n--- terceros vendorizados ---\n");
    for (const auto &kv : tot.vend)
      std::printf("%-16s %8lld %12lld\n", kv.first.c_str(), kv.second.first, kv.second.second);
    std::printf("%-16s %8lld %12lld\n", "TOTAL", tot.vend_files, tot.vend_lines);
    std::printf("\n(informe completo: flipendo-metrics --estado %s)\n", op.root.c_str());
    return 0;
  }

  std::string creator_src = git.show(snap.rev, "source/creator/creator_args.cc");
  const bool have_binary = fs::exists(op.binary);
  Battery bat;
  if ((op.run_bateria || op.bateria_only) && have_binary && !creator_src.empty()) {
    /* El estado sucio se lee ANTES de la batería: es el árbol del que salió el
     * binario que se va a probar. */
    ProcResult st = git.run({"status", "--porcelain"}, "", false);
    for (const std::string &ln : split_lines(st.out)) {
      std::string t = trim(ln);
      if (t.empty() || starts_with(t, "??")) continue;
      bat.dirty.push_back(t);
    }
    std::vector<std::string> dirty = bat.dirty;
    bat = run_battery(root, op.binary, creator_src, op.run_gui);
    bat.dirty = dirty;
  }

  if (op.bateria_only) {
    std::printf("\n=== bateria de verificadores (%s) ===\n", snap.shortrev.c_str());
    for (const Verifier &v : bat.v)
      std::printf("%-34s %-13s %-11s %5.1fs  %s\n", v.label.c_str(), v.verdict.c_str(),
                  v.mode.c_str(), v.secs, v.detail.c_str());
    std::printf("\nverde %lld · rojo %lld · inestable %lld · sin linea base %lld · "
                "volcadores %lld · no ejecutados %lld · %.0f s\n",
                bat.green, bat.red, bat.unstable, bat.nobase, bat.volcadores, bat.skipped,
                bat.secs);
    return bat.red == 0 ? 0 : 1;
  }

  BuildFacts build = read_build(op.build, root);

  /* ---- historia ---- */
  std::vector<Commit> commits = commit_list(git, snap.rev);
  std::vector<HistPoint> daily, hourly;
  {
    std::set<std::string> seen_day;
    std::vector<std::string> day_revs;
    for (const Commit &c : commits) {   /* del más nuevo al más viejo */
      if (seen_day.insert(c.day).second) day_revs.push_back(c.hash);
    }
    std::reverse(day_revs.begin(), day_revs.end());
    /* El primer commit del repositorio es la línea base y tiene que salir. */
    if (!commits.empty()) {
      const std::string first = commits.back().hash;
      if (std::find(day_revs.begin(), day_revs.end(), first) == day_revs.end())
        day_revs.insert(day_revs.begin(), first);
    }
    for (const std::string &r : day_revs) daily.push_back(measure_point(git, r, snap.vendored_prefixes));

    std::set<std::string> seen_hour;
    std::vector<std::string> hour_revs;
    const std::string last_day = commits.empty() ? "" : commits.front().day;
    for (const Commit &c : commits) {
      if (c.day != last_day) break;
      if (seen_hour.insert(c.hour).second) hour_revs.push_back(c.hash);
    }
    std::reverse(hour_revs.begin(), hour_revs.end());
    for (const std::string &r : hour_revs) hourly.push_back(measure_point(git, r, snap.vendored_prefixes));
  }

  /* ---- shell por *shebang*, no por extensión ---- */
  std::vector<std::string> shebang_files;
  {
    ProcResult r = git.run({"grep", "-l", "-I", "-E", "^#!.*(bash|/bin/sh|zsh|ksh)", snap.rev, "--", "*"},
                           "", false);
    for (const std::string &ln : split_lines(r.out)) {
      std::string p = trim(ln);
      size_t c = p.find(':');
      if (c != std::string::npos) p = p.substr(c + 1);
      if (p.empty()) continue;
      bool vend = false;
      for (const std::string &pre : snap.vendored_prefixes) if (starts_with(p, pre)) vend = true;
      if (!vend) shebang_files.push_back(p);
    }
    std::sort(shebang_files.begin(), shebang_files.end());
  }

  /* ---- auditoría de documentos ---- */
  std::vector<DocFinding> findings;
  {
    std::string readme;
    if (read_file(root + "/README.md", readme)) {
      size_t p = readme.find("verificadores-");
      if (p != std::string::npos) {
        size_t a = p + 14, e = a;
        while (e < readme.size() && isdigit((unsigned char)readme[e])) ++e;
        long long said = strtoll(readme.substr(a, e - a).c_str(), nullptr, 10);
        if (bat.fl_args_total && said != bat.distinct_flags) {
          std::string why;
          if (said == bat.distinct_flags + bat.dumpers)
            why = sfmt(" La cifra de la insignia sale de sumar los %lld volcadores "
                       "`--fl-dump-*`, que no dan veredicto: no se pueden poner «en verde».",
                       bat.dumpers);
          findings.push_back({
              "README.md (insignia)",
              sfmt("«verificadores — %lld»", said),
              sfmt("%lld banderas `--fl-check-*`/`--fl-selftest-*` registradas en "
                   "`creator_args.cc` (+ %lld volcadores `--fl-dump-*` y %lld conversores: "
                   "%lld banderas `--fl-*` en total).%s",
                   bat.distinct_flags, bat.dumpers, bat.converters, bat.fl_args_total,
                   why.c_str()),
              "contando las cadenas `\"--fl-...\"` de `source/creator/creator_args.cc` al commit medido"});
        }
      }
    }
    std::string metricas;
    if (read_file(root + "/politicas/METRICAS.md", metricas)) {
      size_t p = metricas.find("commit `");
      if (p != std::string::npos) {
        size_t a = p + 8, e = metricas.find('`', a);
        std::string said_rev = metricas.substr(a, e - a);
        std::string full = git.line({"rev-parse", said_rev});
        if (!full.empty() && full != snap.rev) {
          std::string behind = git.line({"rev-list", "--count", said_rev + ".." + snap.rev});
          HistPoint then = measure_point(git, said_rev, snap.vendored_prefixes);
          auto delta = [](long long a, long long b) {
            long long d = b - a;
            return sfmt("%s → %s (%s%s)", mil(a).c_str(), mil(b).c_str(), d >= 0 ? "+" : "−",
                        mil(d >= 0 ? d : -d).c_str());
          };
          findings.push_back({
              "politicas/METRICAS.md (cabecera)",
              sfmt("medido al commit `%s`", said_rev.c_str()),
              sfmt("ese commit va **%s commits por detrás** del medido aquí (`%s`). Entre uno y "
                   "otro: Python %s, C++ %s, Objective-C++ %s, `.hpp` %s",
                   behind.c_str(), snap.shortrev.c_str(),
                   delta(then.py_lines, lines_of(tot, "Python")).c_str(),
                   delta(then.cpp, lines_of(tot, "C++")).c_str(),
                   delta(then.mm, lines_of(tot, "Objective-C++")).c_str(),
                   delta(then.hpp, lines_of(tot, ".hpp")).c_str()),
              "`git rev-list --count` entre los dos commits y una medición completa de cada uno"});
        }
      }
    }
  }

  /* ---- informe ---- */
  std::string r;
  const long long own = tot.own_lines;
  const long long cpp = lines_of(tot, "C++"), hh = lines_of(tot, ".hh"),
                  hpp = lines_of(tot, ".hpp"), hlegacy = lines_of(tot, ".h heredada");
  const long long py = lines_of(tot, "Python"), glsl = lines_of(tot, "GLSL"),
                  mm = lines_of(tot, "Objective-C++"), msl = lines_of(tot, "MSL"),
                  metal = lines_of(tot, "Metal"), cc = lines_of(tot, "C"),
                  sh = lines_of(tot, "shell");

  r += "# Estado de Flipendo — medido, no escrito\n\n";
  r += "> **FICHERO GENERADO. NO SE EDITA A MANO.** Cualquier cambio que escribas aquí\n";
  r += "> lo borra la siguiente regeneración. Se genera con\n";
  r += "> `tools/flipendo_metrics/flipendo_metrics.cpp`, que es C++ y mide el árbol:\n";
  r += "> ninguna cifra de este documento está copiada de otro documento.\n";
  r += ">\n";
  r += sfmt("> **Medido el %s sobre el commit `%s`** (%s — «%s»).\n",
            today_local().c_str(), snap.shortrev.c_str(), snap.date.c_str(), snap.subject.c_str());
  r += ">\n";
  r += "> Regenerar:\n";
  r += "> ```sh\n";
  r += "> c++ -std=c++17 -O2 -o ~/Flipendo/bin/flipendo-metrics \\\n";
  r += ">     ~/Flipendo/dev/upbge/tools/flipendo_metrics/flipendo_metrics.cpp\n";
  r += "> flipendo-metrics --estado ~/Flipendo/dev/upbge\n";
  r += "> ```\n\n";
  r += "Por qué existe: `politicas/METRICAS.md` publicó el 8 de septiembre una cifra de\n";
  r += "Python medida el 5 —entre medias habían caído 109.442 líneas sin registrarse— y el\n";
  r += "backlog describía zonas que el árbol ya no tenía. Un documento a mano envejece en\n";
  r += "horas cuando hay siete carriles trabajando. La solución no es escribir mejor: es\n";
  r += "**generar**.\n\n";
  r += "---\n\n";

  /* §1 método */
  r += "## 1. Cómo se mide cada cifra\n\n";
  r += "- **A un commit, nunca al árbol de trabajo.** Con varios carriles vivos, medir el\n";
  r += "  disco no es reproducible ni por quien lo hizo: se ven ficheros sin versionar y no\n";
  r += "  se ve lo que otro acaba de borrar. El árbol se lee con `git ls-tree -r` y un solo\n";
  r += "  `git cat-file --batch`, al commit de la cabecera.\n";
  r += "- **Una línea es un `\\n`**, como `wc -l`.\n";
  r += "- **Propio contra vendorizado**: los prefijos no están escritos en la herramienta,\n";
  r += "  se leen de `.gitattributes` (todo patrón marcado `linguist-vendored`). Hoy son: ";
  for (size_t i = 0; i < snap.vendored_prefixes.size(); ++i)
    r += sfmt("%s`%s`", i ? ", " : "", snap.vendored_prefixes[i].c_str());
  r += ".\n";
  r += "- **Compilado o no** sale de `build.ninja` y `CMakeCache.txt` del árbol de\n";
  r += "  compilación, no de lo que diga una política.\n";
  r += "- **La evolución** sale del historial de git: se mide de verdad cada commit\n";
  r += "  muestreado, no se copia de ninguna tabla.\n";
  r += "- **La batería** se ejecuta: cada verificador del binario, con su línea base, y el\n";
  r += "  código de salida es el veredicto.\n\n";
  r += sfmt("Ficheros versionados al commit medido: **%s**. De ellos, **%s son de código**:\n"
            "%s propios y %s vendorizados. El resto (%s) son datos, assets, textos y\n"
            "configuración, que por doctrina no son código.\n\n",
            mil(snap.tracked_total).c_str(), mil(tot.own_files + tot.vend_files).c_str(),
            mil(tot.own_files).c_str(), mil(tot.vend_files).c_str(),
            mil(snap.tracked_total - tot.own_files - tot.vend_files).c_str());
  r += "---\n\n";

  /* §2 composición */
  r += "## 2. Composición: código propio\n\n";
  emit_lang_table(r, tot.own, own);
  r += sfmt("| **TOTAL propio** | **%s** | **%s** | 100 %% |\n\n",
            mil(tot.own_files).c_str(), mil(own).c_str());
  {
    const long long fam = cpp + hh + hpp + hlegacy + cc;
    const long long fam_strict = cpp + hh + hpp;
    const long long nocpp = py + glsl + mm + msl + metal + cc + sh + lines_of(tot, "Objective-C");
    r += sfmt("- Familia C/C++ contando las `.h` heredadas: **%s → %s**.\n",
              mil(fam).c_str(), pct(fam, own).c_str());
    r += sfmt("- C++ en sentido estricto (`.cc`/`.cpp`/`.hh`/`.hpp`): **%s → %s**.\n",
              mil(fam_strict).c_str(), pct(fam_strict, own).c_str());
    r += sfmt("- Todo lo que no es C++ (Python + GLSL + ObjC++ + MSL + Metal + C + shell):\n"
              "  **%s → %s**.\n\n", mil(nocpp).c_str(), pct(nocpp, own).c_str());
  }
  r += "### Terceros vendorizados — no son de Flipendo\n\n";
  r += "Se mantienen verbatim y se actualizan desde upstream; reescribirlos a mano no es\n";
  r += "propiedad del código, es asumir el mantenimiento de bibliotecas ajenas\n";
  r += "(`LENGUAJE-CPP.md`, decisión del 2026-09-11).\n\n";
  emit_lang_table(r, tot.vend, tot.vend_lines);
  r += sfmt("| **TOTAL vendorizado** | **%s** | **%s** | 100 %% |\n\n",
            mil(tot.vend_files).c_str(), mil(tot.vend_lines).c_str());

  /* shell por shebang */
  r += "### Shell: cero `.sh` no es cero shell\n\n";
  if (shebang_files.empty()) {
    r += "Buscando por *shebang* en vez de por extensión, fuera de los vendorizados no\n";
    r += "aparece ningún fichero de shell.\n\n";
  }
  else {
    r += sfmt("Buscando por *shebang* (`git grep -l -E '^#!.*(bash|/bin/sh|zsh|ksh)'`) y no\n"
              "por extensión, fuera de los vendorizados aparecen **%zu ficheros** que ninguna\n"
              "tabla por extensión ve:\n\n", shebang_files.size());
    for (const std::string &f : shebang_files) r += sfmt("- `%s`\n", f.c_str());
    r += "\n";
  }
  r += "---\n\n";

  /* §3 Python por zonas */
  r += "## 3. El Python que queda, por zonas\n\n";
  r += "Las zonas no son una lista escrita a mano: son los directorios del árbol. Si una\n";
  r += "se vacía, desaparece sola de esta tabla.\n\n";
  {
    std::map<std::string, std::pair<long long, long long>> zones;
    for (const FileRec &f : snap.files) {
      if (f.vendored || f.lang != "Python") continue;
      auto &z = zones[py_zone(f.path)];
      z.first++;
      z.second += f.lines;
    }
    std::vector<std::pair<std::string, std::pair<long long, long long>>> rows(zones.begin(), zones.end());
    std::sort(rows.begin(), rows.end(),
              [](const auto &a, const auto &b) { return a.second.second > b.second.second; });
    r += "| Zona | Ficheros | Líneas | % del Python |\n|---|---:|---:|---:|\n";
    for (const auto &kv : rows)
      r += sfmt("| `%s` | %s | %s | %s |\n", kv.first.c_str(), mil(kv.second.first).c_str(),
                mil(kv.second.second).c_str(), pct(kv.second.second, py).c_str());
    r += sfmt("| **TOTAL** | **%s** | **%s** | 100 %% |\n\n",
              mil(files_of(tot, "Python")).c_str(), mil(py).c_str());
  }
  r += "---\n\n";

  /* §4 lo que compila */
  r += "## 4. Lo que el binario COMPILA y lo que no\n\n";
  if (!build.have_ninja) {
    r += sfmt("No se pudo leer `%s`: esta sección no se ha medido.\n\n", build.ninja_path.c_str());
  }
  else {
    r += sfmt("Medido sobre `%s` (%s aristas de objeto) y `%s`.\n\n",
              build.ninja_path.c_str(), mil(build.object_edges).c_str(), build.cache_path.c_str());
    r += "La diferencia importa: **código que no entra en el binario no es trabajo\n";
    r += "pendiente, es código muerto**, y ninguna tabla por extensión la hacía.\n\n";
    r += "| Lenguaje | Ficheros | Líneas | Con objeto en el build | Líneas que compilan |\n";
    r += "|---|---:|---:|---:|---:|\n";
    std::map<std::string, std::pair<long long, long long>> comp;   /* lenguaje -> {ficheros, líneas} compilados */
    for (const FileRec &f : snap.files) {
      if (f.vendored) continue;
      if (!build.objects.count(root + "/" + f.path)) continue;
      auto &c = comp[f.lang];
      c.first++;
      c.second += f.lines;
    }
    std::vector<std::string> interesting = {"C++", "Objective-C++", "Objective-C", "C", "Metal", "MSL", "GLSL"};
    for (const std::string &l : interesting) {
      if (!tot.own.count(l)) continue;
      if (!lang_makes_objects(l)) {
        long long fmen = 0, lmen = 0;
        for (const FileRec &f : snap.files) {
          if (f.vendored || f.lang != l) continue;
          if (build.mentioned.count(root + "/" + f.path)) { fmen++; lmen += f.lines; }
        }
        r += sfmt("| %s | %s | %s | (no produce `.o`) | %s lineas en %s ficheros citados en el build |\n", l.c_str(),
                  mil(files_of(tot, l.c_str())).c_str(), mil(lines_of(tot, l.c_str())).c_str(),
                  mil(lmen).c_str(), mil(fmen).c_str());
        continue;
      }
      r += sfmt("| %s | %s | %s | %s | **%s** |\n", l.c_str(),
                mil(files_of(tot, l.c_str())).c_str(), mil(lines_of(tot, l.c_str())).c_str(),
                mil(comp[l].first).c_str(), mil(comp[l].second).c_str());
    }
    r += "\n";

    /* Objective-C++ al detalle: es el frente abierto de la decisión del 11. */
    r += "### Objective-C++, fichero a fichero\n\n";
    std::vector<const FileRec *> mms;
    for (const FileRec &f : snap.files)
      if (!f.vendored && (f.lang == "Objective-C++" || f.lang == "Objective-C")) mms.push_back(&f);
    if (mms.empty()) {
      r += "**No queda ni un fichero `.mm` ni `.m` propio.** Objetivo cumplido.\n\n";
    }
    else {
      std::sort(mms.begin(), mms.end(), [](const FileRec *a, const FileRec *b) { return a->lines > b->lines; });
      r += "| Fichero | Líneas | ¿Produce objeto? |\n|---|---:|---|\n";
      long long dead = 0, alive = 0;
      for (const FileRec *f : mms) {
        bool o = build.objects.count(root + "/" + f->path) != 0;
        r += sfmt("| `%s` | %s | %s |\n", f->path.c_str(), mil(f->lines).c_str(),
                  o ? "**sí**" : "no");
        if (o) alive += f->lines; else dead += f->lines;
      }
      r += sfmt("\n**%s líneas de Objective-C++ propio, de las cuales %s COMPILAN y %s no.**\n\n",
                mil(alive + dead).c_str(), mil(alive).c_str(), mil(dead).c_str());
      /* ¿A qué subsistema pertenece lo que no compila, y por qué está apagado? */
      std::map<std::string, long long> by_top;
      for (const FileRec *f : mms) {
        if (build.objects.count(root + "/" + f->path)) continue;
        size_t s1 = f->path.find('/');
        size_t s2 = s1 == std::string::npos ? std::string::npos : f->path.find('/', s1 + 1);
        by_top[f->path.substr(0, s2)] += f->lines;
      }
      for (const auto &kv : by_top) {
        r += sfmt("- `%s`: %s líneas sin compilar.", kv.first.c_str(), mil(kv.second).c_str());
        if (contains(kv.first, "cycles")) {
          auto it = build.cache.find("WITH_CYCLES");
          r += sfmt(" `WITH_CYCLES` está en **%s** en `CMakeCache.txt`",
                    it == build.cache.end() ? "(no aparece)" : it->second.c_str());
          long long objs = 0, ment = 0;
          for (const auto &o : build.objects) if (contains(o, "/intern/cycles/")) objs++;
          for (const auto &m : build.mentioned) if (contains(m, "/intern/cycles/")) ment++;
          r += sfmt(" y el build no genera **ningún** objeto de `intern/cycles` (%lld objetos, "
                    "%lld ficheros fuente citados).", objs, ment);
          r += "\n  Ojo con cómo se dice: `build.ninja` **sí** nombra `intern/cycles/blender`, pero\n"
               "  como `-I` en las líneas de `INCLUDES` de otros objetivos. «Cero referencias» es\n"
               "  falso; lo cierto y lo que importa es **cero objetos compilados**.\n";
        }
        else r += "\n";
      }
      r += "\n";
    }
  }
  r += "---\n\n";

  /* §5 evolución */
  r += "## 5. Evolución, medida commit a commit\n\n";
  r += "Cada fila de estas tablas se ha medido ejecutando la misma medición sobre ese\n";
  r += "commit. No hay ni una cifra copiada de ningún documento anterior.\n\n";
  r += "Un aviso de método que cambia las cifras viejas: el `.gitattributes` que marca\n";
  r += "`extern` y `lib` como vendorizados se añadió a media historia. Para que la serie\n";
  r += "sea comparable consigo misma se imponen a TODAS las filas los prefijos\n";
  r += "vendorizados del commit de la cabecera; si no, los commits anteriores contarían\n";
  r += "las bibliotecas ajenas como código de Flipendo y la caída parecería mayor de lo\n";
  r += "que es.\n\n";
  r += "### Por días (último commit de cada día)\n\n";
  r += "| Commit | Fecha | Python (f.) | Python | C++ | ObjC++ | GLSL | `.h` | `.hpp` | C |\n";
  r += "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|\n";
  for (const HistPoint &h : daily)
    r += sfmt("| `%s` | %s | %s | **%s** | %s | %s | %s | %s | %s | %s |\n", h.shortrev.c_str(),
              h.when.c_str(), mil(h.py_files).c_str(), mil(h.py_lines).c_str(), mil(h.cpp).c_str(),
              mil(h.mm).c_str(), mil(h.glsl).c_str(), mil(h.h).c_str(), mil(h.hpp).c_str(),
              mil(h.c).c_str());
  r += "\n";
  if (daily.size() >= 2) {
    long long d = daily.front().py_lines - daily.back().py_lines;
    r += sfmt("De %s a %s líneas de Python propio: **%s%s líneas, %s** desde `%s` (%s).\n\n",
              mil(daily.front().py_lines).c_str(), mil(daily.back().py_lines).c_str(),
              d > 0 ? "−" : "+", mil(d > 0 ? d : -d).c_str(),
              pct(d > 0 ? d : -d, daily.front().py_lines).c_str(),
              daily.front().shortrev.c_str(), daily.front().when.c_str());
  }
  if (hourly.size() > 1) {
    r += sfmt("### Hora a hora del último día medido (%s)\n\n", hourly.front().when.substr(0, 10).c_str());
    r += "| Commit | Hora | Python | C++ | ObjC++ | Total propio |\n|---|---|---:|---:|---:|---:|\n";
    for (const HistPoint &h : hourly)
      r += sfmt("| `%s` | %s | %s | %s | %s | %s |\n", h.shortrev.c_str(), h.when.c_str(),
                mil(h.py_lines).c_str(), mil(h.cpp).c_str(), mil(h.mm).c_str(),
                mil(h.own_lines).c_str());
    r += "\n";
  }
  r += "---\n\n";

  /* §6 puentes */
  r += "## 6. Los puentes que quedan de C++ a Python\n\n";
  r += "No son líneas de Python: son **llamadas reales desde el C++** al intérprete. Miden\n";
  r += "lo que todavía depende de CPython aunque el `.py` ya no exista.\n\n";
  r += "| Puente | Ocurrencias | Ficheros | Qué es |\n|---|---:|---:|---|\n";
  long long bridge_total = 0;
  for (const BridgePattern &bp : kBridges) {
    long long c = bs.occ.count(bp.pat) ? bs.occ[bp.pat] : 0;
    if (!c) continue;
    bridge_total += c;
    r += sfmt("| `%s` | %s | %s | %s |\n", bp.pat, mil(c).c_str(),
              mil((long long)bs.pat_files[bp.pat].size()).c_str(), bp.what);
  }
  r += sfmt("| **TOTAL de llamadas** | **%s** | **%s** | |\n\n", mil(bridge_total).c_str(),
            mil((long long)bs.file_calls.size()).c_str());
  r += sfmt("Además, **%s líneas de preprocesador** (`#if*` con `WITH_PYTHON`) en **%s\n"
            "ficheros** esconden capacidad detrás de la compilación con Python: son los\n"
            "sitios donde el binario sin intérprete hace menos cosas. `WITH_PYTHON` aparece\n"
            "%s veces en total en el código propio.\n\n",
            mil(bs.guard_lines).c_str(), mil((long long)bs.guard_files.size()).c_str(),
            mil(bs.with_python_occ).c_str());
  {
    std::vector<std::pair<std::string, long long>> top(bs.file_calls.begin(), bs.file_calls.end());
    std::sort(top.begin(), top.end(), [](const auto &a, const auto &b) {
      if (a.second != b.second) return a.second > b.second;
      return a.first < b.first;
    });
    r += "Los ficheros que más puentes concentran (ahí es donde está el trabajo):\n\n";
    r += "| Fichero | Llamadas | ¿Lo compila el binario? |\n|---|---:|---|\n";
    for (size_t i = 0; i < top.size() && i < 15; ++i) {
      bool o = build.have_ninja && build.objects.count(root + "/" + top[i].first);
      r += sfmt("| `%s` | %s | %s |\n", top[i].first.c_str(), mil(top[i].second).c_str(),
                build.have_ninja ? (o ? "sí" : "no") : "(no medido)");
    }
    r += "\n";
  }
  r += "---\n\n";

  /* §7 batería */
  r += "## 7. La batería de verificación, ejecutada\n\n";
  if (!have_binary) {
    r += sfmt("**No medida**: no existe el binario `%s`. Compila con `nb install` y vuelve a\n"
              "generar este documento.\n\n", op.binary.c_str());
  }
  else if (!op.run_bateria) {
    r += "**No ejecutada en esta pasada** (`--sin-bateria`). Este documento está incompleto:\n";
    r += "regenéralo sin esa bandera antes de citarlo.\n\n";
  }
  else if (bat.v.empty()) {
    r += "**No medida**: no se pudo leer `source/creator/creator_args.cc` al commit.\n\n";
  }
  else {
    const long long nchecks = bat.n_checks, nselfs = bat.n_selfs;
    r += sfmt("Los verificadores se descubren leyendo las banderas `\"--fl-...\"` registradas en\n"
              "`source/creator/creator_args.cc`: **%s banderas `--fl-*` en total**, de las cuales\n"
              "**%lld** son comprobadores (`--fl-check-*` %lld + `--fl-selftest-*` %lld), **%lld**\n"
              "son volcadores `--fl-dump-*` (no dan veredicto: escriben estado) y **%lld** son\n"
              "conversores o preparadores de escena. Aquí se ejecutan **%zu** comprobaciones,\n"
              "porque `--fl-check-ui` admite dos líneas base (registro y dibujo) y son dos\n"
              "comprobaciones distintas.\n\n",
              mil(bat.fl_args_total).c_str(), bat.distinct_flags, nchecks, nselfs, bat.dumpers,
              bat.converters, bat.v.size());
    /* La cifra más importante de esta sección es de qué binario hablamos. Sin
     * esto, un rojo de otro carril parece una regresión del proyecto. */
    r += "### Contra qué binario se ha medido\n\n";
    r += sfmt("- Binario: `%s`\n", bat.binary.c_str());
    if (!bat.bin_hash.empty()) {
      const bool same = starts_with(snap.rev, bat.bin_hash) || starts_with(bat.bin_hash, snap.shortrev);
      r += sfmt("- Construido del commit `%s`%s, el %s.\n", bat.bin_hash.c_str(),
                same ? " — **el mismo que se mide arriba**" :
                       sfmt(" — **OJO: no es el commit medido (`%s`)**", snap.shortrev.c_str()).c_str(),
                bat.bin_built.c_str());
    }
    if (!bat.dirty.empty()) {
      /* No todo lo sucio puede haber entrado en el binario. Una textura de test
       * modificada no cambia una línea de código, y meterla en la misma lista que
       * un `.cc` a medias convierte el aviso en ruido: se separan, y se listan
       * una a una solo las que sí pueden cambiar lo que se está probando. */
      std::vector<std::string> code, other;
      for (const std::string &d : bat.dirty) {
        const size_t sp = d.find_last_of(' ');
        const std::string path = sp == std::string::npos ? d : d.substr(sp + 1);
        const bool is_code = (starts_with(path, "source/") || starts_with(path, "intern/") ||
                              starts_with(path, "scripts/") || starts_with(path, "extern/") ||
                              starts_with(path, "lib/") || starts_with(path, "build_files/") ||
                              ends_with(path, "CMakeLists.txt") || ends_with(path, ".cmake")) &&
                             !starts_with(path, "tests/");
        (is_code ? code : other).push_back(d);
      }
      r += sfmt("- **El árbol de trabajo tenía %zu ficheros versionados modificados sin\n"
                "  commitear cuando se midió**, de los cuales **%zu pueden cambiar el binario**\n"
                "  (fuente, cabeceras, scripts instalados o ficheros de compilación) y %zu no.\n",
                bat.dirty.size(), code.size(), other.size());
      if (!code.empty()) {
        r += "  El binario se compila del árbol de trabajo, no del commit: un rojo puede ser\n"
             "  trabajo a medias de otro carril y no una regresión del proyecto. Estaban\n"
             "  tocados:\n\n";
        size_t shown = 0;
        for (const std::string &d : code) {
          if (shown++ >= 14) { r += sfmt("  - …y %zu más\n", code.size() - 14); break; }
          r += sfmt("  - `%s`\n", d.c_str());
        }
        r += "\n";
      }
      else {
        r += "  **Ninguno de los que pueden cambiar el binario**: el veredicto de abajo es del\n"
             "  commit medido, no de trabajo a medias de nadie.\n";
      }
      if (!other.empty()) {
        std::map<std::string, long long> by_dir;
        for (const std::string &d : other) {
          const size_t sp = d.find_last_of(' ');
          std::string path = sp == std::string::npos ? d : d.substr(sp + 1);
          size_t s1 = path.find('/');
          size_t s2 = s1 == std::string::npos ? std::string::npos : path.find('/', s1 + 1);
          by_dir[path.substr(0, s2)]++;
        }
        r += sfmt("  Los otros %zu, que no entran en el binario, por directorio:", other.size());
        bool first = true;
        for (const auto &kv : by_dir) {
          r += sfmt("%s `%s` %s", first ? "" : " ·", kv.first.c_str(), mil(kv.second).c_str());
          first = false;
        }
        r += ".\n\n";
      }
    }
    else {
      r += "- El árbol de trabajo estaba limpio: el binario corresponde al commit medido.\n\n";
    }
    r += "**Un `--fl-selftest-*` no es una prueba, es un volcador**: escribe un fichero y su\n";
    r += "código de salida no es un veredicto. Contarlos como «verdes» por ese código sería un\n";
    r += "verde falso. Aquí el veredicto de un selftest se saca comparando su volcado con su\n";
    r += "línea base —honrando las declaraciones, ver más abajo—, que es lo que hace su\n";
    r += "`--fl-check-*` hermano cuando existe. Y los que en su propia ayuda dicen «Solo\n";
    r += "VUELCA: el veredicto lo da `--fl-check-X`» se marcan **volcador** y no se cuentan ni\n";
    r += "verdes ni rojos: quien firma es el comprobador.\n\n";
    {
      std::string missing;
      for (const Verifier &v : bat.v) {
        if (v.doc_baseline_missing.empty()) continue;
        missing += sfmt("- `%s` declara en su ayuda la línea base `%s`, que **no está en el\n"
                        "  árbol**. Sin ella no hay nada que comparar.\n",
                        v.label.c_str(), v.doc_baseline_missing.c_str());
      }
      if (!missing.empty()) {
        r += "**Comprobadores cuya línea base declarada no existe:**\n\n" + missing + "\n";
      }
    }
    r += "Y un rojo no se firma a la primera: **todo rojo se repite hasta tres veces**, porque\n";
    r += "el REGLAMENTO ya midió que hay resultados no deterministas (el cálculo de normales\n";
    r += "acumula en paralelo y en coma flotante). Si alguna pasada sale verde, el veredicto\n";
    r += "es **INESTABLE**, que no es lo mismo que un fallo ni que un aprobado. Excepción\n";
    r += "declarada: un verificador que tarde más de 60 s no se repite —triplicarlo se comería\n";
    r += "la batería— y su fila dice que va con una sola pasada.\n\n";
    r += sfmt("**Resultado: %lld en verde, %lld en rojo, %lld inestables, %lld sin línea base con\n"
              "la que comparar", bat.green, bat.red, bat.unstable, bat.nobase);
    if (bat.volcadores)
      r += sfmt(", %lld que su propia ayuda declara volcadores (no dan veredicto)", bat.volcadores);
    if (bat.skipped) r += sfmt(", %lld no ejecutados", bat.skipped);
    r += sfmt(".** Tardó %.0f segundos en total; el más lento, %.0f s.\n\n", bat.secs, [&] {
      double m = 0;
      for (const Verifier &v : bat.v) m = std::max(m, v.secs);
      return m;
    }());

    for (int pass = 0; pass < 2; ++pass) {
      std::vector<const Verifier *> group;
      for (const Verifier &v : bat.v) if ((int)v.gui == pass) group.push_back(&v);
      if (group.empty()) continue;
      if (pass == 0) {
        r += "### Grupo 1 — valen en `--background`\n\n";
      }
      else {
        r += "### Grupo 2 — necesitan modo gráfico\n\n";
        r += "En `--background` no se carga el keymap por defecto y los volcados de interfaz no\n";
        r += "tienen ventana: estos hay que pasarlos con pantalla, y por eso van aparte.\n\n";
      }
      r += "| Verificador | Veredicto | Tiempo | Cifras |\n|---|---|---:|---|\n";
      for (const Verifier *v : group) {
        std::string det = v->detail;
        for (auto &c : det) if (c == '|') c = '/';
        if (det.size() > 160) det = det.substr(0, 160) + "...";
        r += sfmt("| `%s` | %s | %.1f s | %s |\n", v->label.c_str(),
                  v->verdict == "VERDE" ? "verde" :
                  v->verdict == "ROJO" ? "**ROJO**" :
                  v->verdict == "INESTABLE" ? "**inestable**" :
                  v->verdict == "VOLCADOR" ? "volcador" :
                  v->verdict == "SIN LINEA BASE" ? "sin línea base" : "no ejecutado",
                  v->secs, det.empty() ? "(sin salida)" : det.c_str());
      }
      r += "\n";
      if (pass == 1) {
        for (const Verifier *v : group)
          if (!v->gui_reason.empty())
            r += sfmt("- `%s` necesita pantalla: %s.\n", v->label.c_str(), v->gui_reason.c_str());
        r += "\n";
      }
    }
    if (bat.red || bat.unstable || bat.nobase) {
      r += "### Lo que no está en verde, con su detalle\n\n";
      for (const Verifier &v : bat.v) {
        if (v.verdict == "VERDE" || v.verdict == "NO EJECUTADO" || v.verdict == "VOLCADOR")
          continue;
        r += sfmt("- **`%s`** — %s (rc=%d, modo %s)", v.label.c_str(), v.verdict.c_str(), v.rc,
                  v.mode.c_str());
        if (!v.baseline_used.empty()) r += sfmt(", línea base `%s`", v.baseline_used.c_str());
        r += sfmt(":\n  %s\n", v.detail.empty() ? "(sin detalle)" : v.detail.c_str());
      }
      r += "\n";
    }
    /* Requisito del carril ARNES: las declaraciones se cuentan y se nombran. */
    {
      std::vector<DeclaredFile> decls = scan_declarations(root);
      long long ntol = 0, ndiv = 0;
      for (DeclaredFile &d : decls) {
        (d.kind == "tolerancia" ? ntol : ndiv)++;
        for (const Verifier &v : bat.v) {
          if (v.baselines.empty() || v.baselines[0] != d.baseline) continue;
          if (v.verdict == "NO EJECUTADO") continue;
          if (!d.used_by.empty()) d.used_by += ", ";
          d.used_by += "`" + v.label + "`";
          d.passes_total += v.passes;
          if (d.kind == "tolerancia") d.passes_needed += v.tol_passes_used;
          else d.passes_needed += v.div_passes_used;
        }
        d.needed = d.passes_needed > 0;
      }
      r += "### Declaraciones honradas\n\n";
      r += sfmt("Junto a las líneas base hay **%lld tolerancia(s)** y **%lld fichero(s) de\n"
                "divergencias deliberadas**. Esta batería **los lee y los honra**: un valor\n"
                "dentro de la tolerancia declarada no es un rojo, y una divergencia declarada y\n"
                "cumplida tampoco. Al revés también: si la divergencia deja de ocurrir, es\n"
                "ROJO, porque entonces la declaración sobra.\n\n",
                ntol, ndiv);
      if (decls.empty()) {
        r += "Hoy no hay ninguna declarada.\n\n";
      }
      else {
        r += "| Fichero | Declara | Línea base | ¿Quién la usa? | ¿Hizo falta? |\n";
        r += "|---|---|---|---|---|\n";
        for (const DeclaredFile &d : decls) {
          std::string needed;
          if (d.used_by.empty()) needed = "no se ejercitó en esta pasada";
          else if (d.needed)
            needed = sfmt("**sí**, en %lld de %lld pasadas", d.passes_needed, d.passes_total);
          else needed = sfmt("**no**, en 0 de %lld pasadas", d.passes_total);
          r += sfmt("| `%s` | %s | %s | %s | %s |\n", d.path.c_str(), d.summary.c_str(),
                    d.baseline_exists ? "existe" : "**NO EXISTE**",
                    d.used_by.empty() ? "—" : d.used_by.c_str(), needed.c_str());
        }
        r += "\n";
        for (const DeclaredFile &d : decls) {
          if (d.reason.empty()) continue;
          r += sfmt("- `%s`: %s\n", d.path.c_str(), d.reason.c_str());
        }
        r += "\n";
        /* La otra mitad del encargo: una declaración que deja de hacer falta es
         * noticia, porque significa que algo cambió por debajo. Se juzga por
         * declaración y sobre TODAS las pasadas de TODOS los verificadores que la
         * usan; si uno solo la necesitó, la declaración se gana el sitio. */
        std::string sobran;
        for (const DeclaredFile &d : decls) {
          if (d.used_by.empty() || d.needed) continue;
          sobran += sfmt("- `%s`: **no hizo falta en ninguna de las %lld pasadas** de %s. O el\n"
                         "  resultado se ha vuelto determinista, o la escena ya no llega al caso\n"
                         "  que la necesitaba: en los dos casos la declaración hay que revisarla,\n"
                         "  no heredarla.\n",
                         d.path.c_str(), d.passes_total, d.used_by.c_str());
        }
        if (!sobran.empty()) {
          r += "**Declaraciones que en esta pasada no hicieron falta:**\n\n" + sobran + "\n";
        }
        else {
          r += "Todas las declaradas hicieron falta en esta pasada: ninguna sobra hoy.\n\n";
        }
      }
    }

    /* Un `--fl-check-X` y su `--fl-selftest-X` miran lo mismo por dos caminos: el
     * comprobador compara elemento a elemento mientras ejecuta, el volcador
     * escribe el fichero y aquí se compara byte a byte. Que discrepen es una
     * señal, no un empate: casi siempre significa que el resultado no es
     * determinista y que uno de los dos caminos tuvo suerte. */
    {
      std::string pairs;
      for (const Verifier &a : bat.v) {
        if (a.kind != "check" || contains(a.label, "(")) continue;
        std::string sibling = a.flag;
        sibling.replace(0, std::string("--fl-check-").size(), "--fl-selftest-");
        for (const Verifier &b : bat.v) {
          if (b.flag != sibling) continue;
          /* Solo se comparan dos VEREDICTOS. Un volcador declarado no emite
           * veredicto, y un «sin línea base» tampoco: enfrentarlos daría una
           * discrepancia inventada. */
          auto is_verdict = [](const std::string &s) {
            return s == "VERDE" || s == "ROJO" || s == "INESTABLE";
          };
          if (!is_verdict(a.verdict) || !is_verdict(b.verdict)) continue;
          if (a.verdict == b.verdict) continue;
          pairs += sfmt("- `%s` da **%s** y `%s` da **%s** sobre la misma línea base.\n",
                        a.label.c_str(), a.verdict.c_str(), b.label.c_str(), b.verdict.c_str());
        }
      }
      if (!pairs.empty()) {
        r += "### Un comprobador y su volcador que no dicen lo mismo\n\n";
        r += pairs;
        r += "\nMiran lo mismo por dos caminos —el comprobador compara mientras ejecuta, el\n"
             "volcador escribe el fichero y aquí se compara byte a byte—, así que discrepar es\n"
             "una señal: el resultado no es determinista y uno de los dos caminos tuvo suerte.\n"
             "Vale la lección del REGLAMENTO: una línea base que no se reproduce a sí misma\n"
             "tres veces seguidas no es una línea base.\n\n";
      }
    }
    r += "Cómo se pasan todos de una vez:\n\n```sh\n";
    r += "flipendo-metrics --bateria ~/Flipendo/dev/upbge          # los dos grupos\n";
    r += "flipendo-metrics --bateria --sin-grafico ~/Flipendo/dev/upbge  # solo --background\n";
    r += "```\n\n";
    r += "Sale con 0 si no hay ningún rojo, así que vale de guardián antes de un push.\n\n";
  }
  r += "---\n\n";

  /* §8 discrepancias */
  r += "## 8. Discrepancias entre lo que dicen los documentos y lo que mide el árbol\n\n";
  if (findings.empty()) {
    r += "En esta pasada no se ha encontrado ninguna. Se comprueban automáticamente la\n";
    r += "insignia de verificadores del `README.md` y el commit de la cabecera de\n";
    r += "`politicas/METRICAS.md`.\n\n";
  }
  else {
    for (const DocFinding &f : findings) {
      r += sfmt("### %s\n\n- **Decía:** %s\n- **Mide el árbol:** %s\n- **Cómo se comprobó:** %s\n\n",
                f.doc.c_str(), f.said.c_str(), f.measured.c_str(), f.how.c_str());
    }
  }
  r += "---\n\n";
  r += "## 9. Qué falta para «cero Python»\n\n";
  r += sfmt("Quedan **%s líneas de Python propio** en %s ficheros, **%s llamadas** desde C++ al\n"
            "intérprete y **%s guardas `#if*` con `WITH_PYTHON`**. El documento que ordena el\n"
            "trabajo por zonas es [`BACKLOG-EDITOR-PYTHON.md`](BACKLOG-EDITOR-PYTHON.md); la\n"
            "doctrina, [`LENGUAJE-CPP.md`](LENGUAJE-CPP.md); la historia de las mediciones,\n"
            "[`METRICAS.md`](METRICAS.md).\n\n",
            mil(py).c_str(), mil(files_of(tot, "Python")).c_str(), mil(bridge_total).c_str(),
            mil(bs.guard_lines).c_str());
  r += sfmt("<!-- generado por tools/flipendo_metrics/flipendo_metrics.cpp a %s -->\n",
            snap.rev.c_str());

  std::ofstream of(op.out, std::ios::binary);
  if (!of) { std::fprintf(stderr, "No se pudo escribir '%s'\n", op.out.c_str()); return 2; }
  of << r;
  of.close();

  std::printf("Escrito %s\n", op.out.c_str());
  std::printf("  commit %s (%s)\n", snap.shortrev.c_str(), snap.date.c_str());
  std::printf("  propio %s lineas · Python %s · ObjC++ %s · GLSL %s\n", mil(own).c_str(),
              mil(py).c_str(), mil(mm).c_str(), mil(glsl).c_str());
  if (!bat.v.empty())
    std::printf("  bateria: %lld verde, %lld rojo, %lld inestable, %lld sin linea base, "
                "%lld volcadores, %lld no ejecutados\n",
                bat.green, bat.red, bat.unstable, bat.nobase, bat.volcadores, bat.skipped);
  return 0;
}
