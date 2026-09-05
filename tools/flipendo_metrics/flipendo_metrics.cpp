// flipendo_metrics — contador de composición del árbol mantenido de Flipendo.
// Regenera las métricas de migración a C++ desde el árbol. Doctrina C++.
// Uso: flipendo_metrics [raiz_del_repo]
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
namespace fs = std::filesystem;

static long long count_lines(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return 0;
  long long n = 0; char c;
  bool any = false;
  while (f.get(c)) { any = true; if (c == '\n') ++n; }
  return n + (any ? 1 : 0);
}

int main(int argc, char **argv) {
  std::string root = argc > 1 ? argv[1] : ".";
  const std::map<std::string, std::string> lang = {
    {".c","C"},{".cc","C++"},{".cpp","C++"},{".cxx","C++"},
    {".h",".h (C-style)"},{".hpp",".hpp"},{".hh",".hh (C++)"},
    {".py","Python"},{".mm","Objective-C++"},{".m","Objective-C"},
    {".glsl","GLSL"},{".msl","MSL"}};
  std::map<std::string, std::pair<long long,long long>> maint, ext;  // {files, lines}
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
    bool is_extern = sp.find("/extern/") != std::string::npos;
    auto &tgt = is_extern ? ext : maint;
    auto &slot = tgt[l->second];
    slot.first += 1;
    slot.second += count_lines(p);
  }
  auto dump = [](const char *title, std::map<std::string,std::pair<long long,long long>> &m) {
    std::printf("\n=== %s ===\n%-16s %8s %12s\n", title, "lenguaje", "ficheros", "lineas");
    for (auto &kv : m)
      std::printf("%-16s %8lld %12lld\n", kv.first.c_str(), kv.second.first, kv.second.second);
  };
  dump("Flipendo mantenido (sin extern/)", maint);
  dump("extern/ (terceros reales)", ext);
  return 0;
}
