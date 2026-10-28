#pragma once

// Raw OpenGL calls needed to share raylib's context with an external renderer (projectM).
// Functions are loaded through GLFW (bundled inside raylib), so this works without a GL loader.
namespace audrop::gl {

bool load();

// Neutral state for an external renderer drawing into the default framebuffer: no blending,
// culling, depth or scissor, and the framebuffer cleared to opaque black so nothing from the
// previous frame can bleed into its output.
void prepare_external(int framebuffer_w, int framebuffer_h);

// Copies the bottom-left w x h region of the default framebuffer into `target_fbo`.
void blit_default_to(unsigned int target_fbo, int w, int h);

// Puts the GL state back the way raylib's rlgl expects after another renderer used the context.
void restore_raylib_state(int framebuffer_w, int framebuffer_h);

}  // namespace audrop::gl
