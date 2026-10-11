#include "platform.h"
#include <dlfcn.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace sdl {
#define WL_DEF(name) decltype(&::SDL_##name) name = nullptr;
WL_SDL_FUNCS(WL_DEF)
#undef WL_DEF

static void* g_handle = nullptr;

bool load(const std::string& exeDir, std::string& err) {
    std::vector<std::string> candidates;
    if (const char* env = getenv("WICKLIGHT_SDL")) candidates.push_back(env);
    // Prefer the system SDL2 (best integration with the user's Wayland/X11/audio setup),
    // fall back to a copy bundled next to the executable.
    candidates.push_back("libSDL2-2.0.so.0");
    candidates.push_back("libSDL2-2.0.so");
    candidates.push_back("libSDL2.so");
    candidates.push_back(exeDir + "/lib/libSDL2-2.0.so.0");
    candidates.push_back(exeDir + "/../lib/libSDL2-2.0.so.0");
    std::string tried;
    for (auto& c : candidates) {
        g_handle = dlopen(c.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (g_handle) break;
        const char* e = dlerror();
        tried += "  " + c + ": " + (e ? e : "?") + "\n";
    }
    if (!g_handle) {
        err = "Could not load the SDL2 library (libSDL2-2.0.so.0).\n\nTried:\n" + tried;
        return false;
    }
#define WL_LOAD(name)                                                              \
    name = (decltype(name))dlsym(g_handle, "SDL_" #name);                          \
    if (!name) { err = std::string("SDL2 is too old: missing SDL_") + #name; return false; }
    WL_SDL_FUNCS(WL_LOAD)
#undef WL_LOAD
    return true;
}
}  // namespace sdl

#define WL_GLDEF(type, name) type name = nullptr;
WL_GL_FUNCS(WL_GLDEF)
#undef WL_GLDEF

bool loadGL(std::string& err) {
#define WL_GLLOAD(type, name)                                                     \
    name = (type)sdl::GL_GetProcAddress(#name);                                    \
    if (!name) { err = std::string("OpenGL function missing: ") + #name; return false; }
    WL_GL_FUNCS(WL_GLLOAD)
#undef WL_GLLOAD
    return true;
}

std::string executableDir() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return ".";
    buf[n] = 0;
    std::string s(buf);
    size_t p = s.find_last_of('/');
    return p == std::string::npos ? "." : s.substr(0, p);
}

static bool commandExists(const char* name) {
    std::string cmd = std::string("command -v ") + name + " >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

void showFatalError(const std::string& title, const std::string& msg) {
    fprintf(stderr, "\n[WICKLIGHT] %s\n%s\n", title.c_str(), msg.c_str());
    // Try SDL's message box first (only if SDL is loaded), then common desktop dialog tools.
    if (sdl::ShowSimpleMessageBox && sdl::ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), msg.c_str(), nullptr) == 0)
        return;
    if (isatty(2)) return;  // launched from a terminal: stderr is enough
    std::string esc;
    for (char c : msg) { if (c == '\'') esc += "'\\''"; else esc += c; }
    std::string t = "WICKLIGHT - " + title;
    if (commandExists("zenity")) system(("zenity --error --title='" + t + "' --text='" + esc + "' >/dev/null 2>&1").c_str());
    else if (commandExists("kdialog")) system(("kdialog --title '" + t + "' --error '" + esc + "' >/dev/null 2>&1").c_str());
    else if (commandExists("xmessage")) system(("xmessage -center '" + esc + "' >/dev/null 2>&1").c_str());
}
