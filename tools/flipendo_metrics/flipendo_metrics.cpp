// flipendo_metrics — contador de composición del árbol mantenido de Flipendo.
// Regenera las métricas de migración a C++ desde el árbol. Doctrina C++.
// Uso: flipendo_metrics [raiz_del_repo]
//
// AVISO (2026-09-11): esta herramienta recorre el DISCO, así que ve ficheros sin
// versionar y no ve lo que otro carril acaba de borrar. Sirve para una comprobación
// rápida. La cifra que se cita en `politicas/METRICAS.md` se mide con git a un
// commit nombrado; el método está en el §1 de ese documento.
//
// No está en ningún CMakeLists.txt (herramienta suelta). Se compila a mano:
//   c++ -std=c++17 -O2 -o ~/Flipendo/bin/flipendo-metrics \
//       ~/Flipendo/dev/upbge/tools/flipendo_metrics/flipendo_metrics.cpp
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
namespace fs = std::filesystem;

// Cuenta líneas como `wc -l`: el número de '\n'.
//
// Corregido el 2026-09-11. La versión anterior devolvía `n + (any ? 1 : 0)`, es
// decir, sumaba una línea de más en todo fichero no vacío: un fichero de tres
// líneas devolvía 4. Sobre el árbol entero, la diferencia contra `wc -l` era
// exactamente el número de ficheros de cada lenguaje (Python +610 sobre 610
// ficheros no vacíos, GLSL +745 sobre 745, .hpp +305 sobre 305...). Por ese fallo
// todas las cifras históricas del repo van ligeramente altas.
static long long count_lines(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return 0;
  long long n = 0; char c;
  while (f.get(c)) { if (c == '\n') ++n; }
  return n;
}

int main(int argc, char **argv) {
  std::string root = argc > 1 ? argv[1] : ".";
  const std::map<std::string, std::string> lang = {
    {".c","C"},{".cc","C++"},{".cpp","C++"},{".cxx","C++"},
    {".h",".h (C-style)"},{".hpp",".hpp"},{".hh",".hh (C++)"},
    {".py","Python"},{".mm","Objective-C++"},{".m","Objective-C"},
    {".glsl","GLSL"},{".msl","MSL"},{".metal","Metal (.metal)"},
    {".sh","shell"},{".bash","shell"},{".zsh","shell"}};
  std::map<std::string, std::pair<long long,long long>> maint, vend;  // {files, lines}
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) break;
    if (!it->is_regular_file(ec)) continue;
    const fs::path &p = it->path();
    std::string e = p.extension().string();
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    auto l = lang.find(e);
    if (l == lang.end()) continue;
    std::string sp = p.string();
    // Terceros vendorizados: `extern/` y `lib/`, tal y como los marca
    // .gitattributes:107-108 (linguist-vendored) y los define la «Decisión del
    // 2026-09-11» de politicas/LENGUAJE-CPP.md. La versión anterior solo
    // separaba `extern/` y metía `lib/` en el saco de Flipendo.
    bool is_vendored = sp.find("/extern/") != std::string::npos ||
                       sp.find("/lib/") != std::string::npos;
    auto &tgt = is_vendored ? vend : maint;
    auto &slot = tgt[l->second];
    slot.first += 1;
    slot.second += count_lines(p);
  }
  auto dump = [](const char *title, std::map<std::string,std::pair<long long,long long>> &m) {
    long long tf = 0, tl = 0;
    std::printf("\n=== %s ===\n%-16s %8s %12s\n", title, "lenguaje", "ficheros", "lineas");
    for (auto &kv : m) {
      std::printf("%-16s %8lld %12lld\n", kv.first.c_str(), kv.second.first, kv.second.second);
      tf += kv.second.first; tl += kv.second.second;
    }
    std::printf("%-16s %8lld %12lld\n", "TOTAL", tf, tl);
  };
  dump("Flipendo mantenido (sin extern/ ni lib/)", maint);
  dump("Vendorizado (extern/ + lib/)", vend);
  return 0;
}
