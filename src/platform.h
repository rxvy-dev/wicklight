// WICKLIGHT - runtime-loaded SDL2 and OpenGL entry points.
// SDL2 is loaded with dlopen() so the game can show a clear message when it is
// missing, and can fall back to a bundled copy in ./lib next to the executable.
#pragma once
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <GL/glcorearb.h>
#include <string>

#define WL_SDL_FUNCS(X) \
    X(Init) X(Quit) X(GetError) X(CreateWindow) X(DestroyWindow) X(GL_SetAttribute) \
    X(GL_CreateContext) X(GL_DeleteContext) X(GL_MakeCurrent) X(GL_SetSwapInterval) \
    X(GL_SwapWindow) X(GL_GetProcAddress) X(GL_GetDrawableSize) X(PollEvent) X(GetTicks) \
    X(GetPerformanceCounter) X(GetPerformanceFrequency) X(Delay) X(SetRelativeMouseMode) \
    X(SetWindowFullscreen) X(GetWindowSize) X(ShowSimpleMessageBox) X(OpenAudioDevice) \
    X(PauseAudioDevice) X(CloseAudioDevice) X(LockAudioDevice) X(UnlockAudioDevice) \
    X(GetKeyName) X(GetScancodeName) X(GetKeyFromScancode) X(GameControllerOpen) X(GameControllerClose) \
    X(IsGameController) X(NumJoysticks) X(GameControllerGetAxis) X(GameControllerGetButton) \
    X(SetHint) X(ShowCursor) X(SetWindowMinimumSize) X(GetVersion) X(GetCurrentVideoDriver) \
    X(InitSubSystem) X(GameControllerName)

namespace sdl {
#define WL_DECL(name) extern decltype(&::SDL_##name) name;
WL_SDL_FUNCS(WL_DECL)
#undef WL_DECL
// Loads libSDL2. Returns false and fills err on failure.
bool load(const std::string& exeDir, std::string& err);
}

#define WL_GL_FUNCS(X) \
    X(PFNGLCLEARPROC, glClear) X(PFNGLCLEARCOLORPROC, glClearColor) X(PFNGLVIEWPORTPROC, glViewport) \
    X(PFNGLENABLEPROC, glEnable) X(PFNGLDISABLEPROC, glDisable) X(PFNGLBLENDFUNCPROC, glBlendFunc) \
    X(PFNGLDEPTHMASKPROC, glDepthMask) X(PFNGLDEPTHFUNCPROC, glDepthFunc) X(PFNGLCULLFACEPROC, glCullFace) \
    X(PFNGLPOLYGONOFFSETPROC, glPolygonOffset) X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLBUFFERSUBDATAPROC, glBufferSubData) X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays) X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) X(PFNGLDRAWARRAYSPROC, glDrawArrays) \
    X(PFNGLDRAWELEMENTSPROC, glDrawElements) X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv) X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLDELETESHADERPROC, glDeleteShader) X(PFNGLDELETEPROGRAMPROC, glDeleteProgram) \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) X(PFNGLUNIFORM1IPROC, glUniform1i) \
    X(PFNGLUNIFORM1FPROC, glUniform1f) X(PFNGLUNIFORM2FPROC, glUniform2f) X(PFNGLUNIFORM3FPROC, glUniform3f) \
    X(PFNGLUNIFORM4FPROC, glUniform4f) X(PFNGLUNIFORM3FVPROC, glUniform3fv) X(PFNGLUNIFORM4FVPROC, glUniform4fv) \
    X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv) X(PFNGLGENTEXTURESPROC, glGenTextures) \
    X(PFNGLBINDTEXTUREPROC, glBindTexture) X(PFNGLTEXIMAGE2DPROC, glTexImage2D) \
    X(PFNGLTEXPARAMETERIPROC, glTexParameteri) X(PFNGLACTIVETEXTUREPROC, glActiveTexture) \
    X(PFNGLDELETETEXTURESPROC, glDeleteTextures) X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) \
    X(PFNGLREADPIXELSPROC, glReadPixels) X(PFNGLPIXELSTOREIPROC, glPixelStorei) X(PFNGLGETSTRINGPROC, glGetString) \
    X(PFNGLGETERRORPROC, glGetError) X(PFNGLDRAWBUFFERPROC, glDrawBuffer) X(PFNGLREADBUFFERPROC, glReadBuffer) \
    X(PFNGLSCISSORPROC, glScissor) X(PFNGLTEXPARAMETERFVPROC, glTexParameterfv)

#define WL_GLDECL(type, name) extern type name;
WL_GL_FUNCS(WL_GLDECL)
#undef WL_GLDECL

bool loadGL(std::string& err);
std::string executableDir();
void showFatalError(const std::string& title, const std::string& msg);
