/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

// gl_state_dsa.cpp - direct state access emulation
#include "gl_state_dsa.h"

using namespace std;

/**
 * This contains all posible vertex buffer targets ordered by thier id
 * If you insert a new target, keep the order intact or lower_bound
 * will be not able to find the correct target.
 */
static const GLenum vertex_buffer_targets[] = {
    GL_ARRAY_BUFFER,
    GL_ELEMENT_ARRAY_BUFFER,
    GL_PIXEL_PACK_BUFFER,
    GL_PIXEL_UNPACK_BUFFER,
    GL_UNIFORM_BUFFER,
    GL_TEXTURE_BUFFER,
    GL_TRANSFORM_FEEDBACK_BUFFER,
    GL_COPY_READ_BUFFER,
    GL_COPY_WRITE_BUFFER,
    GL_DRAW_INDIRECT_BUFFER,
    GL_SHADER_STORAGE_BUFFER,
    GL_DISPATCH_INDIRECT_BUFFER,
    GL_QUERY_BUFFER,
    GL_ATOMIC_COUNTER_BUFFER
};

static const GLenum framebuffer_targets[] = {
    GL_READ_FRAMEBUFFER,
    GL_DRAW_FRAMEBUFFER
};

static const GLenum renderbuffer_targets[] = {
    GL_RENDERBUFFER
};

static const GLenum transform_feedback_targets[] = {
    GL_TRANSFORM_FEEDBACK
};

static const GLenum texture_targets[] = {
    GL_TEXTURE_1D,
    GL_TEXTURE_2D,
    GL_TEXTURE_3D,
    GL_TEXTURE_RECTANGLE,
    GL_TEXTURE_CUBE_MAP,
    GL_TEXTURE_1D_ARRAY,
    GL_TEXTURE_2D_ARRAY,
    GL_TEXTURE_BUFFER,
    GL_TEXTURE_CUBE_MAP_ARRAY,
    GL_TEXTURE_2D_MULTISAMPLE,
    GL_TEXTURE_2D_MULTISAMPLE_ARRAY
};

#define ArraySize(name_) ( sizeof(name_) / sizeof(name_[0]) )

static int remap_vertex_buffer_target( GLenum target_ ) {
    auto ref = lower_bound( &vertex_buffer_targets[0], &vertex_buffer_targets[0] + ArraySize( vertex_buffer_targets ), target_ );
//    assert( ref != ( &vertex_buffer_targets[0] + ArraySize( vertex_buffer_targets ) ) && "unable to find requested vertex buffer target, update required" );
//    assert( *ref == target_ && "unable to find requested vertex buffer target, update required" );
    return static_cast<int>( distance( &vertex_buffer_targets[0], ref ) );
}

static int remap_framebuffer_target( GLenum target_ ) {
    auto ref = lower_bound( &framebuffer_targets[0], &framebuffer_targets[0] + ArraySize( framebuffer_targets ), target_ );
//    assert( ref != ( &framebuffer_targets[0] + ArraySize( framebuffer_targets ) ) && "unable to find requested vertex buffer target, update required" );
//    assert( *ref == target_ && "unable to find requested vertex buffer target, update required" );
    return static_cast<int>( distance( &framebuffer_targets[0], ref ) );
}

static int remap_renderbuffer_target( GLuint target_ ) {
    auto ref = lower_bound( &renderbuffer_targets[0], &renderbuffer_targets[0] + ArraySize( renderbuffer_targets ), target_ );
//    assert( ref != ( &renderbuffer_targets[0] + ArraySize( renderbuffer_targets ) ) && "unable to find requested vertex buffer target, update required" );
//    assert( *ref == target_ && "unable to find requested vertex buffer target, update required" );
    return static_cast<int>( distance( &renderbuffer_targets[0], ref ) );
}

static int remap_transform_feedback_target( GLuint target_ ) {
    auto ref = lower_bound( &transform_feedback_targets[0], &transform_feedback_targets[0] + ArraySize( transform_feedback_targets ), target_ );
//    assert( ref != ( &transform_feedback_targets[0] + ArraySize( transform_feedback_targets ) ) && "unable to find requested vertex buffer target, update required" );
//    assert( *ref == target_ && "unable to find requested vertex buffer target, update required" );
    return static_cast<int>( distance( &transform_feedback_targets[0], ref ) );
}

static int remap_texture_target( GLuint target_ ) {
    auto ref = lower_bound( &texture_targets[0], &texture_targets[0] + ArraySize( texture_targets ), target_ );
//    assert( ref != ( &texture_targets[0] + ArraySize( texture_targets ) ) && "unable to find requested vertex buffer target, update required" );
//    assert( *ref == target_ && "unable to find requested vertex buffer target, update required" );
    return static_cast<int>( distance( &texture_targets[0], ref ) );
}

struct context {
    struct texture_unit_targets {
        GLuint  _targets[ArraySize( texture_targets )];

        texture_unit_targets() {
            std::fill( _targets, _targets + ArraySize( texture_targets ), 0 );
        }
    };
    // track currently bound vertex buffer, per target
    GLuint                               _vertex_buffers[ArraySize( vertex_buffer_targets )];
    GLuint                              _framebuffers[ArraySize( framebuffer_targets )];
    GLuint                              _renderbuffers[ArraySize( renderbuffer_targets )];
    GLuint                              _transform_feedbacks[ArraySize( transform_feedback_targets )];
    GLuint                              _program_pipeline;
    GLuint                              _vertex_array;
    GLuint                              _active_texture_unit;
    std::vector<GLuint>                 _samplers;
    std::vector<texture_unit_targets>   _textures;

    context() {
        fill( _vertex_buffers, _vertex_buffers + ArraySize( vertex_buffer_targets ), 0 );
        fill( _framebuffers, _framebuffers + ArraySize( framebuffer_targets ), 0 );
        fill( _renderbuffers, _renderbuffers + ArraySize( renderbuffer_targets ), 0 );
        fill( _transform_feedbacks, _transform_feedbacks + ArraySize( transform_feedback_targets ), 0 );
        _program_pipeline = 0;
        _vertex_array = 0;
        _active_texture_unit = 0;

        GLint units;
        RemapGLName( glGetIntegerv )( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &units );
        _samplers.resize( units, 0 );
        _textures.resize( units );
    }
};

context& get_context() {
    OGL_DSA_DEFINE_THREAD_LOCAL_VARIABLE( context, ctx );
    return ctx;
}

/* binding points original pointers */
PFNGLBINDBUFFERPROC glBindBuffer_original;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_original;
PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer_original;
PFNGLBINDTRANSFORMFEEDBACKPROC glBindTransformFeedback_original;
PFNGLBINDSAMPLERPROC glBindSampler_original;
PFNGLBINDPROGRAMPIPELINEPROC glBindProgramPipeline_original;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray_original;
#ifndef PFNGLBINDTEXTUREPROC
typedef void (GLAPIENTRY *PFNGLBINDTEXTUREPROC) (GLenum target, GLuint texture);
#endif
PFNGLBINDTEXTUREPROC glBindTexture_original;
PFNGLACTIVETEXTUREPROC glActiveTexture_original;
/* binding point injectors */
void APIENTRY glBindBuffer_injected( GLenum target, GLuint buffer );
void APIENTRY glBindFramebuffer_injected( GLenum target, GLuint framebuffer );
void APIENTRY glBindRenderbuffer_injected( GLenum target, GLuint renderbuffer );
void APIENTRY glBindTransformFeedback_injected( GLenum target, GLuint id );
void APIENTRY glBindSampler_injected( GLuint unit, GLuint sampler );
void APIENTRY glBindProgramPipeline_injected( GLuint pipeline );
void APIENTRY glBindVertexArray_injected( GLuint array );
void APIENTRY glBindTexture_injected( GLenum target, GLuint texture );
void APIENTRY glActiveTexture_injected( GLenum texture );
/* transform feedback */
void APIENTRY glCreateTransformFeedbacks_emulated( GLsizei n, GLuint *ids );
void APIENTRY glTransformFeedbackBufferBase_emulated( GLuint xfb, GLuint index, GLuint buffer );
void APIENTRY glTransformFeedbackBufferRange_emulated( GLuint xfb, GLuint index, GLuint buffer, GLintptr offset, GLsizei size );
void APIENTRY glGetTransformFeedbackiv_emulated( GLuint xfb, GLenum pname, GLint *param );
void APIENTRY glGetTransformFeedbacki_v_emulated( GLuint xfb, GLenum pname, GLuint index, GLint *param );
void APIENTRY glGetTransformFeedbacki64_v_emulated( GLuint xfb, GLenum pname, GLuint index, GLint64 *param );
/* vertex buffer object */
void APIENTRY glCreateBuffers_emulated( GLsizei n, GLuint *buffers );
void APIENTRY glNamedBufferStorage_emulated( GLuint buffer, GLsizei size, const GLvoid *data, GLbitfield flags );
void APIENTRY glNamedBufferData_emulated( GLuint buffer, GLsizei size, const GLvoid *data, GLenum usage );
void APIENTRY glNamedBufferSubData_emulated( GLuint buffer, GLintptr offset, GLsizei size, const GLvoid *data );
void APIENTRY glCopyNamedBufferSubData_emulated( GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizei size );
void APIENTRY glClearNamedBufferData_emulated( GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const GLvoid *data );
void APIENTRY glClearNamedBufferSubData_emulated( GLuint buffer, GLenum internalformat, GLintptr offset, GLsizei size, GLenum format, GLenum type, const GLvoid *data );
void* APIENTRY glMapNamedBuffer_emulated( GLuint buffer, GLenum access );
void* APIENTRY glMapNamedBufferRange_emulated( GLuint buffer, GLintptr offset, GLsizei length, GLbitfield access );
GLboolean APIENTRY glUnmapNamedBuffer_emulated( GLuint buffer );
void APIENTRY glFlushMappedNamedBufferRange_emulated( GLuint buffer, GLintptr offset, GLsizei length );
void APIENTRY glGetNamedBufferParameteriv_emulated( GLuint buffer, GLenum pname, GLint *params );
void APIENTRY glGetNamedBufferParameteri64v_emulated( GLuint buffer, GLenum pname, GLint64 *params );
void APIENTRY glGetNamedBufferPointerv_emulated( GLuint buffer, GLenum pname, GLvoid **params );
void APIENTRY glGetNamedBufferSubData_emulated( GLuint buffer, GLintptr offset, GLsizei size, GLvoid *data );
/* framebuffer object */
void APIENTRY glCreateFramebuffers_emulated( GLsizei n, GLuint *framebuffers );
void APIENTRY glNamedFramebufferRenderbuffer_emulated( GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer );
void APIENTRY glNamedFramebufferParameteri_emulated( GLuint framebuffer, GLenum pname, GLint param );
void APIENTRY glNamedFramebufferTexture_emulated( GLuint framebuffer, GLenum attachment, GLuint texture, GLint level );
void APIENTRY glNamedFramebufferTextureLayer_emulated( GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer );
void APIENTRY glNamedFramebufferDrawBuffer_emulated( GLuint framebuffer, GLenum mode );
void APIENTRY glNamedFramebufferDrawBuffers_emulated( GLuint framebuffer, GLsizei n, const GLenum *bufs );
void APIENTRY glNamedFramebufferReadBuffer_emulated( GLuint framebuffer, GLenum mode );
void APIENTRY glInvalidateNamedFramebufferData_emulated( GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments );
void APIENTRY glInvalidateNamedFramebufferSubData_emulated( GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height );
void APIENTRY glClearNamedFramebufferiv_emulated( GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value );
void APIENTRY glClearNamedFramebufferuiv_emulated( GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value );
void APIENTRY glClearNamedFramebufferfv_emulated( GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value );
void APIENTRY glClearNamedFramebufferfi_emulated( GLuint framebuffer, GLenum buffer, GLfloat depth, GLint stencil );
void APIENTRY glBlitNamedFramebuffer_emulated( GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter );
GLenum APIENTRY glCheckNamedFramebufferStatus_emulated( GLuint framebuffer, GLenum target );
void APIENTRY glGetNamedFramebufferParameteriv_emulated( GLuint framebuffer, GLenum pname, GLint *param );
void APIENTRY glGetNamedFramebufferAttachmentParameteriv_emulated( GLuint framebuffer, GLenum attachment, GLenum pname, GLint *params );
/* Renderbuffer object functions */
void APIENTRY glCreateRenderbuffers_emulated( GLsizei n, GLuint *renderbuffers );
void APIENTRY glNamedRenderbufferStorage_emulated( GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height );
void APIENTRY glNamedRenderbufferStorageMultisample_emulated( GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height );
void APIENTRY glGetNamedRenderbufferParameteriv_emulated( GLuint renderbuffer, GLenum pname, GLint *params );
/* Sampler object functions */
void APIENTRY glCreateSamplers_emulated( GLsizei n, GLuint *samplers );
/* Program Pipeline object functions */
void APIENTRY glCreateProgramPipelines_emulated( GLsizei n, GLuint* pipelines );
/* Query object functions */
void APIENTRY glCreateQueries_emulated( GLenum target, GLsizei n, GLuint* ids );
/* Vertex Array object functions */
void APIENTRY glCreateVertexArrays_emulated( GLsizei n, GLuint *arrays );
void APIENTRY glDisableVertexArrayAttrib_emulated( GLuint vaobj, GLuint index );
void APIENTRY glEnableVertexArrayAttrib_emulated( GLuint vaobj, GLuint index );
void APIENTRY glVertexArrayElementBuffer_emulated( GLuint vaobj, GLuint buffer );
void APIENTRY glVertexArrayVertexBuffer_emulated( GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride );
void APIENTRY glVertexArrayVertexBuffers_emulated( GLuint vaobj, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides );
void APIENTRY glVertexArrayAttribFormat_emulated( GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset );
void APIENTRY glVertexArrayAttribIFormat_emulated( GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset );
void APIENTRY glVertexArrayAttribLFormat_emulated( GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset );
void APIENTRY glVertexArrayAttribBinding_emulated( GLuint vaobj, GLuint attribindex, GLuint bindingindex );
void APIENTRY glVertexArrayBindingDivisor_emulated( GLuint vaobj, GLuint bindingindex, GLuint divisor );
void APIENTRY glGetVertexArrayiv_emulated( GLuint vaobj, GLenum pname, GLint *param );
void APIENTRY glGetVertexArrayIndexediv_emulated( GLuint vaobj, GLuint index, GLenum pname, GLint *param );
void APIENTRY glGetVertexArrayIndexed64iv_emulated( GLuint vaobj, GLuint index, GLenum pname, GLint64 *param );
/* Texture object functions */
void APIENTRY glCreateTextures_emulated( GLenum target, GLsizei n, GLuint *textures );
void APIENTRY glTextureBufferEXT_emulated( GLuint texture, GLenum target, GLenum internalformat, GLuint buffer );
void APIENTRY glTextureBufferRangeEXT_emulated( GLuint texture, GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size );
void APIENTRY glTextureStorage1DEXT_emulated( GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width );
void APIENTRY glTextureStorage2DEXT_emulated( GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height );
void APIENTRY glTextureStorage3DEXT_emulated( GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth );
void APIENTRY glTextureStorage2DMultisampleEXT_emulated( GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations );
void APIENTRY glTextureStorage3DMultisampleEXT_emulated( GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations );
void APIENTRY glTextureSubImage1DEXT_emulated( GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels );
void APIENTRY glTextureSubImage2DEXT_emulated( GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels );
void APIENTRY glTextureSubImage3DEXT_emulated( GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels );
void APIENTRY glCompressedTextureSubImage1DEXT_emulated( GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *bits );
void APIENTRY glCompressedTextureSubImage2DEXT_emulated( GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *bits );
void APIENTRY glCompressedTextureSubImage3DEXT_emulated( GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *bits );
void APIENTRY glCopyTextureSubImage1DEXT_emulated( GLuint texture, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width );
void APIENTRY glCopyTextureSubImage2DEXT_emulated( GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height );
void APIENTRY glCopyTextureSubImage3DEXT_emulated( GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height );
void APIENTRY glTextureParameterfEXT_emulated( GLuint texture, GLenum target, GLenum pname, GLfloat param );
void APIENTRY glTextureParameterfvEXT_emulated( GLuint texture, GLenum target, GLenum pname, const GLfloat *params );
void APIENTRY glTextureParameteriEXT_emulated( GLuint texture, GLenum target, GLenum pname, GLint param );
void APIENTRY glTextureParameterivEXT_emulated( GLuint texture, GLenum target, GLenum pname, const GLint *params );
void APIENTRY glTextureParameterIivEXT_emulated( GLuint texture, GLenum target, GLenum pname, const GLint *params );
void APIENTRY glTextureParameterIuivEXT_emulated( GLuint texture, GLenum target, GLenum pname, const GLuint *params );
void APIENTRY glGenerateTextureMipmapEXT_emulated( GLuint texture, GLenum target );
void APIENTRY glBindMultiTextureEXT_emulated( GLenum texunit, GLenum target, GLuint texture );
void APIENTRY glGetTextureImageEXT_emulated( GLuint texture, GLenum target, GLint level, GLenum format, GLenum type, void *pixels );
void APIENTRY glGetCompressedTextureImageEXT_emulated( GLuint texture, GLenum target, GLint lod, void *img );
void APIENTRY glGetTextureLevelParameterfvEXT_emulated( GLuint texture, GLenum target, GLint level, GLenum pname, GLfloat *params );
void APIENTRY glGetTextureLevelParameterivEXT_emulated( GLuint texture, GLenum target, GLint level, GLenum pname, GLint *params );
void APIENTRY glGetTextureParameterfvEXT_emulated( GLuint texture, GLenum target, GLenum pname, GLfloat *params );
void APIENTRY glGetTextureParameterivEXT_emulated( GLuint texture, GLenum target, GLenum pname, GLint *params );
void APIENTRY glGetTextureParameterIivEXT_emulated( GLuint texture, GLenum target, GLenum pname, GLint *params );
void APIENTRY glGetTextureParameterIuivEXT_emulated( GLuint texture, GLenum target, GLenum pname, GLuint *params );

/* some mappings from ARB dsa to EXT dsa require a wrapper on systems were sizeof(GLintptr) != sizeof(GLint) */
/* vertex buffer object */
void APIENTRY glNamedBufferStorage_EXT( GLuint buffer, GLsizei size, const GLvoid *data, GLbitfield flags );
void APIENTRY glNamedBufferData_EXT( GLuint buffer, GLsizei size, const GLvoid *data, GLenum usage );
void APIENTRY glNamedBufferSubData_EXT( GLuint buffer, GLintptr offset, GLsizei size, const GLvoid *data );
void APIENTRY glCopyNamedBufferSubData_EXT( GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizei size );
void* APIENTRY glMapNamedBufferRange_EXT( GLuint buffer, GLintptr offset, GLsizei length, GLbitfield access );
void APIENTRY glFlushMappedNamedBufferRange_EXT( GLuint buffer, GLintptr offset, GLsizei length );
void APIENTRY glGetNamedBufferSubData_EXT( GLuint buffer, GLintptr offset, GLsizei size, GLvoid *data );
/* Vertex Array object functions */
void APIENTRY glVertexArrayVertexBuffers_EXT( GLuint vaobj, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides );
/* texture object stuff on ARB is different from EXT stuff, ARB does not take the texture type, because createtexture sets this */
/* Texture object functions */
void APIENTRY glTextureBufferEXT_ARB( GLuint texture, GLenum target, GLenum internalformat, GLuint buffer );
void APIENTRY glTextureBufferRangeEXT_ARB( GLuint texture, GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size );
void APIENTRY glTextureStorage1DEXT_ARB( GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width );
void APIENTRY glTextureStorage2DEXT_ARB( GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height );
void APIENTRY glTextureStorage3DEXT_ARB( GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth );
void APIENTRY glTextureStorage2DMultisampleEXT_ARB( GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations );
void APIENTRY glTextureStorage3DMultisampleEXT_ARB( GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations );
void APIENTRY glTextureSubImage1DEXT_ARB( GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels );
void APIENTRY glTextureSubImage2DEXT_ARB( GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels );
void APIENTRY glTextureSubImage3DEXT_ARB( GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels );
void APIENTRY glCompressedTextureSubImage1DEXT_ARB( GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *bits );
void APIENTRY glCompressedTextureSubImage2DEXT_ARB( GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *bits );
void APIENTRY glCompressedTextureSubImage3DEXT_ARB( GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *bits );
void APIENTRY glCopyTextureSubImage1DEXT_ARB( GLuint texture, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width );
void APIENTRY glCopyTextureSubImage2DEXT_ARB( GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height );
void APIENTRY glCopyTextureSubImage3DEXT_ARB( GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height );
void APIENTRY glTextureParameterfEXT_ARB( GLuint texture, GLenum target, GLenum pname, GLfloat param );
void APIENTRY glTextureParameterfvEXT_ARB( GLuint texture, GLenum target, GLenum pname, const GLfloat *params );
void APIENTRY glTextureParameteriEXT_ARB( GLuint texture, GLenum target, GLenum pname, GLint param );
void APIENTRY glTextureParameterivEXT_ARB( GLuint texture, GLenum target, GLenum pname, const GLint *params );
void APIENTRY glTextureParameterIivEXT_ARB( GLuint texture, GLenum target, GLenum pname, const GLint *params );
void APIENTRY glTextureParameterIuivEXT_ARB( GLuint texture, GLenum target, GLenum pname, const GLuint *params );
void APIENTRY glGenerateTextureMipmapEXT_ARB( GLuint texture, GLenum target );
void APIENTRY glBindMultiTextureEXT_ARB( GLenum texunit, GLenum target, GLuint texture );
void APIENTRY glGetTextureImageEXT_ARB( GLuint texture, GLenum target, GLint level, GLenum format, GLenum type, void *pixels );
void APIENTRY glGetCompressedTextureImageEXT_ARB( GLuint texture, GLenum target, GLint lod, void *img );
void APIENTRY glGetTextureLevelParameterfvEXT_ARB( GLuint texture, GLenum target, GLint level, GLenum pname, GLfloat *params );
void APIENTRY glGetTextureLevelParameterivEXT_ARB( GLuint texture, GLenum target, GLint level, GLenum pname, GLint *params );
void APIENTRY glGetTextureParameterfvEXT_ARB( GLuint texture, GLenum target, GLenum pname, GLfloat *params );
void APIENTRY glGetTextureParameterivEXT_ARB( GLuint texture, GLenum target, GLenum pname, GLint *params );
void APIENTRY glGetTextureParameterIivEXT_ARB( GLuint texture, GLenum target, GLenum pname, GLint *params );
void APIENTRY glGetTextureParameterIuivEXT_ARB( GLuint texture, GLenum target, GLenum pname, GLuint *params );


int GL_Init_DSA_Emulation( bool inject_always_ /*= false*/, bool allow_arb_dsa_ /*= true*/, bool allow_ext_dsa_ /*= true*/ ) {
    bool arb_used = true;
    bool ext_used = true;

    /* transform feedback */
    if ( !CheckedFunctionLoad( glCreateTransformFeedbacks ) || !allow_arb_dsa_ ) {
        RemapGLName( glCreateTransformFeedbacks ) = glCreateTransformFeedbacks_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glTransformFeedbackBufferBase ) || !allow_arb_dsa_ ) {
        RemapGLName( glTransformFeedbackBufferBase ) = glTransformFeedbackBufferBase_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glTransformFeedbackBufferRange ) || !allow_arb_dsa_ ) {
        RemapGLName( glTransformFeedbackBufferRange ) = glTransformFeedbackBufferRange_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glGetTransformFeedbackiv ) || !allow_arb_dsa_ ) {
        RemapGLName( glGetTransformFeedbackiv ) = glGetTransformFeedbackiv_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glGetTransformFeedbacki_v ) || !allow_arb_dsa_ ) {
        RemapGLName( glGetTransformFeedbacki_v ) = glGetTransformFeedbacki_v_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glGetTransformFeedbacki64_v ) || !allow_arb_dsa_ ) {
        RemapGLName( glGetTransformFeedbacki64_v ) = glGetTransformFeedbacki64_v_emulated;
        arb_used = false;
    }

    /* vertex buffer object */
    if ( !CheckedFunctionLoad( glCreateBuffers ) || !allow_arb_dsa_ ) {
        RemapGLName( glCreateBuffers ) = glCreateBuffers_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glNamedBufferStorage ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glNamedBufferStorageEXT ) && allow_ext_dsa_ ) {
#if PTRDIFF_MAX == INT_MAX
            RemapGLName( glNamedBufferStorage ) = RemapGLName( glNamedBufferStorageEXT );
#else
            RemapGLName( glNamedBufferStorage ) = glNamedBufferStorage_EXT;
#endif
        } else {
            RemapGLName( glNamedBufferStorage ) = glNamedBufferStorage_emulated;
            ext_used = false;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glNamedBufferData ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glNamedBufferDataEXT ) && allow_ext_dsa_ ) {
#if PTRDIFF_MAX == INT_MAX
            RemapGLName( glNamedBufferData ) = RemapGLName( glNamedBufferDataEXT );
#else
            RemapGLName( glNamedBufferData ) = glNamedBufferData_EXT;
#endif
        } else {
            RemapGLName( glNamedBufferData ) = glNamedBufferData_emulated;
            ext_used = false;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glNamedBufferSubData ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glNamedBufferSubDataEXT ) && allow_ext_dsa_ ) {
#if PTRDIFF_MAX == INT_MAX
            RemapGLName( glNamedBufferSubData ) = RemapGLName( glNamedBufferSubDataEXT );
#else
            RemapGLName( glNamedBufferSubData ) = glNamedBufferSubData_EXT;
#endif
        } else {
            RemapGLName( glNamedBufferSubData ) = glNamedBufferSubData_emulated;
            ext_used = false;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glCopyNamedBufferSubData ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glNamedCopyBufferSubDataEXT ) && allow_ext_dsa_ ) {
#if PTRDIFF_MAX == INT_MAX
            RemapGLName( glCopyNamedBufferSubData ) = RemapGLName( glNamedCopyBufferSubDataEXT );
#else
            RemapGLName( glCopyNamedBufferSubData ) = glCopyNamedBufferSubData_EXT;
#endif
        } else {
            RemapGLName( glCopyNamedBufferSubData ) = glCopyNamedBufferSubData_emulated;
            ext_used = false;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glClearNamedBufferData ) || !allow_arb_dsa_ ) {
        RemapGLName( glClearNamedBufferData ) = glClearNamedBufferData_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glClearNamedBufferSubData ) || !allow_arb_dsa_ ) {
        RemapGLName( glClearNamedBufferSubData ) = glClearNamedBufferSubData_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glMapNamedBuffer ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glMapNamedBufferEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glMapNamedBuffer ) = RemapGLName( glMapNamedBufferEXT );
        } else {
            RemapGLName( glMapNamedBuffer ) = glMapNamedBuffer_emulated;
            ext_used = false;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glMapNamedBufferRange ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glMapNamedBufferRangeEXT ) && allow_ext_dsa_ ) {
#if PTRDIFF_MAX == INT_MAX
            RemapGLName( glMapNamedBufferRange ) = RemapGLName( glMapNamedBufferRangeEXT );
#else
            RemapGLName( glMapNamedBufferRange ) = glMapNamedBufferRange_EXT;
#endif
        } else {
            RemapGLName( glMapNamedBufferRange ) = glMapNamedBufferRange_emulated;
            ext_used = false;
        }
    }
    if ( !CheckedFunctionLoad( glUnmapNamedBuffer ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glUnmapNamedBufferEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glUnmapNamedBuffer ) = RemapGLName( glUnmapNamedBufferEXT );
        } else {
            RemapGLName( glUnmapNamedBuffer ) = glUnmapNamedBuffer_emulated;
            ext_used = false;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glFlushMappedNamedBufferRange ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glFlushMappedNamedBufferRangeEXT ) && allow_ext_dsa_ ) {
#if PTRDIFF_MAX == INT_MAX
            RemapGLName( glFlushMappedNamedBufferRange ) = RemapGLName( glFlushMappedNamedBufferRangeEXT );
#else
            RemapGLName( glFlushMappedNamedBufferRange ) = glFlushMappedNamedBufferRange_EXT;
#endif
        } else {
            RemapGLName( glFlushMappedNamedBufferRange ) = glFlushMappedNamedBufferRange_emulated;
            ext_used = false;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glGetNamedBufferParameteriv ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glGetNamedBufferParameterivEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glGetNamedBufferParameteriv ) = RemapGLName( glGetNamedBufferParameterivEXT );
        } else {
            RemapGLName( glGetNamedBufferParameteriv ) = glGetNamedBufferParameteriv_emulated;
            ext_used = false;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glGetNamedBufferParameteri64v ) || !allow_arb_dsa_ ) {
        RemapGLName( glGetNamedBufferParameteri64v ) = glGetNamedBufferParameteri64v_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glGetNamedBufferPointerv ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glGetNamedBufferPointervEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glGetNamedBufferPointerv ) = RemapGLName( glGetNamedBufferPointervEXT );
        } else {
            RemapGLName( glGetNamedBufferPointerv ) = glGetNamedBufferPointerv_emulated;
            ext_used = false;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glGetNamedBufferSubData ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glGetNamedBufferSubDataEXT ) && allow_ext_dsa_ ) {
#if PTRDIFF_MAX == INT_MAX
            RemapGLName( glGetNamedBufferSubData ) = RemapGLName( glGetNamedBufferSubDataEXT );
#else
            RemapGLName( glGetNamedBufferSubData ) = glGetNamedBufferSubData_EXT;
#endif
        } else {
            RemapGLName( glGetNamedBufferSubData ) = glGetNamedBufferSubData_emulated;
            ext_used = false;
        }
        arb_used = false;
    }

    /* framebuffer object */
    if ( !CheckedFunctionLoad( glCreateFramebuffers ) || !allow_arb_dsa_ ) {
        RemapGLName( glCreateFramebuffers ) = glCreateFramebuffers_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glNamedFramebufferRenderbuffer ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glNamedFramebufferRenderbufferEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glNamedFramebufferRenderbuffer ) = RemapGLName( glNamedFramebufferRenderbufferEXT );
        } else {
            RemapGLName( glNamedFramebufferRenderbuffer ) = glNamedFramebufferRenderbuffer_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glNamedFramebufferParameteri ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glNamedFramebufferParameteriEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glNamedFramebufferParameteri ) = RemapGLName( glNamedFramebufferParameteriEXT );
        } else {
            RemapGLName( glNamedFramebufferParameteri ) = glNamedFramebufferParameteri_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glNamedFramebufferTexture ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glNamedFramebufferTextureEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glNamedFramebufferTexture ) = RemapGLName( glNamedFramebufferTextureEXT );
        } else {
            RemapGLName( glNamedFramebufferTexture ) = glNamedFramebufferTexture_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glNamedFramebufferTextureLayer ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glNamedFramebufferTextureLayerEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glNamedFramebufferTextureLayer ) = RemapGLName( glNamedFramebufferTextureLayerEXT );
        } else {
            RemapGLName( glNamedFramebufferTextureLayer ) = glNamedFramebufferTextureLayer_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glNamedFramebufferDrawBuffer ) || !allow_arb_dsa_ ) {
        RemapGLName( glNamedFramebufferDrawBuffer ) = glNamedFramebufferDrawBuffer_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glNamedFramebufferDrawBuffers ) || !allow_arb_dsa_ ) {
        RemapGLName( glNamedFramebufferDrawBuffers ) = glNamedFramebufferDrawBuffers_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glNamedFramebufferReadBuffer ) || !allow_arb_dsa_ ) {
        RemapGLName( glNamedFramebufferReadBuffer ) = glNamedFramebufferReadBuffer_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glInvalidateNamedFramebufferData ) || !allow_arb_dsa_ ) {
        RemapGLName( glInvalidateNamedFramebufferData ) = glInvalidateNamedFramebufferData_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glInvalidateNamedFramebufferSubData ) || !allow_arb_dsa_ ) {
        RemapGLName( glInvalidateNamedFramebufferSubData ) = glInvalidateNamedFramebufferSubData_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glClearNamedFramebufferiv ) || !allow_arb_dsa_ ) {
        RemapGLName( glClearNamedFramebufferiv ) = glClearNamedFramebufferiv_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glClearNamedFramebufferuiv ) || !allow_arb_dsa_ ) {
        RemapGLName( glClearNamedFramebufferuiv ) = glClearNamedFramebufferuiv_emulated;
        arb_used = false;
    }
/*
    if ( !CheckedFunctionLoad( glClearNamedFramebufferfv ) || !allow_arb_dsa_ ) {
        RemapGLName( glClearNamedFramebufferfv ) = glClearNamedFramebufferfv_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glClearNamedFramebufferfi ) || !allow_arb_dsa_ ) {
        RemapGLName( glClearNamedFramebufferfi ) = glClearNamedFramebufferfi_emulated;
        arb_used = false;
    }
*/
    if ( !CheckedFunctionLoad( glBlitNamedFramebuffer ) || !allow_arb_dsa_ ) {
        RemapGLName( glBlitNamedFramebuffer ) = glBlitNamedFramebuffer_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glCheckNamedFramebufferStatus ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glCheckNamedFramebufferStatusEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glCheckNamedFramebufferStatus ) = RemapGLName( glCheckNamedFramebufferStatusEXT );
        } else {
            RemapGLName( glCheckNamedFramebufferStatus ) = glCheckNamedFramebufferStatus_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glGetNamedFramebufferParameteriv ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glGetNamedFramebufferParameterivEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glGetNamedFramebufferParameteriv ) = RemapGLName( glGetNamedFramebufferParameterivEXT );
        } else {
            RemapGLName( glGetNamedFramebufferParameteriv ) = glGetNamedFramebufferParameteriv_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glGetNamedFramebufferAttachmentParameteriv ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glGetNamedFramebufferAttachmentParameterivEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glGetNamedFramebufferAttachmentParameteriv ) = RemapGLName( glGetNamedFramebufferAttachmentParameterivEXT );
        } else {
            RemapGLName( glGetNamedFramebufferAttachmentParameteriv ) = glGetNamedFramebufferAttachmentParameteriv_emulated;
        }
        arb_used = false;
    }

    /* Renderbuffer object functions */
    if ( !CheckedFunctionLoad( glCreateRenderbuffers ) || !allow_arb_dsa_ ) {
        RemapGLName( glCreateRenderbuffers ) = glCreateRenderbuffers_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glNamedRenderbufferStorage ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glNamedRenderbufferStorageEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glNamedRenderbufferStorage ) = RemapGLName( glNamedRenderbufferStorageEXT );
        } else {
            RemapGLName( glNamedRenderbufferStorage ) = glNamedRenderbufferStorage_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glNamedRenderbufferStorageMultisample ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glNamedRenderbufferStorageMultisampleEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glNamedRenderbufferStorageMultisample ) = RemapGLName( glNamedRenderbufferStorageMultisampleEXT );
        } else {
            RemapGLName( glNamedRenderbufferStorageMultisample ) = glNamedRenderbufferStorageMultisample_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glGetNamedRenderbufferParameteriv ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glGetNamedRenderbufferParameterivEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glGetNamedRenderbufferParameteriv ) = RemapGLName( glGetNamedRenderbufferParameterivEXT );
        } else {
            RemapGLName( glGetNamedRenderbufferParameteriv ) = glGetNamedRenderbufferParameteriv_emulated;
        }
        arb_used = false;
    }
    /* Sampler object functions */
    if ( !CheckedFunctionLoad( glCreateSamplers ) || !allow_arb_dsa_ ) {
        RemapGLName( glCreateSamplers ) = glCreateSamplers_emulated;
        arb_used = false;
    }

    /* Program Pipeline object functions */
    if ( !CheckedFunctionLoad( glCreateProgramPipelines ) || !allow_arb_dsa_ ) {
        RemapGLName( glCreateProgramPipelines ) = glCreateProgramPipelines_emulated;
        arb_used = false;
    }

    /* Query object functions */
    if ( !CheckedFunctionLoad( glCreateQueries ) || !allow_arb_dsa_ ) {
        RemapGLName( glCreateQueries ) = glCreateQueries_emulated;
        arb_used = false;
    }

    /* Vertex Array object functions */
    if ( !CheckedFunctionLoad( glCreateVertexArrays ) || !allow_arb_dsa_ ) {
        RemapGLName( glCreateVertexArrays ) = glCreateVertexArrays_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glDisableVertexArrayAttrib ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glDisableVertexArrayAttribEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glDisableVertexArrayAttrib ) = RemapGLName( glDisableVertexArrayAttribEXT );
        } else {
            RemapGLName( glDisableVertexArrayAttrib ) = glDisableVertexArrayAttrib_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glEnableVertexArrayAttrib ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glEnableVertexArrayAttribEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glEnableVertexArrayAttrib ) = RemapGLName( glEnableVertexArrayAttribEXT );
        } else {
            RemapGLName( glEnableVertexArrayAttrib ) = glEnableVertexArrayAttrib_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glVertexArrayElementBuffer ) || !allow_arb_dsa_ ) {
        RemapGLName( glVertexArrayElementBuffer ) = glVertexArrayElementBuffer_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glVertexArrayVertexBuffer ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glVertexArrayBindVertexBufferEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glVertexArrayVertexBuffer ) = RemapGLName( glVertexArrayBindVertexBufferEXT );
        } else {
            RemapGLName( glVertexArrayVertexBuffer ) = glVertexArrayVertexBuffer_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glVertexArrayVertexBuffers ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glVertexArrayBindVertexBufferEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glVertexArrayVertexBuffers ) = glVertexArrayVertexBuffers_EXT;
        } else {
            RemapGLName( glVertexArrayVertexBuffers ) = glVertexArrayVertexBuffers_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glVertexArrayAttribFormat ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glVertexArrayVertexAttribFormatEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glVertexArrayAttribFormat ) = RemapGLName( glVertexArrayVertexAttribFormatEXT );
        } else {
            RemapGLName( glVertexArrayAttribFormat ) = glVertexArrayAttribFormat_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glVertexArrayAttribIFormat ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glVertexArrayVertexAttribIFormatEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glVertexArrayAttribIFormat ) = RemapGLName( glVertexArrayVertexAttribIFormatEXT );
        } else {
            RemapGLName( glVertexArrayAttribIFormat ) = glVertexArrayAttribIFormat_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glVertexArrayAttribLFormat ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glVertexArrayVertexAttribLFormatEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glVertexArrayAttribLFormat ) = RemapGLName( glVertexArrayVertexAttribLFormatEXT );
        } else {
            RemapGLName( glVertexArrayAttribLFormat ) = glVertexArrayAttribLFormat_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glVertexArrayAttribBinding ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glVertexArrayVertexAttribBindingEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glVertexArrayAttribBinding ) = RemapGLName( glVertexArrayVertexAttribBindingEXT );
        } else {
            RemapGLName( glVertexArrayAttribBinding ) = glVertexArrayAttribBinding_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glVertexArrayBindingDivisor ) || !allow_arb_dsa_ ) {
        if ( CheckedFunctionLoad( glVertexArrayVertexBindingDivisorEXT ) && allow_ext_dsa_ ) {
            RemapGLName( glVertexArrayBindingDivisor ) = RemapGLName( glVertexArrayVertexBindingDivisorEXT );
        } else {
            RemapGLName( glVertexArrayBindingDivisor ) = glVertexArrayBindingDivisor_emulated;
        }
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glGetVertexArrayiv ) || !allow_arb_dsa_ ) {
        RemapGLName( glGetVertexArrayiv ) = glGetVertexArrayiv_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glGetVertexArrayIndexediv ) || !allow_arb_dsa_ ) {
        RemapGLName( glGetVertexArrayIndexediv ) = glGetVertexArrayIndexediv_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glGetVertexArrayIndexed64iv ) || !allow_arb_dsa_ ) {
        RemapGLName( glGetVertexArrayIndexed64iv ) = glGetVertexArrayIndexed64iv_emulated;
        arb_used = false;
    }

    /* Texture object functions */
    if ( !CheckedFunctionLoad( glCreateTextures ) || !allow_arb_dsa_ ) {
        RemapGLName( glCreateTextures ) = glCreateTextures_emulated;
        arb_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureBufferEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureBuffer ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureBufferEXT ) = glTextureBufferEXT_ARB;
        } else {
            RemapGLName( glTextureBufferEXT ) = glTextureBufferEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureBufferRangeEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureBufferRange ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureBufferRangeEXT ) = glTextureBufferRangeEXT_ARB;
        } else {
            RemapGLName( glTextureBufferRangeEXT ) = glTextureBufferRangeEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureStorage1DEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureStorage1D ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureStorage1DEXT ) = glTextureStorage1DEXT_ARB;
        } else {
            RemapGLName( glTextureStorage1DEXT ) = glTextureStorage1DEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureStorage2DEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureStorage2D ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureStorage2DEXT ) = glTextureStorage2DEXT_ARB;
        } else {
            RemapGLName( glTextureStorage2DEXT ) = glTextureStorage2DEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureStorage3DEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureStorage3D ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureStorage3DEXT ) = glTextureStorage3DEXT_ARB;
        } else {
            RemapGLName( glTextureStorage3DEXT ) = glTextureStorage3DEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureStorage2DMultisampleEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureStorage2DMultisample ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureStorage2DMultisampleEXT ) = glTextureStorage2DMultisampleEXT_ARB;
        } else {
            RemapGLName( glTextureStorage2DMultisampleEXT ) = glTextureStorage2DMultisampleEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureStorage3DMultisampleEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureStorage3DMultisample ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureStorage3DMultisampleEXT ) = glTextureStorage3DMultisampleEXT_ARB;
        } else {
            RemapGLName( glTextureStorage3DMultisampleEXT ) = glTextureStorage3DMultisampleEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureSubImage1DEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureSubImage1D ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureSubImage1DEXT ) = glTextureSubImage1DEXT_ARB;
        } else {
            RemapGLName( glTextureSubImage1DEXT ) = glTextureSubImage1DEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureSubImage2DEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureSubImage2D ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureSubImage2DEXT ) = glTextureSubImage2DEXT_ARB;
        } else {
            RemapGLName( glTextureSubImage2DEXT ) = glTextureSubImage2DEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureSubImage3DEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureSubImage3D ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureSubImage3DEXT ) = glTextureSubImage3DEXT_ARB;
        } else {
            RemapGLName( glTextureSubImage3DEXT ) = glTextureSubImage3DEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glCompressedTextureSubImage1DEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glCompressedTextureSubImage1D ) && allow_arb_dsa_ ) {
            RemapGLName( glCompressedTextureSubImage1DEXT ) = glCompressedTextureSubImage1DEXT_ARB;
        } else {
            RemapGLName( glCompressedTextureSubImage1DEXT ) = glCompressedTextureSubImage1DEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glCompressedTextureSubImage2DEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glCompressedTextureSubImage2D ) && allow_arb_dsa_ ) {
            RemapGLName( glCompressedTextureSubImage2DEXT ) = glCompressedTextureSubImage2DEXT_ARB;
        } else {
            RemapGLName( glCompressedTextureSubImage2DEXT ) = glCompressedTextureSubImage2DEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glCompressedTextureSubImage3DEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glCompressedTextureSubImage3D ) && allow_arb_dsa_ ) {
            RemapGLName( glCompressedTextureSubImage3DEXT ) = glCompressedTextureSubImage3DEXT_ARB;
        } else {
            RemapGLName( glCompressedTextureSubImage3DEXT ) = glCompressedTextureSubImage3DEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glCopyTextureSubImage1DEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glCopyTextureSubImage1D ) && allow_arb_dsa_ ) {
            RemapGLName( glCopyTextureSubImage1DEXT ) = glCopyTextureSubImage1DEXT_ARB;
        } else {
            RemapGLName( glCopyTextureSubImage1DEXT ) = glCopyTextureSubImage1DEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glCopyTextureSubImage2DEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glCopyTextureSubImage2D ) && allow_arb_dsa_ ) {
            RemapGLName( glCopyTextureSubImage2DEXT ) = glCopyTextureSubImage2DEXT_ARB;
        } else {
            RemapGLName( glCopyTextureSubImage2DEXT ) = glCopyTextureSubImage2DEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glCopyTextureSubImage3DEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glCopyTextureSubImage3D ) && allow_arb_dsa_ ) {
            RemapGLName( glCopyTextureSubImage3DEXT ) = glCopyTextureSubImage3DEXT_ARB;
        } else {
            RemapGLName( glCopyTextureSubImage3DEXT ) = glCopyTextureSubImage3DEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureParameterfEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureParameterf ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureParameterfEXT ) = glTextureParameterfEXT_ARB;
        } else {
            RemapGLName( glTextureParameterfEXT ) = glTextureParameterfEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureParameterfvEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureParameterfv ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureParameterfvEXT ) = glTextureParameterfvEXT_ARB;
        } else {
            RemapGLName( glTextureParameterfvEXT ) = glTextureParameterfvEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureParameteriEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureParameterfv ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureParameteriEXT ) = glTextureParameteriEXT_ARB;
        } else {
            RemapGLName( glTextureParameteriEXT ) = glTextureParameteriEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureParameterivEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureParameteriv ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureParameterivEXT ) = glTextureParameterivEXT_ARB;
        } else {
            RemapGLName( glTextureParameterivEXT ) = glTextureParameterivEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureParameterIivEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureParameterIiv ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureParameterIivEXT ) = glTextureParameterIivEXT_ARB;
        } else {
            RemapGLName( glTextureParameterIivEXT ) = glTextureParameterIivEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glTextureParameterIuivEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureParameterIuiv ) && allow_arb_dsa_ ) {
            RemapGLName( glTextureParameterIuivEXT ) = glTextureParameterIuivEXT_ARB;
        } else {
            RemapGLName( glTextureParameterIuivEXT ) = glTextureParameterIuivEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glGenerateTextureMipmapEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glTextureParameterIuiv ) && allow_arb_dsa_ ) {
            RemapGLName( glGenerateTextureMipmapEXT ) = glGenerateTextureMipmapEXT_ARB;
        } else {
            RemapGLName( glGenerateTextureMipmapEXT ) = glGenerateTextureMipmapEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glBindMultiTextureEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glBindTextureUnit ) && allow_arb_dsa_ ) {
            RemapGLName( glBindMultiTextureEXT ) = glBindMultiTextureEXT_ARB;
        } else {
            RemapGLName( glBindMultiTextureEXT ) = glBindMultiTextureEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glGetTextureImageEXT ) || !allow_ext_dsa_ ) {
        RemapGLName( glGetTextureImageEXT ) = glGetTextureImageEXT_emulated;
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glGetCompressedTextureImageEXT ) || !allow_ext_dsa_ ) {
        RemapGLName( glGetCompressedTextureImageEXT ) = glGetCompressedTextureImageEXT_emulated;
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glGetTextureLevelParameterfvEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glGetTextureLevelParameterfv ) && allow_arb_dsa_ ) {
            RemapGLName( glGetTextureLevelParameterfvEXT ) = glGetTextureLevelParameterfvEXT_ARB;
        } else {
            RemapGLName( glGetTextureLevelParameterfvEXT ) = glGetTextureLevelParameterfvEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glGetTextureLevelParameterivEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glGetTextureLevelParameteriv ) && allow_arb_dsa_ ) {
            RemapGLName( glGetTextureLevelParameterivEXT ) = glGetTextureLevelParameterivEXT_ARB;
        } else {
            RemapGLName( glGetTextureLevelParameterivEXT ) = glGetTextureLevelParameterivEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glGetTextureParameterfvEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glGetTextureParameterfv ) && allow_arb_dsa_ ) {
            RemapGLName( glGetTextureParameterfvEXT ) = glGetTextureParameterfvEXT_ARB;
        } else {
            RemapGLName( glGetTextureParameterfvEXT ) = glGetTextureParameterfvEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glGetTextureParameterivEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glGetTextureParameteriv ) && allow_arb_dsa_ ) {
            RemapGLName( glGetTextureParameterivEXT ) = glGetTextureParameterivEXT_ARB;
        } else {
            RemapGLName( glGetTextureParameterivEXT ) = glGetTextureParameterivEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glGetTextureParameterIivEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glGetTextureParameterIiv ) && allow_arb_dsa_ ) {
            RemapGLName( glGetTextureParameterIivEXT ) = glGetTextureParameterIivEXT_ARB;
        } else {
            RemapGLName( glGetTextureParameterIivEXT ) = glGetTextureParameterIivEXT_emulated;
        }
        ext_used = false;
    }
    if ( !CheckedFunctionLoad( glGetTextureParameterIuivEXT ) || !allow_ext_dsa_ ) {
        if ( CheckedFunctionLoad( glGetTextureParameterIuiv ) && allow_arb_dsa_ ) {
            RemapGLName( glGetTextureParameterIuivEXT ) = glGetTextureParameterIuivEXT_ARB;
        } else {
            RemapGLName( glGetTextureParameterIuivEXT ) = glGetTextureParameterIuivEXT_emulated;
        }
        ext_used = false;
    }

    dsa_level result;
    if ( arb_used ) {
        if ( ext_used ) {
            result = dsa_level::full_support;
        } else {
            result = dsa_level::arb_only;
        }
    } else if ( ext_used ) {
        result = dsa_level::ext_only;
    } else {
        result = dsa_level::emulated;
    }

    if ( !inject_always_ ) {
        /* exit here if binding injectors are not required */
        if ( result == dsa_level::full_support ) {
            return result;
        }

        if ( result == dsa_level::arb_only ) {
            return result;
        }
    }

    glBindBuffer_original = RemapGLName( glBindBuffer );
    RemapGLName( glBindBuffer ) = glBindBuffer_injected;

    glBindFramebuffer_original = RemapGLName( glBindFramebuffer );
    RemapGLName( glBindFramebuffer ) = glBindFramebuffer_injected;

    glBindRenderbuffer_original = RemapGLName( glBindRenderbuffer );
    RemapGLName( glBindRenderbuffer ) = glBindRenderbuffer_injected;

    glBindTransformFeedback_original = RemapGLName( glBindTransformFeedback );
    RemapGLName( glBindTransformFeedback ) = glBindTransformFeedback_injected;

    glBindSampler_original = RemapGLName( glBindSampler );
    RemapGLName( glBindSampler ) = glBindSampler_injected;

    glBindProgramPipeline_original = RemapGLName( glBindProgramPipeline );
    RemapGLName( glBindProgramPipeline ) = glBindProgramPipeline_injected;

    glBindVertexArray_original = RemapGLName( glBindVertexArray );
    RemapGLName( glBindVertexArray ) = glBindVertexArray_injected;

    glBindTexture_original = RemapGLName( glBindTexture );
//    RemapGLName( glBindTexture ) = glBindTexture_injected;

    glActiveTexture_original = RemapGLName( glActiveTexture );
    RemapGLName( glActiveTexture ) = glActiveTexture_injected;

    return result;
}


void APIENTRY glBindBuffer_injected( GLenum target_, GLuint buffer_ ) {
    context& ctx = get_context();
    // element array binding is captured by vertex array objects
    if ( target_ == GL_ELEMENT_ARRAY_BUFFER && 0 != ctx._vertex_array ) {
        glBindBuffer_original( target_, buffer_ );
        return;
    }
    int index = remap_vertex_buffer_target( target_ );
    if ( ctx._vertex_buffers[index] != buffer_ ) {
        ctx._vertex_buffers[index] = buffer_;
        glBindBuffer_original( target_, buffer_ );
    }
}

void APIENTRY glBindFramebuffer_injected( GLenum target_, GLuint framebuffer_ ) {
    context& ctx = get_context();
    int index = remap_framebuffer_target( target_ );
    if ( ctx._framebuffers[index] != framebuffer_ ) {
        ctx._framebuffers[index] = framebuffer_;
        glBindFramebuffer_original( target_, framebuffer_ );
    }
}

void APIENTRY glBindRenderbuffer_injected( GLenum target_, GLuint renderbuffer_ ) {
    context& ctx = get_context();
    int index = remap_renderbuffer_target( target_ );
    if ( ctx._renderbuffers[index] != renderbuffer_ ) {
        ctx._renderbuffers[index] = renderbuffer_;
        glBindRenderbuffer_original( target_, renderbuffer_ );
    }
}

void APIENTRY glBindTransformFeedback_injected( GLenum target_, GLuint id_ ) {
    context& ctx = get_context();
    int index = remap_transform_feedback_target( target_ );
    if ( ctx._transform_feedbacks[index] != id_ ) {
        ctx._transform_feedbacks[index] = id_;
        glBindTransformFeedback_original( target_, id_ );
    }
}

void APIENTRY glBindSampler_injected( GLuint unit_, GLuint sampler_ ) {
    context& ctx = get_context();
    if ( ctx._samplers[unit_] != sampler_ ) {
        ctx._samplers[unit_] = sampler_;
        glBindSampler_original( unit_, sampler_ );
    }
}

void APIENTRY glBindProgramPipeline_injected( GLuint pipeline_ ) {
    context& ctx = get_context();
    if ( ctx._program_pipeline != pipeline_ ) {
        ctx._program_pipeline = pipeline_;
        glBindProgramPipeline_original( pipeline_ );
    }
}

void APIENTRY glBindVertexArray_injected( GLuint array_ ) {
    context& ctx = get_context();
    if ( ctx._vertex_array != array_ ) {
        ctx._vertex_array = array_;
        glBindVertexArray_original( array_ );
    }
}

void APIENTRY glBindTexture_injected( GLenum target_, GLuint texture_ ) {
    context& ctx = get_context();
    int index = remap_texture_target( target_ );
    if ( ctx._textures[ctx._active_texture_unit]._targets[index] != texture_ ) {
        ctx._textures[ctx._active_texture_unit]._targets[index] = texture_;
        glBindTexture_original( target_, texture_ );
    }
}

void APIENTRY glActiveTexture_injected( GLenum texture_ ) {
    context& ctx = get_context();
    int index = texture_ - GL_TEXTURE0;
    if ( ctx._active_texture_unit != index ) {
        ctx._active_texture_unit = index;
        glActiveTexture_original( texture_ );
    }
}


/* transform feedback */
void APIENTRY glCreateTransformFeedbacks_emulated( GLsizei n_, GLuint *ids_ ) {
    RemapGLName( glGenTransformFeedbacks )( n_, ids_ );
    for ( GLsizei i = 0; i < n_; ++i ) {
        RemapGLName( glBindTransformFeedback )( GL_TRANSFORM_FEEDBACK, ids_[i] );
    }
}
void APIENTRY glTransformFeedbackBufferBase_emulated( GLuint xfb_, GLuint index_, GLuint buffer_ ) {
    RemapGLName( glBindTransformFeedback )( GL_TRANSFORM_FEEDBACK, xfb_ );
    RemapGLName( glBindBufferBase )( GL_TRANSFORM_FEEDBACK_BUFFER, index_, buffer_ );
}
void APIENTRY glTransformFeedbackBufferRange_emulated( GLuint xfb_, GLuint index_, GLuint buffer_, GLintptr offset_, GLsizei size_ ) {
    RemapGLName( glBindTransformFeedback )( GL_TRANSFORM_FEEDBACK, xfb_ );
    RemapGLName( glBindBufferRange )( GL_TRANSFORM_FEEDBACK_BUFFER, index_, buffer_, offset_, size_ );
}
void APIENTRY glGetTransformFeedbackiv_emulated( GLuint xfb_, GLenum pname_, GLint *param_ ) {
    RemapGLName( glBindTransformFeedback )( GL_TRANSFORM_FEEDBACK, xfb_ );
    RemapGLName( glGetIntegerv )( pname_, param_ );
}
void APIENTRY glGetTransformFeedbacki_v_emulated( GLuint xfb_, GLenum pname_, GLuint index_, GLint *param_ ) {
    RemapGLName( glBindTransformFeedback )( GL_TRANSFORM_FEEDBACK, xfb_ );
    RemapGLName( glGetIntegeri_v )( pname_, index_, param_ );
}
void APIENTRY glGetTransformFeedbacki64_v_emulated( GLuint xfb_, GLenum pname_, GLuint index_, GLint64 *param_ ) {
    RemapGLName( glBindTransformFeedback )( GL_TRANSFORM_FEEDBACK, xfb_ );
    RemapGLName( glGetInteger64i_v )( pname_, index_, param_ );
}
/* vertex buffer object */
void APIENTRY glCreateBuffers_emulated( GLsizei n_, GLuint *buffers_ ) {
    RemapGLName( glGenBuffers )( n_, buffers_ );
    for ( GLsizei i = 0; i < n_; ++i ) {
        RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, buffers_[i] );
    }
}
void APIENTRY glNamedBufferStorage_emulated( GLuint buffer_, GLsizei size_, const GLvoid *data_, GLbitfield flags_ ) {
    RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, buffer_ );
    RemapGLName( glBufferStorage )( GL_COPY_READ_BUFFER, size_, data_, flags_ );
}
void APIENTRY glNamedBufferData_emulated( GLuint buffer_, GLsizei size_, const GLvoid *data_, GLenum usage_ ) {
    RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, buffer_ );
    RemapGLName( glBufferData )( GL_COPY_READ_BUFFER, size_, data_, usage_ );
}
void APIENTRY glNamedBufferSubData_emulated( GLuint buffer_, GLintptr offset_, GLsizei size_, const GLvoid *data_ ) {
    RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, buffer_ );
    RemapGLName( glBufferSubData )( GL_COPY_READ_BUFFER, offset_, size_, data_ );
}
void APIENTRY glCopyNamedBufferSubData_emulated( GLuint readBuffer_, GLuint writeBuffer_, GLintptr readOffset_, GLintptr writeOffset_, GLsizei size_ ) {
    RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, readBuffer_ );
    RemapGLName( glBindBuffer )( GL_COPY_WRITE_BUFFER, writeBuffer_ );
    RemapGLName( glCopyBufferSubData )( GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, readOffset_, writeOffset_, size_ );
}
void APIENTRY glClearNamedBufferData_emulated( GLuint buffer_, GLenum internalformat_, GLenum format_, GLenum type_, const GLvoid *data_ ) {
    RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, buffer_ );
    RemapGLName( glClearBufferData )( GL_COPY_READ_BUFFER, internalformat_, format_, type_, data_ );
}
void APIENTRY glClearNamedBufferSubData_emulated( GLuint buffer_, GLenum internalformat_, GLintptr offset_, GLsizei size_, GLenum format_, GLenum type_, const GLvoid *data_ ) {
    RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, buffer_ );
    RemapGLName( glClearBufferSubData )( GL_COPY_READ_BUFFER, internalformat_, offset_, size_, format_, type_, data_ );
}
void* APIENTRY glMapNamedBuffer_emulated( GLuint buffer_, GLenum access_ ) {
    RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, buffer_ );
    return RemapGLName( glMapBuffer )( GL_COPY_READ_BUFFER, access_ );
}
void* APIENTRY glMapNamedBufferRange_emulated( GLuint buffer_, GLintptr offset_, GLsizei length_, GLbitfield access_ ) {
    RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, buffer_ );
    return RemapGLName( glMapBufferRange )( GL_COPY_READ_BUFFER, offset_, length_, access_ );
}
GLboolean APIENTRY glUnmapNamedBuffer_emulated( GLuint buffer_ ) {
    RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, buffer_ );
    return RemapGLName( glUnmapBuffer )( GL_COPY_READ_BUFFER );
}
void APIENTRY glFlushMappedNamedBufferRange_emulated( GLuint buffer_, GLintptr offset_, GLsizei length_ ) {
    RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, buffer_ );
    RemapGLName( glFlushMappedBufferRange )( GL_COPY_READ_BUFFER, offset_, length_ );
}
void APIENTRY glGetNamedBufferParameteriv_emulated( GLuint buffer_, GLenum pname_, GLint *params_ ) {
    RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, buffer_ );
    RemapGLName( glGetBufferParameteriv )( GL_COPY_READ_BUFFER, pname_, params_ );
}
void APIENTRY glGetNamedBufferParameteri64v_emulated( GLuint buffer_, GLenum pname_, GLint64 *params_ ) {
    RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, buffer_ );
    RemapGLName( glGetBufferParameteri64v )( GL_COPY_READ_BUFFER, pname_, params_ );
}
void APIENTRY glGetNamedBufferPointerv_emulated( GLuint buffer_, GLenum pname_, GLvoid **params_ ) {
    RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, buffer_ );
    RemapGLName( glGetBufferPointerv )( GL_COPY_READ_BUFFER, pname_, params_ );
}
void APIENTRY glGetNamedBufferSubData_emulated( GLuint buffer_, GLintptr offset_, GLsizei size_, GLvoid *data_ ) {
    RemapGLName( glBindBuffer )( GL_COPY_READ_BUFFER, buffer_ );
    RemapGLName( glGetBufferSubData )( GL_COPY_READ_BUFFER, offset_, size_, data_ );
}
/* framebuffer object */
void APIENTRY glCreateFramebuffers_emulated( GLsizei n_, GLuint *framebuffers_ ) {
    RemapGLName( glGenFramebuffers )( n_, framebuffers_ );
    for ( GLsizei i = 0; i < n_; ++i ) {
        RemapGLName( glBindFramebuffer )( GL_READ_FRAMEBUFFER, framebuffers_[i] );
    }
}
void APIENTRY glNamedFramebufferRenderbuffer_emulated( GLuint framebuffer_, GLenum attachment_, GLenum renderbuffertarget_, GLuint renderbuffer_ ) {
    RemapGLName( glBindFramebuffer )( GL_READ_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glFramebufferRenderbuffer )( GL_READ_FRAMEBUFFER, attachment_, renderbuffertarget_, renderbuffer_ );
}
void APIENTRY glNamedFramebufferParameteri_emulated( GLuint framebuffer_, GLenum pname_, GLint param_ ) {
    RemapGLName( glBindFramebuffer )( GL_READ_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glFramebufferParameteri )( GL_READ_FRAMEBUFFER, pname_, param_ );
}
void APIENTRY glNamedFramebufferTexture_emulated( GLuint framebuffer_, GLenum attachment_, GLuint texture_, GLint level_ ) {
    RemapGLName( glBindFramebuffer )( GL_READ_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glFramebufferTexture )( GL_READ_FRAMEBUFFER, attachment_, texture_, level_ );
}
void APIENTRY glNamedFramebufferTextureLayer_emulated( GLuint framebuffer_, GLenum attachment_, GLuint texture_, GLint level_, GLint layer_ ) {
    RemapGLName( glBindFramebuffer )( GL_READ_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glFramebufferTextureLayer )( GL_READ_FRAMEBUFFER, attachment_, texture_, level_, layer_ );
}
void APIENTRY glNamedFramebufferDrawBuffer_emulated( GLuint framebuffer_, GLenum mode_ ) {
    RemapGLName( glBindFramebuffer )( GL_READ_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glDrawBuffer )( mode_ );
}
void APIENTRY glNamedFramebufferDrawBuffers_emulated( GLuint framebuffer_, GLsizei n_, const GLenum *bufs_ ) {
    RemapGLName( glBindFramebuffer )( GL_READ_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glDrawBuffers )( n_, bufs_ );
}
void APIENTRY glNamedFramebufferReadBuffer_emulated( GLuint framebuffer_, GLenum mode_ ) {
    RemapGLName( glBindFramebuffer )( GL_READ_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glReadBuffer )( mode_ );
}
void APIENTRY glInvalidateNamedFramebufferData_emulated( GLuint framebuffer_, GLsizei numAttachments_, const GLenum *attachments_ ) {
    RemapGLName( glBindFramebuffer )( GL_READ_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glInvalidateFramebuffer )( GL_READ_FRAMEBUFFER, numAttachments_, attachments_ );
}
void APIENTRY glInvalidateNamedFramebufferSubData_emulated( GLuint framebuffer_, GLsizei numAttachments_, const GLenum *attachments_, GLint x_, GLint y_, GLsizei width_, GLsizei height_ ) {
    RemapGLName( glBindFramebuffer )( GL_READ_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glInvalidateSubFramebuffer )( GL_READ_FRAMEBUFFER, numAttachments_, attachments_, x_, y_, width_, height_ );
}
void APIENTRY glClearNamedFramebufferiv_emulated( GLuint framebuffer_, GLenum buffer_, GLint drawbuffer_, const GLint *value_ ) {
    RemapGLName( glBindFramebuffer )( GL_DRAW_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glClearBufferiv )( buffer_, drawbuffer_, value_ );
}
void APIENTRY glClearNamedFramebufferuiv_emulated( GLuint framebuffer_, GLenum buffer_, GLint drawbuffer_, const GLuint *value_ ) {
    RemapGLName( glBindFramebuffer )( GL_DRAW_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glClearBufferuiv )( buffer_, drawbuffer_, value_ );
}
void APIENTRY glClearNamedFramebufferfv_emulated( GLuint framebuffer_, GLenum buffer_, GLint drawbuffer_, const GLfloat *value_ ) {
    RemapGLName( glBindFramebuffer )( GL_DRAW_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glClearBufferfv )( buffer_, drawbuffer_, value_ );
}
void APIENTRY glClearNamedFramebufferfi_emulated( GLuint framebuffer_, GLenum buffer_, GLfloat depth_, GLint stencil_ ) {
    RemapGLName( glBindFramebuffer )( GL_DRAW_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glClearBufferfi )( buffer_, 0, depth_, stencil_ );
}
void APIENTRY glBlitNamedFramebuffer_emulated( GLuint readFramebuffer_, GLuint drawFramebuffer_, GLint srcX0_, GLint srcY0_, GLint srcX1_, GLint srcY1_, GLint dstX0_, GLint dstY0_, GLint dstX1_, GLint dstY1_, GLbitfield mask_, GLenum filter_ ) {
    RemapGLName( glBindFramebuffer )( GL_READ_FRAMEBUFFER, readFramebuffer_ );
    RemapGLName( glBindFramebuffer )( GL_DRAW_FRAMEBUFFER, drawFramebuffer_ );
    RemapGLName( glBlitFramebuffer )( srcX0_, srcY0_, srcX1_, srcY1_, dstX0_, dstY0_, dstX1_, dstY1_, mask_, filter_ );
}
GLenum APIENTRY glCheckNamedFramebufferStatus_emulated( GLuint framebuffer_, GLenum target_ ) {
    RemapGLName( glBindFramebuffer )( target_, framebuffer_ );
    return RemapGLName( glCheckFramebufferStatus )( target_ );
}
void APIENTRY glGetNamedFramebufferParameteriv_emulated( GLuint framebuffer_, GLenum pname_, GLint *param_ ) {
    RemapGLName( glBindFramebuffer )( GL_READ_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glGetFramebufferParameteriv )( GL_READ_FRAMEBUFFER, pname_, param_ );
}
void APIENTRY glGetNamedFramebufferAttachmentParameteriv_emulated( GLuint framebuffer_, GLenum attachment_, GLenum pname_, GLint *params_ ) {
    RemapGLName( glBindFramebuffer )( GL_READ_FRAMEBUFFER, framebuffer_ );
    RemapGLName( glGetFramebufferAttachmentParameteriv )( GL_READ_FRAMEBUFFER, attachment_, pname_, params_ );
}
/* Renderbuffer object functions */
void APIENTRY glCreateRenderbuffers_emulated( GLsizei n_, GLuint *renderbuffers_ ) {
    RemapGLName( glGenRenderbuffers )( n_, renderbuffers_ );
    for ( GLsizei i = 0; i < n_; ++i ) {
        RemapGLName( glBindRenderbuffer )( GL_RENDERBUFFER, renderbuffers_[i] );
    }
}
void APIENTRY glNamedRenderbufferStorage_emulated( GLuint renderbuffer_, GLenum internalformat_, GLsizei width_, GLsizei height_ ) {
    RemapGLName( glBindRenderbuffer )( GL_RENDERBUFFER, renderbuffer_ );
    RemapGLName( glRenderbufferStorage )( GL_RENDERBUFFER, internalformat_, width_, height_ );
}
void APIENTRY glNamedRenderbufferStorageMultisample_emulated( GLuint renderbuffer_, GLsizei samples_, GLenum internalformat_, GLsizei width_, GLsizei height_ ) {
    RemapGLName( glBindRenderbuffer )( GL_RENDERBUFFER, renderbuffer_ );
    RemapGLName( glRenderbufferStorageMultisample )( GL_RENDERBUFFER, samples_, internalformat_, width_, height_ );
}
void APIENTRY glGetNamedRenderbufferParameteriv_emulated( GLuint renderbuffer_, GLenum pname_, GLint *params_ ) {
    RemapGLName( glBindRenderbuffer )( GL_RENDERBUFFER, renderbuffer_ );
    RemapGLName( glGetRenderbufferParameteriv )( GL_RENDERBUFFER, pname_, params_ );
}
/* Sampler object functions */
void APIENTRY glCreateSamplers_emulated( GLsizei n_, GLuint *samplers_ ) {
    RemapGLName( glGenSamplers )( n_, samplers_ );
    for ( GLsizei i = 0; i < n_; ++i ) {
        RemapGLName( glBindSampler )( 0, samplers_[i] );
    }
}
/* Program Pipeline object functions */
void APIENTRY glCreateProgramPipelines_emulated( GLsizei n_, GLuint* pipelines_ ) {
    RemapGLName( glGenProgramPipelines )( n_, pipelines_ );
    for ( GLsizei i = 0; i < n_; ++i ) {
        RemapGLName( glBindProgramPipeline )( pipelines_[i] );
    }
}
/* Query object functions */
void APIENTRY glCreateQueries_emulated( GLenum target_, GLsizei n_, GLuint* ids_ ) {
    RemapGLName( glGenQueries )( n_, ids_ );
    for ( GLsizei i = 0; i < n_; ++i ) {
        RemapGLName( glBeginQuery )( target_, ids_[i] );
        RemapGLName( glEndQuery )( target_ );
    }
}
/* Vertex Array object functions */
void APIENTRY glCreateVertexArrays_emulated( GLsizei n_, GLuint *arrays_ ) {
    RemapGLName( glGenVertexArrays )( n_, arrays_ );
    for ( GLsizei i = 0; i < n_; ++i ) {
        RemapGLName( glBindVertexArray )( arrays_[i] );
    }
}
void APIENTRY glDisableVertexArrayAttrib_emulated( GLuint vaobj_, GLuint index_ ) {
    RemapGLName( glBindVertexArray )( vaobj_ );
    RemapGLName( glDisableVertexAttribArray ) ( index_ );
}
void APIENTRY glEnableVertexArrayAttrib_emulated( GLuint vaobj_, GLuint index_ ) {
    RemapGLName( glBindVertexArray )( vaobj_ );
    RemapGLName( glEnableVertexAttribArray ) ( index_ );
}
void APIENTRY glVertexArrayElementBuffer_emulated( GLuint vaobj_, GLuint buffer_ ) {
    RemapGLName( glBindVertexArray )( vaobj_ );
    RemapGLName( glBindBuffer )( GL_ELEMENT_ARRAY_BUFFER, buffer_ );
}
void APIENTRY glVertexArrayVertexBuffer_emulated( GLuint vaobj_, GLuint bindingindex_, GLuint buffer_, GLintptr offset_, GLsizei stride_ ) {
    RemapGLName( glBindVertexArray )( vaobj_ );
    RemapGLName( glBindVertexBuffer )( bindingindex_, buffer_, offset_, stride_ );
}
void APIENTRY glVertexArrayVertexBuffers_emulated( GLuint vaobj_, GLuint first_, GLsizei count_, const GLuint *buffers_, const GLintptr *offsets_, const GLsizei *strides_ ) {
    RemapGLName( glBindVertexArray )( vaobj_ );
    RemapGLName( glBindVertexBuffers )( first_, count_, buffers_, offsets_, strides_ );
}
void APIENTRY glVertexArrayAttribFormat_emulated( GLuint vaobj_, GLuint attribindex_, GLint size_, GLenum type_, GLboolean normalized_, GLuint relativeoffset_ ) {
    RemapGLName( glBindVertexArray )( vaobj_ );
    RemapGLName( glVertexAttribFormat )( attribindex_, size_, type_, normalized_, relativeoffset_ );
}
void APIENTRY glVertexArrayAttribIFormat_emulated( GLuint vaobj_, GLuint attribindex_, GLint size_, GLenum type_, GLuint relativeoffset_ ) {
    RemapGLName( glBindVertexArray )( vaobj_ );
    RemapGLName( glVertexAttribIFormat )( attribindex_, size_, type_, relativeoffset_ );
}
void APIENTRY glVertexArrayAttribLFormat_emulated( GLuint vaobj_, GLuint attribindex_, GLint size_, GLenum type_, GLuint relativeoffset_ ) {
    RemapGLName( glBindVertexArray )( vaobj_ );
    RemapGLName( glVertexAttribLFormat )( attribindex_, size_, type_, relativeoffset_ );
}
void APIENTRY glVertexArrayAttribBinding_emulated( GLuint vaobj_, GLuint attribindex_, GLuint bindingindex_ ) {
    RemapGLName( glBindVertexArray )( vaobj_ );
    RemapGLName( glVertexAttribBinding )( attribindex_, bindingindex_ );
}
void APIENTRY glVertexArrayBindingDivisor_emulated( GLuint vaobj_, GLuint bindingindex_, GLuint divisor_ ) {
    RemapGLName( glBindVertexArray )( vaobj_ );
    RemapGLName( glVertexBindingDivisor )( bindingindex_, divisor_ );
}
void APIENTRY glGetVertexArrayiv_emulated( GLuint vaobj_, GLenum pname_, GLint *param_ ) {
    RemapGLName( glBindVertexArray )( vaobj_ );
    RemapGLName( glGetIntegerv )( pname_, param_ );
}
void APIENTRY glGetVertexArrayIndexediv_emulated( GLuint vaobj_, GLuint index_, GLenum pname_, GLint *param_ ) {
    RemapGLName( glBindVertexArray )( vaobj_ );
    RemapGLName( glGetVertexAttribiv )( index_, pname_, param_ );
}
void APIENTRY glGetVertexArrayIndexed64iv_emulated( GLuint vaobj_, GLuint index_, GLenum pname_, GLint64 *param_ ) {
    RemapGLName( glBindVertexArray )( vaobj_ );
    RemapGLName( glGetInteger64i_v )( index_, pname_, param_ );
}
/* Texture object functions */
void APIENTRY glCreateTextures_emulated( GLenum target_, GLsizei n_, GLuint *textures_ ) {
    RemapGLName( glGenTextures )( n_, textures_ );
    for ( GLsizei i = 0; i < n_; ++i ) {
        RemapGLName( glBindTexture )( target_, textures_[i] );
    }
}
void APIENTRY glTextureBufferEXT_emulated( GLuint texture_, GLenum target_, GLenum internalformat_, GLuint buffer_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexBuffer ) ( target_, internalformat_, buffer_ );
}
void APIENTRY glTextureBufferRangeEXT_emulated( GLuint texture_, GLenum target_, GLenum internalformat_, GLuint buffer_, GLintptr offset_, GLsizeiptr size_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexBufferRange ) ( target_, internalformat_, buffer_, offset_, size_ );
}
void APIENTRY glTextureStorage1DEXT_emulated( GLuint texture_, GLenum target_, GLsizei levels_, GLenum internalformat_, GLsizei width_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexStorage1D ) ( target_, levels_, internalformat_, width_ );
}
void APIENTRY glTextureStorage2DEXT_emulated( GLuint texture_, GLenum target_, GLsizei levels_, GLenum internalformat_, GLsizei width_, GLsizei height_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexStorage2D ) ( target_, levels_, internalformat_, width_, height_ );
}
void APIENTRY glTextureStorage3DEXT_emulated( GLuint texture_, GLenum target_, GLsizei levels_, GLenum internalformat_, GLsizei width_, GLsizei height_, GLsizei depth_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexStorage3D ) ( target_, levels_, internalformat_, width_, height_, depth_ );
}
void APIENTRY glTextureStorage2DMultisampleEXT_emulated( GLuint texture_, GLenum target_, GLsizei levels_, GLenum internalformat_, GLsizei width_, GLsizei height_, GLboolean fixedsamplelocations_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexStorage2DMultisample ) ( target_, levels_, internalformat_, width_, height_, fixedsamplelocations_ );
}
void APIENTRY glTextureStorage3DMultisampleEXT_emulated( GLuint texture_, GLenum target_, GLsizei levels_, GLenum internalformat_, GLsizei width_, GLsizei height_, GLsizei depth_, GLboolean fixedsamplelocations_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexStorage3DMultisample ) ( target_, levels_, internalformat_, width_, height_, depth_, fixedsamplelocations_ );
}
void APIENTRY glTextureSubImage1DEXT_emulated( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLsizei width_, GLenum format_, GLenum type_, const void *pixels_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexSubImage1D ) ( target_, level_, xoffset_, width_, format_, type_, pixels_ );
}
void APIENTRY glTextureSubImage2DEXT_emulated( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLint yoffset_, GLsizei width_, GLsizei height_, GLenum format_, GLenum type_, const void *pixels_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexSubImage2D ) ( target_, level_, xoffset_, yoffset_, width_, height_, format_, type_, pixels_ );
}
void APIENTRY glTextureSubImage3DEXT_emulated( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLint yoffset_, GLint zoffset_, GLsizei width_, GLsizei height_, GLsizei depth_, GLenum format_, GLenum type_, const GLvoid *pixels_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexSubImage3D ) ( target_, level_, xoffset_, yoffset_, zoffset_, width_, height_, depth_, format_, type_, pixels_ );
}
void APIENTRY glCompressedTextureSubImage1DEXT_emulated( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLsizei width_, GLenum format_, GLsizei imageSize_, const void *bits_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glCompressedTexSubImage1D )( target_, level_, xoffset_, width_, format_, imageSize_, bits_ );
}
void APIENTRY glCompressedTextureSubImage2DEXT_emulated( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLint yoffset_, GLsizei width_, GLsizei height_, GLenum format_, GLsizei imageSize_, const void *bits_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glCompressedTexSubImage2D )( target_, level_, xoffset_, yoffset_, width_, height_, format_, imageSize_, bits_ );
}
void APIENTRY glCompressedTextureSubImage3DEXT_emulated( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLint yoffset_, GLint zoffset_, GLsizei width_, GLsizei height_, GLsizei depth_, GLenum format_, GLsizei imageSize_, const void *bits_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glCompressedTexSubImage3D )( target_, level_, xoffset_, yoffset_, zoffset_, width_, height_, depth_, format_, imageSize_, bits_ );
}
void APIENTRY glCopyTextureSubImage1DEXT_emulated( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLint x_, GLint y_, GLsizei width_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glCopyTexSubImage1D )( target_, level_, xoffset_, x_, y_, width_ );
}
void APIENTRY glCopyTextureSubImage2DEXT_emulated( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLint yoffset_, GLint x_, GLint y_, GLsizei width_, GLsizei height_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glCopyTexSubImage2D )( target_, level_, xoffset_, yoffset_, x_, y_, width_, height_ );
}
void APIENTRY glCopyTextureSubImage3DEXT_emulated( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLint yoffset_, GLint zoffset_, GLint x_, GLint y_, GLsizei width_, GLsizei height_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glCopyTexSubImage3D )( target_, level_, xoffset_, yoffset_, zoffset_, x_, y_, width_, height_ );
}
void APIENTRY glTextureParameterfEXT_emulated( GLuint texture_, GLenum target_, GLenum pname_, GLfloat param_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexParameterf )( target_, pname_, param_ );
}
void APIENTRY glTextureParameterfvEXT_emulated( GLuint texture_, GLenum target_, GLenum pname_, const GLfloat *params_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexParameterfv )( target_, pname_, params_ );
}
void APIENTRY glTextureParameteriEXT_emulated( GLuint texture_, GLenum target_, GLenum pname_, GLint param_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexParameteri )( target_, pname_, param_ );
}
void APIENTRY glTextureParameterivEXT_emulated( GLuint texture_, GLenum target_, GLenum pname_, const GLint *params_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexParameteriv )( target_, pname_, params_ );
}
void APIENTRY glTextureParameterIivEXT_emulated( GLuint texture_, GLenum target_, GLenum pname_, const GLint *params_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexParameterIiv )( target_, pname_, params_ );
}
void APIENTRY glTextureParameterIuivEXT_emulated( GLuint texture_, GLenum target_, GLenum pname_, const GLuint *params_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glTexParameterIuiv )( target_, pname_, params_ );
}
void APIENTRY glGenerateTextureMipmapEXT_emulated( GLuint texture_, GLenum target_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glGenerateMipmap )( target_ );
}
void APIENTRY glBindMultiTextureEXT_emulated( GLenum texunit_, GLenum target_, GLuint texture_ ) {
    RemapGLName( glActiveTexture ) ( texunit_ );
    RemapGLName( glBindTexture )( target_, texture_ );
}
void APIENTRY glGetTextureImageEXT_emulated( GLuint texture_, GLenum target_, GLint level_, GLenum format_, GLenum type_, void *pixels_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glGetTexImage )( target_, level_, format_, type_, pixels_ );
}
void APIENTRY glGetCompressedTextureImageEXT_emulated( GLuint texture_, GLenum target_, GLint lod_, void *img_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glGetCompressedTexImage )( target_, lod_, img_ );
}
void APIENTRY glGetTextureLevelParameterfvEXT_emulated( GLuint texture_, GLenum target_, GLint level_, GLenum pname_, GLfloat *params_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glGetTexLevelParameterfv )( target_, level_, pname_, params_ );
}
void APIENTRY glGetTextureLevelParameterivEXT_emulated( GLuint texture_, GLenum target_, GLint level_, GLenum pname_, GLint *params_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glGetTexLevelParameteriv )( target_, level_, pname_, params_ );
}
void APIENTRY glGetTextureParameterfvEXT_emulated( GLuint texture_, GLenum target_, GLenum pname_, GLfloat *params_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glGetTexParameterfv )( target_, pname_, params_ );
}
void APIENTRY glGetTextureParameterivEXT_emulated( GLuint texture_, GLenum target_, GLenum pname_, GLint *params_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glGetTexParameteriv )( target_, pname_, params_ );
}
void APIENTRY glGetTextureParameterIivEXT_emulated( GLuint texture_, GLenum target_, GLenum pname_, GLint *params_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glGetTexParameterIiv )( target_, pname_, params_ );
}
void APIENTRY glGetTextureParameterIuivEXT_emulated( GLuint texture_, GLenum target_, GLenum pname_, GLuint *params_ ) {
    RemapGLName( glBindTexture )( target_, texture_ );
    RemapGLName( glGetTexParameterIuiv )( target_, pname_, params_ );
}

/* vertex buffer object */
void APIENTRY glNamedBufferStorage_EXT( GLuint buffer_, GLsizei size_, const GLvoid *data_, GLbitfield flags_ ) {
    RemapGLName( glNamedBufferStorageEXT )( buffer_, size_, data_, flags_ );
}
void APIENTRY glNamedBufferData_EXT( GLuint buffer_, GLsizei size_, const GLvoid *data_, GLenum usage_ ) {
    RemapGLName( glNamedBufferDataEXT )( buffer_, size_, data_, usage_ );
}
void APIENTRY glNamedBufferSubData_EXT( GLuint buffer_, GLintptr offset_, GLsizei size_, const GLvoid *data_ ) {
    RemapGLName( glNamedBufferSubDataEXT ) ( buffer_, offset_, size_, data_ );
}
void APIENTRY glCopyNamedBufferSubData_EXT( GLuint readBuffer_, GLuint writeBuffer_, GLintptr readOffset_, GLintptr writeOffset_, GLsizei size_ ) {
    RemapGLName( glNamedCopyBufferSubDataEXT )( readBuffer_, writeBuffer_, readOffset_, writeBuffer_, size_ );
}
void* APIENTRY glMapNamedBufferRange_EXT( GLuint buffer_, GLintptr offset_, GLsizei length_, GLbitfield access_ ) {
    return RemapGLName( glMapNamedBufferRangeEXT ) ( buffer_, offset_, length_, access_ );
}
void APIENTRY glFlushMappedNamedBufferRange_EXT( GLuint buffer_, GLintptr offset_, GLsizei length_ ) {
    RemapGLName( glFlushMappedNamedBufferRangeEXT ) ( buffer_, offset_, length_ );
}
void APIENTRY glGetNamedBufferSubData_EXT( GLuint buffer_, GLintptr offset_, GLsizei size_, GLvoid *data_ ) {
    RemapGLName( glGetNamedBufferSubDataEXT ) ( buffer_, offset_, size_, data_ );
}

/* Vertex Array object functions */
void APIENTRY glVertexArrayVertexBuffers_EXT( GLuint vaobj_, GLuint first_, GLsizei count_, const GLuint *buffers_, const GLintptr *offsets_, const GLsizei *strides_ ) {
    for ( GLsizei i = 0; i < count_; ++i ) {
        RemapGLName( glVertexArrayVertexBuffer )( vaobj_, first_ + i, buffers_[i], offsets_[i], strides_[i] );
    }
}

/* Texture object functions */
void APIENTRY glTextureBufferEXT_ARB( GLuint texture_, GLenum target_, GLenum internalformat_, GLuint buffer_ ) {
    (void)target_;
    RemapGLName( glTextureBuffer )( texture_, internalformat_, buffer_ );
}
void APIENTRY glTextureBufferRangeEXT_ARB( GLuint texture_, GLenum target_, GLenum internalformat_, GLuint buffer_, GLintptr offset_, GLsizeiptr size_ ) {
    (void)target_;
#if PTRDIFF_MAX != INT_MAX
    // in x64 GLsizeiptr is 64bit and GLsizei is 32bit
    assert( size_ <= INT_MAX );
#if !defined(GL_DSA_SUPPRESS_64BIT_WARNINGS)
#pragma message("OpenGL direct state system: narrowing from GLsizeiptr to GLsizei, define GL_DSA_SUPPRESS_64BIT_WARNINGS to disable this message")
#endif
#endif
    RemapGLName( glTextureBufferRange )( texture_, internalformat_, buffer_, offset_, static_cast<GLsizei>( size_ ) );
}
void APIENTRY glTextureStorage1DEXT_ARB( GLuint texture_, GLenum target_, GLsizei levels_, GLenum internalformat_, GLsizei width_ ) {
    (void)target_;
    RemapGLName( glTextureStorage1D )( texture_, levels_, internalformat_, width_ );
}
void APIENTRY glTextureStorage2DEXT_ARB( GLuint texture_, GLenum target_, GLsizei levels_, GLenum internalformat_, GLsizei width_, GLsizei height_ ) {
    (void)target_;
    RemapGLName( glTextureStorage2D )( texture_, levels_, internalformat_, width_, height_ );
}
void APIENTRY glTextureStorage3DEXT_ARB( GLuint texture_, GLenum target_, GLsizei levels_, GLenum internalformat_, GLsizei width_, GLsizei height_, GLsizei depth_ ) {
    (void)target_;
    RemapGLName( glTextureStorage3D )( texture_, levels_, internalformat_, width_, height_, depth_ );
}
void APIENTRY glTextureStorage2DMultisampleEXT_ARB( GLuint texture_, GLenum target_, GLsizei levels_, GLenum internalformat_, GLsizei width_, GLsizei height_, GLboolean fixedsamplelocations_ ) {
    (void)target_;
    RemapGLName( glTextureStorage2DMultisample )( texture_, levels_, internalformat_, width_, height_, fixedsamplelocations_ );
}
void APIENTRY glTextureStorage3DMultisampleEXT_ARB( GLuint texture_, GLenum target_, GLsizei levels_, GLenum internalformat_, GLsizei width_, GLsizei height_, GLsizei depth_, GLboolean fixedsamplelocations_ ) {
    (void)target_;
    RemapGLName( glTextureStorage3DMultisample )( texture_, levels_, internalformat_, width_, height_, depth_, fixedsamplelocations_ );
}
void APIENTRY glTextureSubImage1DEXT_ARB( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLsizei width_, GLenum format_, GLenum type_, const void *pixels_ ) {
    (void)target_;
    RemapGLName( glTextureSubImage1D )( texture_, level_, xoffset_, width_, format_, type_, pixels_ );
}
void APIENTRY glTextureSubImage2DEXT_ARB( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLint yoffset_, GLsizei width_, GLsizei height_, GLenum format_, GLenum type_, const void *pixels_ ) {
    if ( target_ >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target_ <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z ) {
        RemapGLName( glTextureSubImage3D )( texture_, level_, xoffset_, yoffset_, target_ - GL_TEXTURE_CUBE_MAP_POSITIVE_X, width_, height_, 1, format_, type_, pixels_ );
    } else {
        RemapGLName( glTextureSubImage2D )( texture_, level_, xoffset_, yoffset_, width_, height_, format_, type_, pixels_ );
    }
}
void APIENTRY glTextureSubImage3DEXT_ARB( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLint yoffset_, GLint zoffset_, GLsizei width_, GLsizei height_, GLsizei depth_, GLenum format_, GLenum type_, const GLvoid *pixels_ ) {
    (void)target_;
    RemapGLName( glTextureSubImage3D )( texture_, level_, xoffset_, yoffset_, zoffset_, width_, height_, depth_, format_, type_, pixels_ );
}
void APIENTRY glCompressedTextureSubImage1DEXT_ARB( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLsizei width_, GLenum format_, GLsizei imageSize_, const void *bits_ ) {
    (void)target_;
    RemapGLName( glCompressedTextureSubImage1D )( texture_, level_, xoffset_, width_, format_, imageSize_, bits_ );
}
void APIENTRY glCompressedTextureSubImage2DEXT_ARB( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLint yoffset_, GLsizei width_, GLsizei height_, GLenum format_, GLsizei imageSize_, const void *bits_ ) {
    if ( target_ >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target_ <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z ) {
        RemapGLName( glCompressedTextureSubImage3D )( texture_, level_, xoffset_, yoffset_, target_ - GL_TEXTURE_CUBE_MAP_POSITIVE_X, width_, height_, 1, format_, imageSize_, bits_ );
    } else {
        RemapGLName( glCompressedTextureSubImage2D )( texture_, level_, xoffset_, yoffset_, width_, height_, format_, imageSize_, bits_ );
    }
}
void APIENTRY glCompressedTextureSubImage3DEXT_ARB( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLint yoffset_, GLint zoffset_, GLsizei width_, GLsizei height_, GLsizei depth_, GLenum format_, GLsizei imageSize_, const void *bits_ ) {
    (void)target_;
    RemapGLName( glCompressedTextureSubImage3D )( texture_, level_, xoffset_, yoffset_, zoffset_, width_, height_, depth_, format_, imageSize_, bits_ );
}
void APIENTRY glCopyTextureSubImage1DEXT_ARB( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLint x_, GLint y_, GLsizei width_ ) {
    (void)target_;
    RemapGLName( glCopyTextureSubImage1D )( texture_, level_, xoffset_, x_, y_, width_ );
}
void APIENTRY glCopyTextureSubImage2DEXT_ARB( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLint yoffset_, GLint x_, GLint y_, GLsizei width_, GLsizei height_ ) {
    if ( target_ >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target_ <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z ) {
        RemapGLName( glCopyTextureSubImage3D )( texture_, level_, xoffset_, yoffset_, target_ - GL_TEXTURE_CUBE_MAP_POSITIVE_X, x_, y_, width_, height_ );
    } else {
        RemapGLName( glCopyTextureSubImage2D )( texture_, level_, xoffset_, yoffset_, x_, y_, width_, height_ );
    }
}
void APIENTRY glCopyTextureSubImage3DEXT_ARB( GLuint texture_, GLenum target_, GLint level_, GLint xoffset_, GLint yoffset_, GLint zoffset_, GLint x_, GLint y_, GLsizei width_, GLsizei height_ ) {
    (void)target_;
    RemapGLName( glCopyTextureSubImage3D )( texture_, level_, xoffset_, yoffset_, zoffset_, x_, y_, width_, height_ );
}
void APIENTRY glTextureParameterfEXT_ARB( GLuint texture_, GLenum target_, GLenum pname_, GLfloat param_ ) {
    (void)target_;
    RemapGLName( glTextureParameterf )( texture_, pname_, param_ );
}
void APIENTRY glTextureParameterfvEXT_ARB( GLuint texture_, GLenum target_, GLenum pname_, const GLfloat *params_ ) {
    (void)target_;
    RemapGLName( glTextureParameterfv )( texture_, pname_, params_ );
}
void APIENTRY glTextureParameteriEXT_ARB( GLuint texture_, GLenum target_, GLenum pname_, GLint param_ ) {
    (void)target_;
    RemapGLName( glTextureParameteri )( texture_, pname_, param_ );
}
void APIENTRY glTextureParameterivEXT_ARB( GLuint texture_, GLenum target_, GLenum pname_, const GLint *params_ ) {
    (void)target_;
    RemapGLName( glTextureParameteriv )( texture_, pname_, params_ );
}
void APIENTRY glTextureParameterIivEXT_ARB( GLuint texture_, GLenum target_, GLenum pname_, const GLint *params_ ) {
    (void)target_;
    RemapGLName( glTextureParameterIiv )( texture_, pname_, params_ );
}
void APIENTRY glTextureParameterIuivEXT_ARB( GLuint texture_, GLenum target_, GLenum pname_, const GLuint *params_ ) {
    (void)target_;
    RemapGLName( glTextureParameterIuiv )( texture_, pname_, params_ );
}
void APIENTRY glGenerateTextureMipmapEXT_ARB( GLuint texture_, GLenum target_ ) {
    (void)target_;
    RemapGLName( glGenerateTextureMipmap )( texture_ );
}
void APIENTRY glBindMultiTextureEXT_ARB( GLenum texunit_, GLenum target_, GLuint texture_ ) {
    (void)target_;
    RemapGLName( glBindTextureUnit )( texunit_, texture_ );
}
void APIENTRY glGetTextureLevelParameterfvEXT_ARB( GLuint texture_, GLenum target_, GLint level_, GLenum pname_, GLfloat *params_ ) {
    (void)target_;
    RemapGLName( glGetTextureLevelParameterfv )( texture_, level_, pname_, params_ );
}
void APIENTRY glGetTextureLevelParameterivEXT_ARB( GLuint texture_, GLenum target_, GLint level_, GLenum pname_, GLint *params_ ) {
    (void)target_;
    RemapGLName( glGetTextureLevelParameteriv )( texture_, level_, pname_, params_ );
}
void APIENTRY glGetTextureParameterfvEXT_ARB( GLuint texture_, GLenum target_, GLenum pname_, GLfloat *params_ ) {
    (void)target_;
    RemapGLName( glGetTextureParameterfv )( texture_, pname_, params_ );
}
void APIENTRY glGetTextureParameterivEXT_ARB( GLuint texture_, GLenum target_, GLenum pname_, GLint *params_ ) {
    (void)target_;
    RemapGLName( glGetTextureParameteriv )( texture_, pname_, params_ );
}
void APIENTRY glGetTextureParameterIivEXT_ARB( GLuint texture_, GLenum target_, GLenum pname_, GLint *params_ ) {
    (void)target_;
    RemapGLName( glGetTextureParameterIiv )( texture_, pname_, params_ );
}
void APIENTRY glGetTextureParameterIuivEXT_ARB( GLuint texture_, GLenum target_, GLenum pname_, GLuint *params_ ) {
    (void)target_;
    RemapGLName( glGetTextureParameterIuiv )( texture_, pname_, params_ );
}
