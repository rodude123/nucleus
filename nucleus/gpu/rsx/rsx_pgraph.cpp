/**
 * (c) 2014-2016 Alexandro Sanchez Bach. All rights reserved.
 * Released under GPL v2 license. Read LICENSE for more details.
 */

#include "rsx_pgraph.h"
#include "nucleus/assert.h"
#include "nucleus/emulator.h"
#include "nucleus/logger/logger.h"
#include "nucleus/gpu/rsx/rsx.h"
#include "nucleus/gpu/rsx/rsx_convert.h"
#include "nucleus/gpu/rsx/rsx_enum.h"
#include "nucleus/gpu/rsx/rsx_methods.h"

namespace gpu {
namespace rsx {

PGRAPH::PGRAPH(std::shared_ptr<gfx::IBackend> graphics, RSX* rsx, mem::Memory* memory) :
    graphics(std::move(graphics)), rsx(rsx), memory(memory), surface() {
}

PGRAPH::~PGRAPH() {
}

U64 PGRAPH::HashTexture() {
    return 0;
}

U64 PGRAPH::HashVertexProgram(rsx_vp_instruction_t* program) {
    // 64-bit Fowler/Noll/Vo FNV-1a hash code
    U64 hash = 0xCBF29CE484222325ULL;
    do {
        hash ^= program->dword[0];
        hash += (hash << 1) + (hash << 4) + (hash << 5) + (hash << 7) + (hash << 8) + (hash << 40);
        hash ^= program->dword[1];
        hash += (hash << 1) + (hash << 4) + (hash << 5) + (hash << 7) + (hash << 8) + (hash << 40);
    } while (!(program++)->end);
    return hash;
}

U64 PGRAPH::HashFragmentProgram(rsx_fp_instruction_t* program) {
    // 64-bit Fowler/Noll/Vo FNV-1a hash code
    bool end = false;
    U64 hash = 0xCBF29CE484222325ULL;
    do {
        hash ^= program->dword[0];
        hash += (hash << 1) + (hash << 4) + (hash << 5) + (hash << 7) + (hash << 8) + (hash << 40);
        hash ^= program->dword[1];
        hash += (hash << 1) + (hash << 4) + (hash << 5) + (hash << 7) + (hash << 8) + (hash << 40);
        end = ((program++)->word[0] >> 8) & 0x1; // NOTE: We can't acces program->end directly, since words require byte swapping
    } while (!end);
    return hash;
}

void PGRAPH::LoadVertexAttributes(U32 first, U32 count) {
    // Bytes per vertex coordinate. Index is given by attribute::type.
    static const U32 vertexTypeSize[] = {
        0, 2, 4, 2, 1, 2, 4, 1
    };

    for (auto& attr : vpe.attr) {
        if (!attr.size) {
            continue;
        }

        // Get vertex buffer address
        U32 addr;
        if (attr.location == RSX_LOCATION_LOCAL) {
            addr = nucleus.memory->getSegment(mem::SEG_RSX_LOCAL_MEMORY).getBaseAddr() + attr.offset;
        } else {
            addr = rsx->get_ea(attr.offset);
        }

        const U32 typeSize = vertexTypeSize[attr.type];
        attr.data.resize(count * attr.size * typeSize);

        // Copy data per vertex
        for (U32 i = 0; i < count; i++) {
            U32 src = addr + vertex_data_base_offset + attr.stride * (first + i + vertex_data_base_index);
            void* dst = &attr.data[i * attr.size * typeSize];

            switch (typeSize) {
            case 1:
                for (U8 j = 0; j < attr.size; j++) {
                    ((U8*)dst)[j] = nucleus.memory->read8(src + 1*j);
                }
                break;
            case 2:
                for (U8 j = 0; j < attr.size; j++) {
                    ((U16*)dst)[j] = nucleus.memory->read16(src + 2*j);
                }
                break;
            case 4:
                for (U8 j = 0; j < attr.size; j++) {
                    ((U32*)dst)[j] = nucleus.memory->read32(src + 4*j);
                }
                break;
            }
        }
    }
}

gfx::ColorTarget* PGRAPH::getColorTarget(U32 address) {
    if (colorTargets.find(address) != colorTargets.end()) {
        return colorTargets[address];
    }
    // Generate a texture to hold the color buffer
    gfx::TextureDesc desc = {};
    desc.mipmapLevels = 1;
    desc.width = surface.width;
    desc.height = surface.height;
    desc.format = convertFormat(surface.colorFormat);
    auto* texture = graphics->createTexture(desc);

    auto* target = graphics->createColorTarget(texture);
    colorTargets[address] = target;
    return target;
}

gfx::DepthStencilTarget* PGRAPH::getDepthStencilTarget(U32 address) {
    if (depthStencilTargets.find(address) != depthStencilTargets.end()) {
        return depthStencilTargets[address];
    }
    // Generate a texture to hold the depth buffer
    gfx::TextureDesc desc = {};
    desc.mipmapLevels = 1;
    desc.width = surface.width;
    desc.height = surface.height;
    desc.format = convertFormat(surface.depthFormat);
    auto* texture = graphics->createTexture(desc);

    auto* target = graphics->createDepthStencilTarget(texture);
    depthStencilTargets[address] = target;
    return target;
}

void PGRAPH::setSurface() {
    if (!surface.dirty) {
        return;
    }    
    Size colorCount = 0;
    gfx::ColorTarget* colors[4];
    gfx::DepthStencilTarget* depth = getDepthStencilTarget(surface.depthOffset);

    switch (surface.colorTarget) {
    case RSX_SURFACE_TARGET_NONE:
        colorCount = 0;
        break;
    case RSX_SURFACE_TARGET_0:
        colors[0] = getColorTarget(surface.colorOffset[0]);
        colorCount = 1;
        break;
    case RSX_SURFACE_TARGET_1:
        colors[1] = getColorTarget(surface.colorOffset[1]);
        colorCount = 1;
        break;
    case RSX_SURFACE_TARGET_MRT1:
        colors[0] = getColorTarget(surface.colorOffset[0]);
        colors[1] = getColorTarget(surface.colorOffset[1]);
        colorCount = 2;
        break;
    case RSX_SURFACE_TARGET_MRT2:
        colors[0] = getColorTarget(surface.colorOffset[0]);
        colors[1] = getColorTarget(surface.colorOffset[1]);
        colors[2] = getColorTarget(surface.colorOffset[2]);
        colorCount = 3;
        break;
    case RSX_SURFACE_TARGET_MRT3:
        colors[0] = getColorTarget(surface.colorOffset[0]);
        colors[1] = getColorTarget(surface.colorOffset[1]);
        colors[2] = getColorTarget(surface.colorOffset[2]);
        colors[3] = getColorTarget(surface.colorOffset[3]);
        colorCount = 4;
        break;
    default:
        assert_always("Unexpected");
    }
    cmdBuffer->cmdSetTargets(colorCount, colors, depth);
    surface.dirty = false;
}

void PGRAPH::setViewport() {
    if (!viewport.dirty) {
        return;
    }
    gfx::Viewport rectangle = { viewport.x, viewport.y, viewport.width, viewport.height };
    cmdBuffer->cmdSetViewports(1, &rectangle);
    viewport.dirty = false;
}

void PGRAPH::setPipeline() {
    // TODO: Hash pipeline and retrieve it from cache
    if (0) {
        cmdBuffer->cmdBindPipeline(nullptr);
    }

    auto vpData = &vpe.data[vpe.start];
    auto vpHash = HashVertexProgram(vpData);
    if (cacheVP.find(vpHash) == cacheVP.end()) {
        RSXVertexProgram vp;
        vp.decompile(vpData);
        vp.compile();
        cacheVP[vpHash] = vp;
    }
    auto fpData = memory->ptr<rsx_fp_instruction_t>((fp_location ? rsx->get_ea(0x0) : 0xC0000000) + fp_offset);
    auto fpHash = HashFragmentProgram(fpData);
    if (cacheFP.find(fpHash) == cacheFP.end()) {
        RSXFragmentProgram fp;
        fp.decompile(fpData);
        fp.compile();
        cacheFP[fpHash] = fp;
    }

    // Shaders
    gfx::PipelineDesc pipelineDesc = {};
    pipelineDesc.vs = cacheVP[vpHash].shader;
    pipelineDesc.ps = cacheFP[fpHash].shader;

    auto* pipeline = graphics->createPipeline(pipelineDesc);
    cmdBuffer->cmdBindPipeline(pipeline);
}



/*GLuint PGRAPH::GetColorTarget(U32 address) {
    if (colorTargets.find(address) == colorTargets.end()) {
        return 0;
    }
    return colorTargets[address];
}*/

/**
 * PGRAPH methods
 */
void PGRAPH::AlphaFunc(U32 func, F32 ref) {
    /*glAlphaFunc(func, ref);
    checkRendererError("AlphaFunc");*/
}

// NOTE: RSX doesn't know how big the vertex buffer is, but OpenGL requires this information
// to copy the data to the host GPU. Therefore, LoadVertexAttributes needs to be called.
void PGRAPH::BindVertexAttributes() {
    /*// Indices are attr.type
    static const GLenum vertexType[] = {
        0, GL_SHORT, GL_FLOAT, GL_HALF_FLOAT, GL_UNSIGNED_BYTE, GL_SHORT, GL_FLOAT, GL_UNSIGNED_BYTE
    };
    static const GLboolean vertexNormalized[] = {
        0, GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE
    };

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    for (int index = 0; index < 16; index++) {
        const auto& attr = vpe.attr[index];
        if (attr.size) {
            GLuint vbo;
            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, attr.data.size(), attr.data.data(), GL_STATIC_DRAW);
            glEnableVertexAttribArray(index);
            glVertexAttribPointer(index, attr.size, vertexType[attr.type], vertexNormalized[attr.type], 0, 0);
        }
    }*/
}

void PGRAPH::Begin(U32 mode) {
    vertex_primitive = mode;
}

void PGRAPH::ClearSurface(U32 mask) {
    // Avoid clearing empty surfaces
    if (surface.width == 0 || surface.height == 0 || surface.colorFormat == 0) {
        return;
    }
    const F32 color[4] = {
        ((clear_color >> 24) & 0xFF) / 255.0f, // Red
        ((clear_color >> 16) & 0xFF) / 255.0f, // Green
        ((clear_color >>  8) & 0xFF) / 255.0f, // Blue
        ((clear_color >>  0) & 0xFF) / 255.0f, // Alpha
    };
    const F32 depth = clear_depth / F32(0xFFFFFF);
    const U8 stencil = clear_stencil;

    auto* colorTarget = getColorTarget(surface.colorOffset[0]);
    if (mask & RSX_CLEAR_BIT_COLOR) {
        cmdBuffer->cmdClearColor(colorTarget, color);
    }

    auto* depthTarget = getDepthStencilTarget(surface.depthOffset);
    if (mask & (RSX_CLEAR_BIT_DEPTH | RSX_CLEAR_BIT_STENCIL)) {
        cmdBuffer->cmdClearDepthStencil(depthTarget, depth, stencil);
    } else {
        assert_true((mask & RSX_CLEAR_BIT_DEPTH) == 0, "Unimplemented depth-exclusive clear");
        assert_true((mask & RSX_CLEAR_BIT_STENCIL) == 0, "Unimplemented depth-exclusive clear");
    }
}

void PGRAPH::ColorMask(bool a, bool r, bool g, bool b) {
    /*glColorMask(r, g, b, a);
    checkRendererError("ColorMask");*/
}

void PGRAPH::DepthFunc(U32 func) {
    /*glDepthFunc(func);
    checkRendererError("DepthFunc");*/
}

void PGRAPH::DrawArrays(U32 first, U32 count) {
    // State
    //glBlendFuncSeparate(blend_sfactor_rgb, blend_dfactor_rgb, blend_sfactor_alpha, blend_dfactor_alpha);
    setPipeline();
    setSurface();
    setViewport();

    // Viewport
    /**/

    // Upload VP constants
    /*for (U32 i = 0; i < 468; i++) {
        auto& constant = vpe.constant[i];
        if (constant.dirty) {
            GLint loc = glGetUniformLocation(id, format("c[%d]", i).c_str());
            glUniform4f(loc, constant.x, constant.y, constant.z, constant.w);
            constant.dirty = false;
        }
    }*/

    // Bind textures
    /*for (U32 i = 0; i < RSX_MAX_TEXTURES; i++) {
        const auto& tex = texture[i];
        if (tex.enable) {
            GLuint tid;
            glActiveTexture(GL_TEXTURE0 + i);
            glGenTextures(1, &tid);
            glBindTexture(GL_TEXTURE_2D, tid);

            // Init texture
            void* texaddr = memory->ptr<void>((tex.location ? nucleus.rsx.get_ea(0x0) : 0xC0000000) + tex.offset);
            switch (tex.format & ~RSX_TEXTURE_LN & ~RSX_TEXTURE_UN) {
            case RSX_TEXTURE_B8:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.width, tex.height, 0, GL_BLUE, GL_UNSIGNED_BYTE, texaddr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_BLUE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_BLUE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                break;
            case RSX_TEXTURE_A8R8G8B8:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.width, tex.height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, texaddr);
                break;
            default:
                logger.error(LOG_GPU, "Unsupported texture format (%d)", tex.format);
            }

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }
    }*/

    /*GLenum mode = vertex_primitive - 1;
    glDrawArrays(mode, first, count);
    checkRendererError("DrawArrays");*/
}

void PGRAPH::Enable(U32 prop, U32 enabled) {
    /*switch (prop) {
    case NV4097_SET_DITHER_ENABLE:
        enabled ? glEnable(GL_DITHER) : glDisable(GL_DITHER);
        break;

    case NV4097_SET_ALPHA_TEST_ENABLE:
        enabled ? glEnable(GL_ALPHA_TEST) : glDisable(GL_ALPHA_TEST);
        break;

    case NV4097_SET_STENCIL_TEST_ENABLE:
        enabled ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST);
        break;

    case NV4097_SET_DEPTH_TEST_ENABLE:
        enabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
        break;

    case NV4097_SET_CULL_FACE_ENABLE:
        enabled ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
        break;

    case NV4097_SET_BLEND_ENABLE:
        enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
        break;

    case NV4097_SET_POLY_OFFSET_FILL_ENABLE:
        enabled ? glEnable(GL_POLYGON_OFFSET_FILL) : glDisable(GL_POLYGON_OFFSET_FILL);
        break;

    case NV4097_SET_POLY_OFFSET_LINE_ENABLE:
        enabled ? glEnable(GL_POLYGON_OFFSET_LINE) : glDisable(GL_POLYGON_OFFSET_LINE);
        break;

    case NV4097_SET_POLY_OFFSET_POINT_ENABLE:
        enabled ? glEnable(GL_POLYGON_OFFSET_POINT) : glDisable(GL_POLYGON_OFFSET_POINT);
        break;

    case NV4097_SET_LOGIC_OP_ENABLE:
        // TODO: Nsight dislikes this
        //enabled ? glEnable(GL_LOGIC_OP) : glDisable(GL_LOGIC_OP);
        break;

    case NV4097_SET_SPECULAR_ENABLE:
        // TODO: Nsight dislikes this
        //enabled ? glEnable(GL_LIGHTING) : glDisable(GL_LIGHTING);
        break;

    case NV4097_SET_LINE_SMOOTH_ENABLE:
        enabled ? glEnable(GL_LINE_SMOOTH) : glDisable(GL_LINE_SMOOTH);
        break;

    case NV4097_SET_POLY_SMOOTH_ENABLE:
        enabled ? glEnable(GL_POLYGON_SMOOTH) : glDisable(GL_POLYGON_SMOOTH);
        break;
    }
    checkRendererError("Enable");*/
}

void PGRAPH::End() {
    vertex_primitive = 0;
}

void PGRAPH::Flip() {
}

void PGRAPH::UnbindVertexAttributes() {
    /*for (int index = 0; index < 16; index++) {
        glDisableVertexAttribArray(index);
        checkRendererError("UnbindVertexAttributes");
    }
    glBindVertexArray(0);*/
}

}  // namespace rsx
}  // namespace gpu
