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

#ifndef __GL_STATE_DSA_H__
#define __GL_STATE_DSA_H__

#include <iterator>
#include <algorithm>
#include <type_traits>
#include <cassert>
#include <vector>

#include <climits>
#include "GL/glew.h"
#include <SDL.h>

#if defined(_MSC_VER)
#define APIENTRY __stdcall
#else
#define APIENTRY
#endif

/**
 * With this macro public open gl entry point names are mapped to
 * your function pointer names.
 * As an example, usually on my projects, i put a 't' in front of
 * each function pointer name.
 * Default is name_
 */
#define RemapGLName(name_)                  name_

/**
 * This defines the function that should be called to get a
 * function pointer from open gl.
 * Default is SDL_GL_GetProcAddress(name_)
 */
#define oglGetProcAddress(name_)               SDL_GL_GetProcAddress(name_)

#if 1
/**
 * This handles the loading of the entry points and checking
 * them for NULL.
 * This variant uses decltype to cast the return value of 
 * GetProcAddress to match the pointer type.
 */
#define CheckedFunctionLoad(name_)          ( ( RemapGLName(name_) = reinterpret_cast<decltype( RemapGLName(name_) )>( oglGetProcAddress(#name_) ) ) != nullptr )
#else
/**
 * This handles the loading of the entry points and checking
 * them for NULL.
 * This variant casts the target pointer to void* to avoid
 * type-checking. Only use this if your compile can't handle
 * decltype.
 */
#define CheckedFunctionLoad(name_)          ( *reinterpret_cast<void**>( &RemapGLName(name_) ) = oglGetProcAddress(#name_) ) != NULL )
#endif

/**
 * Uncomment this on x64 to disable some warnings.
 */
//#define GL_DSA_SUPPRESS_64BIT_WARNINGS

/** 
 * Change this to match the target system
 */
#define OGL_DSA_DEFINE_THREAD_LOCAL_VARIABLE(type_, name_) static thread_local type_ name_

typedef enum dsa_level_e {
    /**
    * Direct state access is emulated
    */
    emulated,
    /**
    * Only the EXT variant is used
    */
    ext_only,
    /**
    * Only the ARB variant is used
    */
    arb_only,
    /**
    * Both ARB and EXT variants are used
    */
    full_support,
} dsa_level;


/** 
 * Initializes the direct state system
 * @param inject_always_ If true, the binding point injectors are always injected, even
 *                       if the emulation layer doesn't need it. If false, the injectors
 *                       are only injected if needed.
 * @param allow_arb_dsa_ If true, then the ARB_direct_state_access extension will be used if available.
 *                       If false, then the ARB_direct_state_access extensions will not be used.
 * @param allow_ext_dsa_ If true, then the EXT_direct_state_access extension will be used if available.
 *                       If false, then the EXT_direct_state_access extensions will not be used.
 * @return Returns the initialized direct state mode.
*/
extern "C" int GL_Init_DSA_Emulation( bool inject_always_, bool allow_arb_dsa_, bool allow_ext_dsa_ );

/* transform feedback */
extern PFNGLCREATETRANSFORMFEEDBACKSPROC RemapGLName( glCreateTransformFeedbacks );
extern PFNGLTRANSFORMFEEDBACKBUFFERBASEPROC RemapGLName( glTransformFeedbackBufferBase );
extern PFNGLTRANSFORMFEEDBACKBUFFERRANGEPROC RemapGLName( glTransformFeedbackBufferRange );
extern PFNGLGETTRANSFORMFEEDBACKIVPROC RemapGLName( glGetTransformFeedbackiv );
extern PFNGLGETTRANSFORMFEEDBACKI_VPROC RemapGLName( glGetTransformFeedbacki_v );
extern PFNGLGETTRANSFORMFEEDBACKI64_VPROC RemapGLName( glGetTransformFeedbacki64_v );
/* vertex buffer object */
extern PFNGLCREATEBUFFERSPROC RemapGLName( glCreateBuffers );
extern PFNGLNAMEDBUFFERSTORAGEPROC RemapGLName( glNamedBufferStorage );
extern PFNGLNAMEDBUFFERDATAPROC RemapGLName( glNamedBufferData );
extern PFNGLNAMEDBUFFERSUBDATAPROC RemapGLName( glNamedBufferSubData );
extern PFNGLCOPYNAMEDBUFFERSUBDATAPROC RemapGLName( glCopyNamedBufferSubData );
extern PFNGLCLEARNAMEDBUFFERDATAPROC RemapGLName( glClearNamedBufferData );
extern PFNGLCLEARNAMEDBUFFERSUBDATAPROC RemapGLName( glClearNamedBufferSubData );
extern PFNGLMAPNAMEDBUFFERPROC RemapGLName( glMapNamedBuffer );
extern PFNGLMAPNAMEDBUFFERRANGEPROC RemapGLName( glMapNamedBufferRange );
extern PFNGLUNMAPNAMEDBUFFERPROC RemapGLName( glUnmapNamedBuffer );
extern PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC RemapGLName( glFlushMappedNamedBufferRange );
extern PFNGLGETNAMEDBUFFERPARAMETERIVPROC RemapGLName( glGetNamedBufferParameteriv );
extern PFNGLGETNAMEDBUFFERPARAMETERI64VPROC RemapGLName( glGetNamedBufferParameteri64v );
extern PFNGLGETNAMEDBUFFERPOINTERVPROC RemapGLName( glGetNamedBufferPointerv );
extern PFNGLGETNAMEDBUFFERSUBDATAPROC RemapGLName( glGetNamedBufferSubData );
/* framebuffer object */
extern PFNGLCREATEFRAMEBUFFERSPROC RemapGLName( glCreateFramebuffers );
extern PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC RemapGLName( glNamedFramebufferRenderbuffer );
extern PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC RemapGLName( glNamedFramebufferParameteri );
extern PFNGLNAMEDFRAMEBUFFERTEXTUREPROC RemapGLName( glNamedFramebufferTexture );
extern PFNGLNAMEDFRAMEBUFFERTEXTURELAYERPROC RemapGLName( glNamedFramebufferTextureLayer );
extern PFNGLNAMEDFRAMEBUFFERDRAWBUFFERPROC RemapGLName( glNamedFramebufferDrawBuffer );
extern PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC RemapGLName( glNamedFramebufferDrawBuffers );
extern PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC RemapGLName( glNamedFramebufferReadBuffer );
extern PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC RemapGLName( glInvalidateNamedFramebufferData );
extern PFNGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC RemapGLName( glInvalidateNamedFramebufferSubData );
extern PFNGLCLEARNAMEDFRAMEBUFFERIVPROC RemapGLName( glClearNamedFramebufferiv );
extern PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC RemapGLName( glClearNamedFramebufferuiv );
extern PFNGLCLEARNAMEDFRAMEBUFFERFVPROC RemapGLName( glClearNamedFramebufferfv );
extern PFNGLCLEARNAMEDFRAMEBUFFERFIPROC RemapGLName( glClearNamedFramebufferfi );
extern PFNGLBLITNAMEDFRAMEBUFFERPROC RemapGLName( glBlitNamedFramebuffer );
extern PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC RemapGLName( glCheckNamedFramebufferStatus );
extern PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC RemapGLName( glGetNamedFramebufferParameteriv );
extern PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC RemapGLName( glGetNamedFramebufferAttachmentParameteriv );
/* Renderbuffer object functions */
extern PFNGLCREATERENDERBUFFERSPROC RemapGLName( glCreateRenderbuffers );
extern PFNGLNAMEDRENDERBUFFERSTORAGEPROC RemapGLName( glNamedRenderbufferStorage );
extern PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC RemapGLName( glNamedRenderbufferStorageMultisample );
extern PFNGLGETNAMEDRENDERBUFFERPARAMETERIVPROC RemapGLName( glGetNamedRenderbufferParameteriv );
/* Sampler object functions */
extern PFNGLCREATESAMPLERSPROC RemapGLName( glCreateSamplers );
/* Program Pipeline object functions */
extern PFNGLCREATEPROGRAMPIPELINESPROC RemapGLName( glCreateProgramPipelines );
/* Query object functions */
extern PFNGLCREATEQUERIESPROC RemapGLName( glCreateQueries );
/* Vertex Array object functions */
extern PFNGLCREATEVERTEXARRAYSPROC RemapGLName( glCreateVertexArrays );
extern PFNGLDISABLEVERTEXARRAYATTRIBPROC RemapGLName( glDisableVertexArrayAttrib );
extern PFNGLENABLEVERTEXARRAYATTRIBPROC RemapGLName( glEnableVertexArrayAttrib );
extern PFNGLVERTEXARRAYELEMENTBUFFERPROC RemapGLName( glVertexArrayElementBuffer );
extern PFNGLVERTEXARRAYVERTEXBUFFERPROC RemapGLName( glVertexArrayVertexBuffer );
extern PFNGLVERTEXARRAYVERTEXBUFFERSPROC RemapGLName( glVertexArrayVertexBuffers );
extern PFNGLVERTEXARRAYATTRIBFORMATPROC RemapGLName( glVertexArrayAttribFormat );
extern PFNGLVERTEXARRAYATTRIBIFORMATPROC RemapGLName( glVertexArrayAttribIFormat );
extern PFNGLVERTEXARRAYATTRIBLFORMATPROC RemapGLName( glVertexArrayAttribLFormat );
extern PFNGLVERTEXARRAYATTRIBBINDINGPROC RemapGLName( glVertexArrayAttribBinding );
extern PFNGLVERTEXARRAYBINDINGDIVISORPROC RemapGLName( glVertexArrayBindingDivisor );
extern PFNGLGETVERTEXARRAYIVPROC RemapGLName( glGetVertexArrayiv );
extern PFNGLGETVERTEXARRAYINDEXEDIVPROC RemapGLName( glGetVertexArrayIndexediv );
extern PFNGLGETVERTEXARRAYINDEXED64IVPROC RemapGLName( glGetVertexArrayIndexed64iv );
/* Texture object functions */
extern PFNGLCREATETEXTURESPROC RemapGLName( glCreateTextures );
extern PFNGLTEXTUREBUFFEREXTPROC RemapGLName( glTextureBufferEXT );
extern PFNGLTEXTUREBUFFERRANGEEXTPROC RemapGLName( glTextureBufferRangeEXT );
extern PFNGLTEXTURESTORAGE1DEXTPROC RemapGLName( glTextureStorage1DEXT );
extern PFNGLTEXTURESTORAGE2DEXTPROC RemapGLName( glTextureStorage2DEXT );
extern PFNGLTEXTURESTORAGE3DEXTPROC RemapGLName( glTextureStorage3DEXT );
extern PFNGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC RemapGLName( glTextureStorage2DMultisampleEXT );
extern PFNGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC RemapGLName( glTextureStorage3DMultisampleEXT );
extern PFNGLTEXTURESUBIMAGE1DEXTPROC RemapGLName( glTextureSubImage1DEXT );
extern PFNGLTEXTURESUBIMAGE2DEXTPROC RemapGLName( glTextureSubImage2DEXT );
extern PFNGLTEXTURESUBIMAGE3DEXTPROC RemapGLName( glTextureSubImage3DEXT );
extern PFNGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC RemapGLName( glCompressedTextureSubImage1DEXT );
extern PFNGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC RemapGLName( glCompressedTextureSubImage2DEXT );
extern PFNGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC RemapGLName( glCompressedTextureSubImage3DEXT );
extern PFNGLCOPYTEXTURESUBIMAGE1DEXTPROC RemapGLName( glCopyTextureSubImage1DEXT );
extern PFNGLCOPYTEXTURESUBIMAGE2DEXTPROC RemapGLName( glCopyTextureSubImage2DEXT );
extern PFNGLCOPYTEXTURESUBIMAGE3DEXTPROC RemapGLName( glCopyTextureSubImage3DEXT );
extern PFNGLTEXTUREPARAMETERFEXTPROC RemapGLName( glTextureParameterfEXT );
extern PFNGLTEXTUREPARAMETERFVEXTPROC RemapGLName( glTextureParameterfvEXT );
extern PFNGLTEXTUREPARAMETERIEXTPROC RemapGLName( glTextureParameteriEXT );
extern PFNGLTEXTUREPARAMETERIVEXTPROC RemapGLName( glTextureParameterivEXT );
extern PFNGLTEXTUREPARAMETERIIVEXTPROC RemapGLName( glTextureParameterIivEXT );
extern PFNGLTEXTUREPARAMETERIUIVEXTPROC RemapGLName( glTextureParameterIuivEXT );
extern PFNGLGENERATETEXTUREMIPMAPEXTPROC RemapGLName( glGenerateTextureMipmapEXT );
extern PFNGLBINDMULTITEXTUREEXTPROC RemapGLName( glBindMultiTextureEXT );
extern PFNGLGETTEXTUREIMAGEEXTPROC RemapGLName( glGetTextureImageEXT );
extern PFNGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC RemapGLName( glGetCompressedTextureImageEXT );
extern PFNGLGETTEXTURELEVELPARAMETERFVEXTPROC RemapGLName( glGetTextureLevelParameterfvEXT );
extern PFNGLGETTEXTURELEVELPARAMETERIVEXTPROC RemapGLName( glGetTextureLevelParameterivEXT );
extern PFNGLGETTEXTUREPARAMETERFVEXTPROC RemapGLName( glGetTextureParameterfvEXT );
extern PFNGLGETTEXTUREPARAMETERIVEXTPROC RemapGLName( glGetTextureParameterivEXT );
extern PFNGLGETTEXTUREPARAMETERIIVEXTPROC RemapGLName( glGetTextureParameterIivEXT );
extern PFNGLGETTEXTUREPARAMETERIUIVEXTPROC RemapGLName( glGetTextureParameterIuivEXT );

#endif