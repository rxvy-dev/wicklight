// WICKLIGHT - entry point: window, input, main loop
#include "game.h"
#include "platform.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdlib>
#include <cmath>

int runAutotest(Game& g, const std::string& which);  // bot.cpp
int runShowcase(Game& g, Renderer& R, const std::string& out, int W, int H);  // showcase.cpp
int surveyIsland(uint32_t seed);

static void usage() {
    printf("WICKLIGHT - a native Linux 3D pixel game\n"
           "  --windowed / --fullscreen     override display mode\n"
           "  --width N --height N          window size (default 1280x720)\n"
           "  --headless --autotest [all]   run the automated campaign test (no window)\n"
           "  --screenshot FILE --frames N  render N frames then save a PPM screenshot\n"
           "  --level L --spawn S           start directly in a level (debug)\n"
           "  --version\n");
}

int main(int argc, char** argv) {
    bool headless = false, autotest = false;
    std::string autotestWhich = "all", shotPath, showcasePath;
    int shotFrames = 0, startLevel = -1, startSpawn = 0, width = 1280, height = 720, forceFs = -1;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--headless") headless = true;
        else if (a == "--autotest") { autotest = true; if (i + 1 < argc && argv[i + 1][0] != '-') autotestWhich = next(); }
        else if (a == "--screenshot") shotPath = next();
        else if (a == "--showcase") { showcasePath = next(); forceFs = 0; }
        else if (a == "--survey") return surveyIsland((uint32_t)strtoul(next().c_str(), nullptr, 10));
        else if (a == "--frames") shotFrames = atoi(next().c_str());
        else if (a == "--level") startLevel = atoi(next().c_str());
        else if (a == "--spawn") startSpawn = atoi(next().c_str());
        else if (a == "--width") width = atoi(next().c_str());
        else if (a == "--height") height = atoi(next().c_str());
        else if (a == "--windowed") forceFs = 0;
        else if (a == "--fullscreen") forceFs = 1;
        else if (a == "--version") { printf("WICKLIGHT 1.0.1\n"); return 0; }
        else if (a == "--help" || a == "-h") { usage(); return 0; }
    }

    if (headless) {
        if (!getenv("WICKLIGHT_DATA_DIR")) setenv("WICKLIGHT_DATA_DIR", "/tmp/wicklight-autotest", 1);
        Game g;
        g.init(nullptr, true);
        if (autotest) return runAutotest(g, autotestWhich);
        printf("--headless needs --autotest\n");
        return 1;
    }

    std::string err;
    std::string exeDir = executableDir();
    if (!sdl::load(exeDir, err)) {
        showFatalError("Missing SDL2", err +
            "\nInstall SDL2 with your package manager:\n"
            "  Arch/Manjaro:   sudo pacman -S sdl2   (or sdl2-compat)\n"
            "  Debian/Ubuntu:  sudo apt install libsdl2-2.0-0\n"
            "  Fedora:         sudo dnf install SDL2\n"
            "  openSUSE:       sudo zypper install libSDL2-2_0-0\n"
            "  Void:           sudo xbps-install SDL2\n"
            "  Gentoo:         sudo emerge media-libs/libsdl2\n"
            "  NixOS:          nix run . (see README)\n");
        return 1;
    }
    sdl::SetHint("SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR", "0");
    if (sdl::Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        showFatalError("Video init failed", std::string("SDL could not start video: ") + sdl::GetError() +
                       "\nMake sure you are running inside an X11 or Wayland session.");
        return 1;
    }
    sdl::GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    sdl::GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    sdl::GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    sdl::GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    sdl::GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    Game game;
    game.initPaths();
    game.loadSettings();
    bool fs = forceFs >= 0 ? forceFs == 1 : game.settings.fullscreen;
    SDL_Window* win = sdl::CreateWindow("WICKLIGHT", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (fs ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!win) { showFatalError("Window creation failed", sdl::GetError()); return 1; }
    sdl::SetWindowMinimumSize(win, 640, 360);
    SDL_GLContext ctx = sdl::GL_CreateContext(win);
    if (!ctx) {
        showFatalError("OpenGL 3.3 not available", std::string(sdl::GetError()) +
                       "\nWICKLIGHT needs OpenGL 3.3. Update your graphics drivers (Mesa 10+ / NVIDIA / AMD).");
        return 1;
    }
    sdl::GL_MakeCurrent(win, ctx);
    sdl::GL_SetSwapInterval(game.settings.vsync ? 1 : 0);
    if (!loadGL(err)) { showFatalError("OpenGL error", err); return 1; }
    fprintf(stderr, "[wicklight] GL: %s | %s | video: %s\n", (const char*)glGetString(GL_RENDERER), (const char*)glGetString(GL_VERSION), sdl::GetCurrentVideoDriver());

    Renderer R;
    int dw, dh; sdl::GL_GetDrawableSize(win, &dw, &dh);
    R.winW = dw; R.winH = dh;
    R.pixelLines = PIXEL_LINES[game.settings.pixelMode];
    if (!showcasePath.empty()) { R.pixelLines = dh > dw ? 480 : 270; game.settings.outlines = true; game.settings.fov = dh > dw ? 78 : 66; }
    R.shadowQuality = game.settings.shadows;
    if (!R.init(err)) { showFatalError("Renderer failed", err); return 1; }
    if (!showcasePath.empty()) audio::initOffline();
    else if (!audio::init()) fprintf(stderr, "[wicklight] continuing without sound\n");
    game.init(&R, false);
    if (!showcasePath.empty()) { sdl::GL_SetSwapInterval(0); int rc = runShowcase(game, R, showcasePath, dw, dh); sdl::GL_DeleteContext(ctx); sdl::DestroyWindow(win); sdl::Quit(); return rc; }
    if (startLevel >= 0 && startLevel < LV_COUNT) { game.loadGame(); game.enterLevel((LevelId)startLevel, startSpawn); game.screen = SC_PLAY; if (getenv("WICKLIGHT_DEBUG_SHOP")) game.screen = SC_SHOP; if (getenv("WICKLIGHT_DEBUG_SCREEN")) game.screen = (Screen)atoi(getenv("WICKLIGHT_DEBUG_SCREEN"));
        if (const char* dp = getenv("WICKLIGHT_DEBUG_POS")) { float x, y, z, yw, pt = -0.2f; if (sscanf(dp, "%f,%f,%f,%f,%f", &x, &y, &z, &yw, &pt) >= 4) { game.pl.pos = game.pl.safePos = V3(x, y, z); game.camYaw = game.pl.yaw = yw; game.camPitch = pt; game.camTarget = game.pl.pos + V3(0, 1.65f, 0); } }
        if (getenv("WICKLIGHT_DEBUG_BOSS") && game.lv.boss.type != B_NONE) { game.pl.pos = game.lv.boss.center + V3(-3, 0, -9); game.camYaw = 0.3f; game.startBoss(); } }

    SDL_GameController* pad = nullptr;
    for (int i = 0; i < sdl::NumJoysticks(); i++) if (sdl::IsGameController(i)) { pad = sdl::GameControllerOpen(i); break; }

    InputState in;
    bool down[A_COUNT] = {};
    Uint64 last = sdl::GetPerformanceCounter();
    double freq = (double)sdl::GetPerformanceFrequency();
    int frame = 0;
    bool relMouse = false;
    bool padPrev[SDL_CONTROLLER_BUTTON_MAX] = {};

    while (!game.quit) {
        in.clearEdges();
        float lookDX = 0, lookDY = 0;
        game.mouseClick = false; game.mouseMoved = false;
        SDL_Event e;
        while (sdl::PollEvent(&e)) {
            if (e.type == SDL_QUIT) game.quit = true;
            else if (e.type == SDL_WINDOWEVENT && (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_RESIZED)) {
                sdl::GL_GetDrawableSize(win, &dw, &dh); R.resize(dw, dh);
            } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST && game.screen == SC_PLAY) {
                game.screen = SC_PAUSE; game.menuSel = 0;
            } else if (e.type == SDL_MOUSEMOTION) {
                if (relMouse) { lookDX += e.motion.xrel; lookDY += e.motion.yrel; }
                int ww, wh; sdl::GetWindowSize(win, &ww, &wh);
                game.mouseX = e.motion.x * (float)dw / ww; game.mouseY = e.motion.y * (float)dh / wh; game.mouseMoved = true;
            } else if (e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_KEYUP || e.type == SDL_MOUSEBUTTONUP) {
                bool isDown = e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN;
                int code = (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) ? (int)e.key.keysym.scancode : 1000 + e.button.button;
                bool repeat = (e.type == SDL_KEYDOWN) && e.key.repeat;
                if (game.rebinding && isDown && !repeat) {
                    game.finishRebind(code == SDL_SCANCODE_ESCAPE ? -1 : code);
                    continue;
                }
                if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT && game.screen != SC_PLAY) game.mouseClick = true;
                if (isDown && !repeat) {
                    int sc = code;
                    if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W) in.mUp = true;
                    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S) in.mDown = true;
                    if (sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_A) in.mLeft = true;
                    if (sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_D) in.mRight = true;
                    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_SPACE) in.mOk = true;
                    if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_BACKSPACE) in.mBack = true;
                    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) in.numKey = sc - SDL_SCANCODE_1 + 1;
                    if (sc == SDL_SCANCODE_F11) { game.settings.fullscreen = !game.settings.fullscreen; game.displayDirty = true; }
                    if (sc == SDL_SCANCODE_F12) R.screenshot(game.saveDir + "/screenshot.ppm");
                } else if (isDown && repeat) {
                    int sc = code;
                    if (sc == SDL_SCANCODE_UP) in.mUp = true;
                    if (sc == SDL_SCANCODE_DOWN) in.mDown = true;
                }
                for (int a = 0; a < A_COUNT; a++) {
                    bool match = game.settings.keys[a] == code || (a == A_PAUSE && code == SDL_SCANCODE_ESCAPE);
                    if (!match || repeat) continue;
                    if (isDown && !down[a]) in.pressed[a] = true;
                    if (!isDown && down[a]) in.released[a] = true;
                    down[a] = isDown;
                }
            } else if (e.type == SDL_MOUSEWHEEL) {
                in.wheel += e.wheel.y > 0 ? -1 : (e.wheel.y < 0 ? 1 : 0);
            } else if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) {
                pad = sdl::GameControllerOpen(e.cdevice.which);
            }
        }
        // gamepad
        float padX = 0, padY = 0;
        bool padDown[A_COUNT] = {};
        if (pad) {
            auto ax = [&](SDL_GameControllerAxis a) { float v = sdl::GameControllerGetAxis(pad, a) / 32767.f; return std::fabs(v) < 0.18f ? 0.f : v; };
            padX = ax(SDL_CONTROLLER_AXIS_LEFTX); padY = -ax(SDL_CONTROLLER_AXIS_LEFTY);
            lookDX += ax(SDL_CONTROLLER_AXIS_RIGHTX) * 14; lookDY += ax(SDL_CONTROLLER_AXIS_RIGHTY) * 10;
            padDown[A_THROW] = ax(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 0.4f;
            padDown[A_WARP] = ax(SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 0.4f;
            auto bt = [&](SDL_GameControllerButton b) { return sdl::GameControllerGetButton(pad, b) != 0; };
            padDown[A_JUMP] = bt(SDL_CONTROLLER_BUTTON_A); padDown[A_DASH] = bt(SDL_CONTROLLER_BUTTON_B);
            padDown[A_BLOOM] = bt(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); padDown[A_INTERACT] = bt(SDL_CONTROLLER_BUTTON_X);
            padDown[A_PAUSE] = bt(SDL_CONTROLLER_BUTTON_START); padDown[A_JOURNAL] = bt(SDL_CONTROLLER_BUTTON_BACK);
            static bool padPrevA[A_COUNT] = {};
            for (int a = 0; a < A_COUNT; a++) {
                if (padDown[a] && !padPrevA[a]) in.pressed[a] = true;
                if (!padDown[a] && padPrevA[a]) in.released[a] = true;
                padPrevA[a] = padDown[a];
            }
            bool dpad[4] = {bt(SDL_CONTROLLER_BUTTON_DPAD_UP), bt(SDL_CONTROLLER_BUTTON_DPAD_DOWN), bt(SDL_CONTROLLER_BUTTON_DPAD_LEFT), bt(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)};
            if (dpad[0] && !padPrev[0]) in.mUp = true;
            if (dpad[1] && !padPrev[1]) in.mDown = true;
            if (dpad[2] && !padPrev[2]) in.mLeft = true;
            if (dpad[3] && !padPrev[3]) in.mRight = true;
            if (padDown[A_JUMP] && !padPrev[4]) in.mOk = true;
            if (padDown[A_DASH] && !padPrev[5]) in.mBack = true;
            { static bool lbPrev = false; bool lb = bt(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); if (lb && !lbPrev) in.wheel += 1; lbPrev = lb; }
            padPrev[0] = dpad[0]; padPrev[1] = dpad[1]; padPrev[2] = dpad[2]; padPrev[3] = dpad[3]; padPrev[4] = padDown[A_JUMP]; padPrev[5] = padDown[A_DASH];
        }
        for (int a = 0; a < A_COUNT; a++) in.down[a] = down[a] || padDown[a];
        in.moveX = (in.down[A_RIGHT] ? 1.f : 0.f) - (in.down[A_LEFT] ? 1.f : 0.f) + padX;
        in.moveY = (in.down[A_FWD] ? 1.f : 0.f) - (in.down[A_BACK] ? 1.f : 0.f) + padY;
        in.moveX = clampf(in.moveX, -1, 1); in.moveY = clampf(in.moveY, -1, 1);

        bool wantRel = game.screen == SC_PLAY && game.dialog.empty();
        if (wantRel != relMouse) { sdl::SetRelativeMouseMode(wantRel ? SDL_TRUE : SDL_FALSE); relMouse = wantRel; lookDX = lookDY = 0; }

        Uint64 now = sdl::GetPerformanceCounter();
        float dt = (float)((now - last) / freq);
        last = now;
        if (!shotPath.empty()) dt = 1.f / 60;
        game.frame(dt, in, lookDX, lookDY);
        if (game.displayDirty) {
            game.displayDirty = false;
            sdl::SetWindowFullscreen(win, game.settings.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            sdl::GL_SetSwapInterval(game.settings.vsync ? 1 : 0);
            sdl::GL_GetDrawableSize(win, &dw, &dh); R.resize(dw, dh);
        }
        game.draw();
        frame++;
        if (!shotPath.empty() && frame >= shotFrames) { R.screenshot(shotPath); fprintf(stderr, "[wicklight] screenshot -> %s\n", shotPath.c_str()); break; }
        sdl::GL_SwapWindow(win);
        if (!game.settings.vsync) {
            float target = 1.f / 144;
            float spent = (float)((sdl::GetPerformanceCounter() - now) / freq);
            if (spent < target) sdl::Delay((Uint32)((target - spent) * 1000));
        }
    }
    if (game.screen == SC_PLAY || game.screen == SC_PAUSE) game.writeSave();
    audio::shutdown();
    if (pad) sdl::GameControllerClose(pad);
    sdl::GL_DeleteContext(ctx);
    sdl::DestroyWindow(win);
    sdl::Quit();
    return 0;
}
