// flipendo — gestor de versiones del motor Flipendo (Mac Intel), en C++.
// Migrado de bin/flipendo (bash) por la doctrina C++ (politicas/LENGUAJE-CPP.md).
// Usa APIs nativas de macOS: clonefile() (clones APFS), removexattr(), statvfs().
#include <sys/clonefile.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/xattr.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static const char *C_B = "\033[1m", *C_G = "\033[32m", *C_Y = "\033[33m",
                  *C_R = "\033[31m", *C_0 = "\033[0m";

static std::string g_root, g_vdir, g_active_file;
static const std::string APPS = "/Applications";

static int die(const std::string &msg) {
  std::fprintf(stderr, "%serror:%s %s\n", C_R, C_0, msg.c_str());
  return 1;
}

static std::string read_file(const std::string &p) {
  std::ifstream f(p);
  if (!f) return "";
  std::stringstream ss; ss << f.rdbuf();
  return ss.str();
}

static std::string basename_of(const std::string &p) {
  return fs::path(p).filename().string();
}

static std::string active() {
  std::string s = read_file(g_active_file);
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
  return s.empty() ? "" : basename_of(s);
}

static bool version_exists(const std::string &name) {
  return fs::is_directory(g_vdir + "/" + name);
}

// Extrae un valor string de un JSON simple: "clave": "valor"
static std::string json_str(const std::string &json, const std::string &key) {
  const std::string pat = "\"" + key + "\"";
  size_t p = json.find(pat);
  if (p == std::string::npos) return "?";
  p = json.find(':', p);
  if (p == std::string::npos) return "?";
  p = json.find('"', p);
  if (p == std::string::npos) return "?";
  size_t q = json.find('"', p + 1);
  if (q == std::string::npos) return "?";
  return json.substr(p + 1, q - p - 1);
}

static bool json_bool(const std::string &json, const std::string &key) {
  const std::string pat = "\"" + key + "\"";
  size_t p = json.find(pat);
  if (p == std::string::npos) return false;
  size_t t = json.find("true", p);
  size_t colon = json.find(':', p);
  size_t nl = json.find('\n', p);
  return t != std::string::npos && colon != std::string::npos && t < (nl == std::string::npos ? json.size() : nl);
}

// Lee CFBundleShortVersionString de un Info.plist (XML) sin PlistBuddy.
static std::string plist_version(const std::string &plist_path) {
  std::string x = read_file(plist_path);
  size_t k = x.find("CFBundleShortVersionString");
  if (k == std::string::npos) return "?";
  size_t s = x.find("<string>", k);
  if (s == std::string::npos) return "?";
  s += 8;
  size_t e = x.find("</string>", s);
  if (e == std::string::npos) return "?";
  return x.substr(s, e - s);
}

static uintmax_t dir_size(const std::string &p) {
  uintmax_t total = 0;
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(p, fs::directory_options::skip_permission_denied, ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) break;
    if (it->is_regular_file(ec)) total += it->file_size(ec);
  }
  return total;
}

static std::string human(uintmax_t bytes) {
  const char *u[] = {"B", "K", "M", "G", "T"};
  double b = (double)bytes; int i = 0;
  while (b >= 1024.0 && i < 4) { b /= 1024.0; ++i; }
  char buf[32]; std::snprintf(buf, sizeof(buf), "%.1f%s", b, u[i]);
  return buf;
}

static bool is_protected(const std::string &name) {
  return json_bool(read_file(g_vdir + "/" + name + "/VERSION.json"), "protegido");
}

static std::vector<std::string> version_dirs() {
  std::vector<std::string> out;
  std::error_code ec;
  for (auto &e : fs::directory_iterator(g_vdir, ec)) {
    if (e.is_directory()) out.push_back(e.path().filename().string());
  }
  std::sort(out.begin(), out.end());
  return out;
}

// clona recursivamente con clonefile (APFS): coste ~0 hasta que difieran.
static bool clone_tree(const std::string &src, const std::string &dst) {
  std::error_code ec;
  fs::remove_all(dst, ec);
  return clonefile(src.c_str(), dst.c_str(), 0) == 0;
}

static void strip_quarantine(const std::string &root) {
  std::error_code ec;
  removexattr(root.c_str(), "com.apple.quarantine", 0);
  for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) break;
    removexattr(it->path().c_str(), "com.apple.quarantine", 0);
  }
}

static bool flipendo_running() {
  return std::system("pgrep -f 'Flipendo.app/Contents/MacOS/Blender' >/dev/null 2>&1") == 0;
}

static int cmd_list() {
  std::string a = active();
  std::printf("%s%-16s %-10s %-10s %-12s %s%s\n", C_B, "VERSION", "BASE", "BLENDER", "TAMANO", "ESTADO", C_0);
  for (auto &n : version_dirs()) {
    std::string j = read_file(g_vdir + "/" + n + "/VERSION.json");
    std::string u = json_str(j, "base_upbge"), b = json_str(j, "base_blender");
    std::string sz = human(dir_size(g_vdir + "/" + n));
    std::string st;
    if (n == a) st = std::string(C_G) + "<- ACTIVA" + C_0;
    if (is_protected(n)) st += std::string(" ") + C_Y + "[protegida]" + C_0;
    std::printf("%-16s %-10s %-10s %-12s %s\n", n.c_str(), u.c_str(), b.c_str(), sz.c_str(), st.c_str());
  }
  struct statvfs vfs;
  if (statvfs("/", &vfs) == 0) {
    uintmax_t freeb = (uintmax_t)vfs.f_bavail * vfs.f_frsize;
    std::printf("\nlibre en /: %s   (los snapshots comparten bloques via clones APFS)\n", human(freeb).c_str());
  }
  return 0;
}

static int cmd_snapshot(const std::string &name, const std::string &msg) {
  if (name.empty()) return die("uso: flipendo snapshot <nombre> [-m \"mensaje\"]");
  if (version_exists(name)) return die("la version '" + name + "' ya existe");
  if (!fs::is_directory(APPS + "/Flipendo.app")) return die("no encuentro /Applications/Flipendo.app");
  fs::create_directories(g_vdir + "/" + name);
  std::printf("clonando /Applications -> %s ...\n", name.c_str());
  if (!clone_tree(APPS + "/Flipendo.app", g_vdir + "/" + name + "/Flipendo.app"))
    return die("fallo al clonar Flipendo.app");
  if (fs::is_directory(APPS + "/Flipendo Player.app"))
    clone_tree(APPS + "/Flipendo Player.app", g_vdir + "/" + name + "/Flipendo Player.app");
  std::string ver = plist_version(g_vdir + "/" + name + "/Flipendo.app/Contents/Info.plist");
  std::time_t t = std::time(nullptr); char date[32];
  std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M", std::localtime(&t));
  std::ofstream j(g_vdir + "/" + name + "/VERSION.json");
  j << "{\n  \"nombre\": \"" << name << "\",\n  \"titulo\": \"Flipendo " << ver
    << "\",\n  \"fecha\": \"" << date << "\",\n  \"padre\": \"" << active()
    << "\",\n  \"version_bundle\": \"" << ver << "\",\n  \"base_upbge\": \"fork-flipendo\""
    << ",\n  \"base_blender\": \"4.5.0\",\n  \"mensaje\": \"" << msg
    << "\",\n  \"protegido\": false\n}\n";
  std::printf("%ssnapshot creado:%s %s  (coste en disco ~0, clon APFS)\n", C_G, C_0, name.c_str());
  return 0;
}

static int cmd_activate(const std::string &name) {
  if (name.empty()) return die("uso: flipendo activate <nombre>");
  if (!version_exists(name)) return die("no existe la version '" + name + "'  (flipendo list)");
  if (flipendo_running()) return die("cierra Flipendo antes de cambiar de version");
  std::string src = g_vdir + "/" + name;
  if (!fs::is_directory(src + "/Flipendo.app")) return die("la version no contiene Flipendo.app");
  std::printf("activando %s ...\n", name.c_str());
  clone_tree(src + "/Flipendo.app", APPS + "/Flipendo.app");
  if (fs::is_directory(src + "/Flipendo Player.app"))
    clone_tree(src + "/Flipendo Player.app", APPS + "/Flipendo Player.app");
  strip_quarantine(APPS + "/Flipendo.app");
  if (fs::is_directory(APPS + "/Flipendo Player.app")) strip_quarantine(APPS + "/Flipendo Player.app");
  std::ofstream(g_active_file) << src;
  std::printf("%sactiva:%s %s\n", C_G, C_0, name.c_str());
  return 0;
}

static int cmd_status() {
  std::printf("activa: %s\nraiz:   %s\n", active().c_str(), g_root.c_str());
  std::printf("%s\n", fs::is_directory(APPS + "/Flipendo.app") ? "/Applications/Flipendo.app: instalada"
                                                               : "(no instalada en /Applications)");
  return 0;
}

static int cmd_info(const std::string &name_in) {
  std::string name = name_in.empty() ? active() : name_in;
  if (!version_exists(name)) return die("no existe '" + name + "'");
  std::string j = read_file(g_vdir + "/" + name + "/VERSION.json");
  std::printf("%s\n", j.empty() ? "(sin VERSION.json)" : j.c_str());
  return 0;
}

static void help() {
  std::printf("%sflipendo%s - gestor de versiones del motor Flipendo (Mac Intel, C++)\n\n"
              "  flipendo list                    lista las versiones\n"
              "  flipendo snapshot <n> [-m msg]   congela el estado actual\n"
              "  flipendo activate <n>            instala esa version en /Applications\n"
              "  flipendo info [n]                metadatos de una version\n"
              "  flipendo status                  version activa\n\n"
              "Snapshots por clones APFS: ~0 bytes hasta que los ficheros difieren.\n",
              C_B, C_0);
}

int main(int argc, char **argv) {
  const char *home = std::getenv("HOME");
  g_root = std::string(home ? home : ".") + "/Flipendo";
  g_vdir = g_root + "/versions";
  g_active_file = g_root + "/.active";

  std::vector<std::string> a(argv + 1, argv + argc);
  std::string cmd = a.empty() ? "help" : a[0];

  if (cmd == "list" || cmd == "ls") return cmd_list();
  if (cmd == "status") return cmd_status();
  if (cmd == "info") return cmd_info(a.size() > 1 ? a[1] : "");
  if (cmd == "snapshot" || cmd == "snap") {
    std::string name = a.size() > 1 ? a[1] : "", msg;
    for (size_t i = 2; i + 1 < a.size(); ++i)
      if (a[i] == "-m") msg = a[i + 1];
    return cmd_snapshot(name, msg);
  }
  if (cmd == "activate" || cmd == "use") return cmd_activate(a.size() > 1 ? a[1] : "");
  help();
  return 0;
}
