#include "platform/gl_state.hpp"

#include <raylib.h>
#include <rlgl.h>

extern "C" {
using GLFWglproc = void (*)();
GLFWglproc glfwGetProcAddress(const char* procname);
}

namespace audrop::gl {
namespace {

using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLbitfield = unsigned int;
using GLboolean = unsigned char;

constexpr GLenum kFramebuffer = 0x8D40;
constexpr GLenum kReadFramebuffer = 0x8CA8;
constexpr GLenum kDrawFramebuffer = 0x8CA9;
constexpr GLbitfield kColorBufferBit = 0x4000;
constexpr GLenum kNearest = 0x2600;
constexpr GLenum kDepthTest = 0x0B71;
constexpr GLenum kCullFace = 0x0B44;
constexpr GLenum kScissorTest = 0x0C11;
constexpr GLenum kBlend = 0x0BE2;
constexpr GLenum kSrcAlpha = 0x0302;
constexpr GLenum kOneMinusSrcAlpha = 0x0303;
constexpr GLenum kFuncAdd = 0x8006;
constexpr GLenum kArrayBuffer = 0x8892;
constexpr GLenum kTexture0 = 0x84C0;
constexpr GLenum kTexture2D = 0x0DE1;
constexpr GLenum kBack = 0x0405;
constexpr GLenum kCcw = 0x0901;

struct Fns {
    void (*BindFramebuffer)(GLenum, GLuint) = nullptr;
    void (*BlitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum) = nullptr;
    void (*UseProgram)(GLuint) = nullptr;
    void (*BindVertexArray)(GLuint) = nullptr;
    void (*BindBuffer)(GLenum, GLuint) = nullptr;
    void (*ActiveTexture)(GLenum) = nullptr;
    void (*BindTexture)(GLenum, GLuint) = nullptr;
    void (*BindSampler)(GLuint, GLuint) = nullptr;
    void (*Enable)(GLenum) = nullptr;
    void (*Disable)(GLenum) = nullptr;
    void (*BlendFunc)(GLenum, GLenum) = nullptr;
    void (*BlendEquation)(GLenum) = nullptr;
    void (*ColorMask)(GLboolean, GLboolean, GLboolean, GLboolean) = nullptr;
    void (*Viewport)(GLint, GLint, GLint, GLint) = nullptr;
    void (*ReadBuffer)(GLenum) = nullptr;
    void (*CullFace)(GLenum) = nullptr;
    void (*ClearColor)(float, float, float, float) = nullptr;
    void (*Clear)(GLbitfield) = nullptr;
    void (*FrontFace)(GLenum) = nullptr;
} fn;

bool loaded = false;

template <class F>
bool load_one(F& out, const char* name) {
    out = reinterpret_cast<F>(glfwGetProcAddress(name));
    if (!out) TraceLog(LOG_WARNING, "audrop: missing GL function %s", name);
    return out != nullptr;
}

}  // namespace

bool load() {
    bool ok = true;
    ok &= load_one(fn.BindFramebuffer, "glBindFramebuffer");
    ok &= load_one(fn.BlitFramebuffer, "glBlitFramebuffer");
    ok &= load_one(fn.UseProgram, "glUseProgram");
    ok &= load_one(fn.BindVertexArray, "glBindVertexArray");
    ok &= load_one(fn.BindBuffer, "glBindBuffer");
    ok &= load_one(fn.ActiveTexture, "glActiveTexture");
    ok &= load_one(fn.BindTexture, "glBindTexture");
    ok &= load_one(fn.BindSampler, "glBindSampler");
    ok &= load_one(fn.Enable, "glEnable");
    ok &= load_one(fn.Disable, "glDisable");
    ok &= load_one(fn.BlendFunc, "glBlendFunc");
    ok &= load_one(fn.BlendEquation, "glBlendEquation");
    ok &= load_one(fn.ColorMask, "glColorMask");
    ok &= load_one(fn.Viewport, "glViewport");
    ok &= load_one(fn.ReadBuffer, "glReadBuffer");
    ok &= load_one(fn.CullFace, "glCullFace");
    ok &= load_one(fn.FrontFace, "glFrontFace");
    ok &= load_one(fn.ClearColor, "glClearColor");
    ok &= load_one(fn.Clear, "glClear");
    loaded = ok;
    return ok;
}

void prepare_external(int framebuffer_w, int framebuffer_h) {
    if (!loaded) return;
    fn.BindFramebuffer(kFramebuffer, 0);
    fn.Disable(kBlend);
    fn.Disable(kCullFace);
    fn.Disable(kDepthTest);
    fn.Disable(kScissorTest);
    fn.ColorMask(1, 1, 1, 1);
    fn.Viewport(0, 0, framebuffer_w, framebuffer_h);
    fn.ClearColor(0, 0, 0, 1);
    fn.Clear(kColorBufferBit);
}

void blit_default_to(unsigned int target_fbo, int w, int h) {
    if (!loaded) return;
    fn.BindFramebuffer(kReadFramebuffer, 0);
    fn.ReadBuffer(kBack);
    fn.BindFramebuffer(kDrawFramebuffer, target_fbo);
    fn.BlitFramebuffer(0, 0, w, h, 0, 0, w, h, kColorBufferBit, kNearest);
    fn.BindFramebuffer(kFramebuffer, 0);
}

void restore_raylib_state(int framebuffer_w, int framebuffer_h) {
    if (!loaded) return;
    fn.BindFramebuffer(kFramebuffer, 0);
    fn.UseProgram(0);
    fn.BindVertexArray(0);
    fn.BindBuffer(kArrayBuffer, 0);
    for (GLuint unit = 0; unit < 8; ++unit) {
        fn.ActiveTexture(kTexture0 + unit);
        fn.BindTexture(kTexture2D, 0);
        fn.BindSampler(unit, 0);
    }
    fn.ActiveTexture(kTexture0);
    fn.Disable(kDepthTest);
    fn.Disable(kScissorTest);
    fn.Enable(kCullFace);
    fn.CullFace(kBack);
    fn.FrontFace(kCcw);
    fn.Enable(kBlend);
    fn.BlendFunc(kSrcAlpha, kOneMinusSrcAlpha);
    fn.BlendEquation(kFuncAdd);
    fn.ColorMask(1, 1, 1, 1);
    fn.Viewport(0, 0, framebuffer_w, framebuffer_h);
    // rlgl caches the blend mode; bounce it so the next draw re-applies it.
    rlSetBlendMode(RL_BLEND_ADDITIVE);
    rlSetBlendMode(RL_BLEND_ALPHA);
}

}  // namespace audrop::gl
