/**
 * (c) 2014-2016 Alexandro Sanchez Bach. All rights reserved.
 * Released under GPL v2 license. Read LICENSE for more details.
 */

#pragma once

#include "nucleus/graphics/target.h"
#include "nucleus/graphics/backend/opengl/opengl.h"

namespace gfx {
namespace opengl {

class OpenGLColorTarget : public ColorTarget {
public:
    // Texture ID holding this color buffer
    GLuint texture;

    // True if this target is attached to the framebuffer below
    bool attached;

    GLuint framebuffer;
    GLint drawbuffer;
};

class OpenGLDepthStencilTarget : public DepthStencilTarget {
public:
    // Texture ID holding this depth-stencil buffer
    GLuint texture;

    // True if this target is attached to the framebuffer below
    bool attached;

    GLuint framebuffer;
    GLint drawbuffer;
};

}  // namespace opengl
}  // namespace gfx
