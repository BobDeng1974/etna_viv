/**************************************************************************
 * 
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * Copyright 2009-2010 VMware, Inc.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/
#ifndef H_MINIGALLIUM
#define H_MINIGALLIUM
/********************************************************************
 * State and tokens from gallium, for experimentation
 *******************************************************************/

union pipe_color_union
{
   float f[4];
   int i[4];
   unsigned int ui[4];
};

#define PIPE_BLENDFACTOR_ONE                 0x1
#define PIPE_BLENDFACTOR_SRC_COLOR           0x2
#define PIPE_BLENDFACTOR_SRC_ALPHA           0x3
#define PIPE_BLENDFACTOR_DST_ALPHA           0x4
#define PIPE_BLENDFACTOR_DST_COLOR           0x5
#define PIPE_BLENDFACTOR_SRC_ALPHA_SATURATE  0x6
#define PIPE_BLENDFACTOR_CONST_COLOR         0x7
#define PIPE_BLENDFACTOR_CONST_ALPHA         0x8
#define PIPE_BLENDFACTOR_SRC1_COLOR          0x9
#define PIPE_BLENDFACTOR_SRC1_ALPHA          0x0A
#define PIPE_BLENDFACTOR_ZERO                0x11
#define PIPE_BLENDFACTOR_INV_SRC_COLOR       0x12
#define PIPE_BLENDFACTOR_INV_SRC_ALPHA       0x13
#define PIPE_BLENDFACTOR_INV_DST_ALPHA       0x14
#define PIPE_BLENDFACTOR_INV_DST_COLOR       0x15
#define PIPE_BLENDFACTOR_INV_CONST_COLOR     0x17
#define PIPE_BLENDFACTOR_INV_CONST_ALPHA     0x18
#define PIPE_BLENDFACTOR_INV_SRC1_COLOR      0x19
#define PIPE_BLENDFACTOR_INV_SRC1_ALPHA      0x1A

#define PIPE_BLEND_ADD               0
#define PIPE_BLEND_SUBTRACT          1
#define PIPE_BLEND_REVERSE_SUBTRACT  2
#define PIPE_BLEND_MIN               3
#define PIPE_BLEND_MAX               4
/**
 * Primitive (point/line/tri) rasterization info
 */
#define PIPE_MAX_ATTRIBS          32
#define PIPE_MAX_CLIP_PLANES       8
#define PIPE_MAX_COLOR_BUFS        8
#define PIPE_MAX_CONSTANT_BUFFERS 32
#define PIPE_MAX_SAMPLERS         16
#define PIPE_MAX_SHADER_INPUTS    32
#define PIPE_MAX_SHADER_OUTPUTS   48 /* 32 GENERICs + POS, PSIZE, FOG, etc. */
#define PIPE_MAX_SHADER_SAMPLER_VIEWS 32
#define PIPE_MAX_SHADER_RESOURCES 32
#define PIPE_MAX_TEXTURE_LEVELS   16
#define PIPE_MAX_SO_BUFFERS        4

/** Polygon fill mode */
#define PIPE_FACE_NONE           0
#define PIPE_FACE_FRONT          1
#define PIPE_FACE_BACK           2
#define PIPE_FACE_FRONT_AND_BACK (PIPE_FACE_FRONT | PIPE_FACE_BACK)

/** Polygon fill mode */
#define PIPE_POLYGON_MODE_FILL  0
#define PIPE_POLYGON_MODE_LINE  1
#define PIPE_POLYGON_MODE_POINT 2

/**
 * Inequality functions.  Used for depth test, stencil compare, alpha
 * test, shadow compare, etc.
 */
#define PIPE_FUNC_NEVER    0
#define PIPE_FUNC_LESS     1
#define PIPE_FUNC_EQUAL    2
#define PIPE_FUNC_LEQUAL   3
#define PIPE_FUNC_GREATER  4
#define PIPE_FUNC_NOTEQUAL 5
#define PIPE_FUNC_GEQUAL   6
#define PIPE_FUNC_ALWAYS   7

/** Stencil ops */
#define PIPE_STENCIL_OP_KEEP       0
#define PIPE_STENCIL_OP_ZERO       1
#define PIPE_STENCIL_OP_REPLACE    2
#define PIPE_STENCIL_OP_INCR       3
#define PIPE_STENCIL_OP_DECR       4
#define PIPE_STENCIL_OP_INCR_WRAP  5
#define PIPE_STENCIL_OP_DECR_WRAP  6
#define PIPE_STENCIL_OP_INVERT     7

/** Texture types.
 * See the documentation for info on PIPE_TEXTURE_RECT vs PIPE_TEXTURE_2D */
enum pipe_texture_target {
   PIPE_BUFFER           = 0,
   PIPE_TEXTURE_1D       = 1,
   PIPE_TEXTURE_2D       = 2,
   PIPE_TEXTURE_3D       = 3,
   PIPE_TEXTURE_CUBE     = 4,
   PIPE_TEXTURE_RECT     = 5,
   PIPE_TEXTURE_1D_ARRAY = 6,
   PIPE_TEXTURE_2D_ARRAY = 7,
   PIPE_TEXTURE_CUBE_ARRAY = 8,
   PIPE_MAX_TEXTURE_TYPES
};

#define PIPE_TEX_WRAP_REPEAT                   0
#define PIPE_TEX_WRAP_CLAMP                    1
#define PIPE_TEX_WRAP_CLAMP_TO_EDGE            2
#define PIPE_TEX_WRAP_CLAMP_TO_BORDER          3
#define PIPE_TEX_WRAP_MIRROR_REPEAT            4
#define PIPE_TEX_WRAP_MIRROR_CLAMP             5
#define PIPE_TEX_WRAP_MIRROR_CLAMP_TO_EDGE     6
#define PIPE_TEX_WRAP_MIRROR_CLAMP_TO_BORDER   7

/* Between mipmaps, ie mipfilter
 */
#define PIPE_TEX_MIPFILTER_NEAREST  0
#define PIPE_TEX_MIPFILTER_LINEAR   1
#define PIPE_TEX_MIPFILTER_NONE     2

/* Within a mipmap, ie min/mag filter 
 */
#define PIPE_TEX_FILTER_NEAREST      0
#define PIPE_TEX_FILTER_LINEAR       1

#define PIPE_TEX_COMPARE_NONE          0
#define PIPE_TEX_COMPARE_R_TO_TEXTURE  1

enum pipe_type {
   PIPE_TYPE_UNORM = 0,
   PIPE_TYPE_SNORM,
   PIPE_TYPE_SINT,
   PIPE_TYPE_UINT,
   PIPE_TYPE_FLOAT,
   PIPE_TYPE_COUNT
};

/**
 * Texture/surface image formats (preliminary)
 */

/* KW: Added lots of surface formats to support vertex element layout
 * definitions, and eventually render-to-vertex-buffer.
 */

enum pipe_format {
   PIPE_FORMAT_NONE                    = 0,
   PIPE_FORMAT_B8G8R8A8_UNORM          = 1,
   PIPE_FORMAT_B8G8R8X8_UNORM          = 2,
   PIPE_FORMAT_A8R8G8B8_UNORM          = 3,
   PIPE_FORMAT_X8R8G8B8_UNORM          = 4,
   PIPE_FORMAT_B5G5R5A1_UNORM          = 5,
   PIPE_FORMAT_B4G4R4A4_UNORM          = 6,
   PIPE_FORMAT_B5G6R5_UNORM            = 7,
   PIPE_FORMAT_R10G10B10A2_UNORM       = 8,
   PIPE_FORMAT_L8_UNORM                = 9,    /**< ubyte luminance */
   PIPE_FORMAT_A8_UNORM                = 10,   /**< ubyte alpha */
   PIPE_FORMAT_I8_UNORM                = 11,   /**< ubyte intensity */
   PIPE_FORMAT_L8A8_UNORM              = 12,   /**< ubyte alpha, luminance */
   PIPE_FORMAT_L16_UNORM               = 13,   /**< ushort luminance */
   PIPE_FORMAT_UYVY                    = 14,
   PIPE_FORMAT_YUYV                    = 15,
   PIPE_FORMAT_Z16_UNORM               = 16,
   PIPE_FORMAT_Z32_UNORM               = 17,
   PIPE_FORMAT_Z32_FLOAT               = 18,
   PIPE_FORMAT_Z24_UNORM_S8_UINT       = 19,
   PIPE_FORMAT_S8_UINT_Z24_UNORM       = 20,
   PIPE_FORMAT_Z24X8_UNORM             = 21,
   PIPE_FORMAT_X8Z24_UNORM             = 22,
   PIPE_FORMAT_S8_UINT                 = 23,   /**< ubyte stencil */
   PIPE_FORMAT_R64_FLOAT               = 24,
   PIPE_FORMAT_R64G64_FLOAT            = 25,
   PIPE_FORMAT_R64G64B64_FLOAT         = 26,
   PIPE_FORMAT_R64G64B64A64_FLOAT      = 27,
   PIPE_FORMAT_R32_FLOAT               = 28,
   PIPE_FORMAT_R32G32_FLOAT            = 29,
   PIPE_FORMAT_R32G32B32_FLOAT         = 30,
   PIPE_FORMAT_R32G32B32A32_FLOAT      = 31,
   PIPE_FORMAT_R32_UNORM               = 32,
   PIPE_FORMAT_R32G32_UNORM            = 33,
   PIPE_FORMAT_R32G32B32_UNORM         = 34,
   PIPE_FORMAT_R32G32B32A32_UNORM      = 35,
   PIPE_FORMAT_R32_USCALED             = 36,
   PIPE_FORMAT_R32G32_USCALED          = 37,
   PIPE_FORMAT_R32G32B32_USCALED       = 38,
   PIPE_FORMAT_R32G32B32A32_USCALED    = 39,
   PIPE_FORMAT_R32_SNORM               = 40,
   PIPE_FORMAT_R32G32_SNORM            = 41,
   PIPE_FORMAT_R32G32B32_SNORM         = 42,
   PIPE_FORMAT_R32G32B32A32_SNORM      = 43,
   PIPE_FORMAT_R32_SSCALED             = 44,
   PIPE_FORMAT_R32G32_SSCALED          = 45,
   PIPE_FORMAT_R32G32B32_SSCALED       = 46,
   PIPE_FORMAT_R32G32B32A32_SSCALED    = 47,
   PIPE_FORMAT_R16_UNORM               = 48,
   PIPE_FORMAT_R16G16_UNORM            = 49,
   PIPE_FORMAT_R16G16B16_UNORM         = 50,
   PIPE_FORMAT_R16G16B16A16_UNORM      = 51,
   PIPE_FORMAT_R16_USCALED             = 52,
   PIPE_FORMAT_R16G16_USCALED          = 53,
   PIPE_FORMAT_R16G16B16_USCALED       = 54,
   PIPE_FORMAT_R16G16B16A16_USCALED    = 55,
   PIPE_FORMAT_R16_SNORM               = 56,
   PIPE_FORMAT_R16G16_SNORM            = 57,
   PIPE_FORMAT_R16G16B16_SNORM         = 58,
   PIPE_FORMAT_R16G16B16A16_SNORM      = 59,
   PIPE_FORMAT_R16_SSCALED             = 60,
   PIPE_FORMAT_R16G16_SSCALED          = 61,
   PIPE_FORMAT_R16G16B16_SSCALED       = 62,
   PIPE_FORMAT_R16G16B16A16_SSCALED    = 63,
   PIPE_FORMAT_R8_UNORM                = 64,
   PIPE_FORMAT_R8G8_UNORM              = 65,
   PIPE_FORMAT_R8G8B8_UNORM            = 66,
   PIPE_FORMAT_R8G8B8A8_UNORM          = 67,
   PIPE_FORMAT_X8B8G8R8_UNORM          = 68,
   PIPE_FORMAT_R8_USCALED              = 69,
   PIPE_FORMAT_R8G8_USCALED            = 70,
   PIPE_FORMAT_R8G8B8_USCALED          = 71,
   PIPE_FORMAT_R8G8B8A8_USCALED        = 72,
   PIPE_FORMAT_R8_SNORM                = 74,
   PIPE_FORMAT_R8G8_SNORM              = 75,
   PIPE_FORMAT_R8G8B8_SNORM            = 76,
   PIPE_FORMAT_R8G8B8A8_SNORM          = 77,
   PIPE_FORMAT_R8_SSCALED              = 82,
   PIPE_FORMAT_R8G8_SSCALED            = 83,
   PIPE_FORMAT_R8G8B8_SSCALED          = 84,
   PIPE_FORMAT_R8G8B8A8_SSCALED        = 85,
   PIPE_FORMAT_R32_FIXED               = 87,
   PIPE_FORMAT_R32G32_FIXED            = 88,
   PIPE_FORMAT_R32G32B32_FIXED         = 89,
   PIPE_FORMAT_R32G32B32A32_FIXED      = 90,
   PIPE_FORMAT_R16_FLOAT               = 91,
   PIPE_FORMAT_R16G16_FLOAT            = 92,
   PIPE_FORMAT_R16G16B16_FLOAT         = 93,
   PIPE_FORMAT_R16G16B16A16_FLOAT      = 94,

   /* sRGB formats */
   PIPE_FORMAT_L8_SRGB                 = 95,
   PIPE_FORMAT_L8A8_SRGB               = 96,
   PIPE_FORMAT_R8G8B8_SRGB             = 97,
   PIPE_FORMAT_A8B8G8R8_SRGB           = 98,
   PIPE_FORMAT_X8B8G8R8_SRGB           = 99,
   PIPE_FORMAT_B8G8R8A8_SRGB           = 100,
   PIPE_FORMAT_B8G8R8X8_SRGB           = 101,
   PIPE_FORMAT_A8R8G8B8_SRGB           = 102,
   PIPE_FORMAT_X8R8G8B8_SRGB           = 103,
   PIPE_FORMAT_R8G8B8A8_SRGB           = 104,

   /* compressed formats */
   PIPE_FORMAT_DXT1_RGB                = 105,
   PIPE_FORMAT_DXT1_RGBA               = 106,
   PIPE_FORMAT_DXT3_RGBA               = 107,
   PIPE_FORMAT_DXT5_RGBA               = 108,

   /* sRGB, compressed */
   PIPE_FORMAT_DXT1_SRGB               = 109,
   PIPE_FORMAT_DXT1_SRGBA              = 110,
   PIPE_FORMAT_DXT3_SRGBA              = 111,
   PIPE_FORMAT_DXT5_SRGBA              = 112,

   /* rgtc compressed */
   PIPE_FORMAT_RGTC1_UNORM             = 113,
   PIPE_FORMAT_RGTC1_SNORM             = 114,
   PIPE_FORMAT_RGTC2_UNORM             = 115,
   PIPE_FORMAT_RGTC2_SNORM             = 116,

   PIPE_FORMAT_R8G8_B8G8_UNORM         = 117,
   PIPE_FORMAT_G8R8_G8B8_UNORM         = 118,

   /* mixed formats */
   PIPE_FORMAT_R8SG8SB8UX8U_NORM       = 119,
   PIPE_FORMAT_R5SG5SB6U_NORM          = 120,

   /* TODO: re-order these */
   PIPE_FORMAT_A8B8G8R8_UNORM          = 121,
   PIPE_FORMAT_B5G5R5X1_UNORM          = 122,
   PIPE_FORMAT_R10G10B10A2_USCALED     = 123,
   PIPE_FORMAT_R11G11B10_FLOAT         = 124,
   PIPE_FORMAT_R9G9B9E5_FLOAT          = 125,
   PIPE_FORMAT_Z32_FLOAT_S8X24_UINT    = 126,
   PIPE_FORMAT_R1_UNORM                = 127,
   PIPE_FORMAT_R10G10B10X2_USCALED     = 128,
   PIPE_FORMAT_R10G10B10X2_SNORM       = 129,
   PIPE_FORMAT_L4A4_UNORM              = 130,
   PIPE_FORMAT_B10G10R10A2_UNORM       = 131,
   PIPE_FORMAT_R10SG10SB10SA2U_NORM    = 132,
   PIPE_FORMAT_R8G8Bx_SNORM            = 133,
   PIPE_FORMAT_R8G8B8X8_UNORM          = 134,
   PIPE_FORMAT_B4G4R4X4_UNORM          = 135,

   /* some stencil samplers formats */
   PIPE_FORMAT_X24S8_UINT              = 136,
   PIPE_FORMAT_S8X24_UINT              = 137,
   PIPE_FORMAT_X32_S8X24_UINT          = 138,

   PIPE_FORMAT_B2G3R3_UNORM            = 139,
   PIPE_FORMAT_L16A16_UNORM            = 140,
   PIPE_FORMAT_A16_UNORM               = 141,
   PIPE_FORMAT_I16_UNORM               = 142,

   PIPE_FORMAT_LATC1_UNORM             = 143,
   PIPE_FORMAT_LATC1_SNORM             = 144,
   PIPE_FORMAT_LATC2_UNORM             = 145,
   PIPE_FORMAT_LATC2_SNORM             = 146,

   PIPE_FORMAT_A8_SNORM                = 147,
   PIPE_FORMAT_L8_SNORM                = 148,
   PIPE_FORMAT_L8A8_SNORM              = 149,
   PIPE_FORMAT_I8_SNORM                = 150,
   PIPE_FORMAT_A16_SNORM               = 151,
   PIPE_FORMAT_L16_SNORM               = 152,
   PIPE_FORMAT_L16A16_SNORM            = 153,
   PIPE_FORMAT_I16_SNORM               = 154,

   PIPE_FORMAT_A16_FLOAT               = 155,
   PIPE_FORMAT_L16_FLOAT               = 156,
   PIPE_FORMAT_L16A16_FLOAT            = 157,
   PIPE_FORMAT_I16_FLOAT               = 158,
   PIPE_FORMAT_A32_FLOAT               = 159,
   PIPE_FORMAT_L32_FLOAT               = 160,
   PIPE_FORMAT_L32A32_FLOAT            = 161,
   PIPE_FORMAT_I32_FLOAT               = 162,

   PIPE_FORMAT_YV12                    = 163,
   PIPE_FORMAT_YV16                    = 164,
   PIPE_FORMAT_IYUV                    = 165,  /**< aka I420 */
   PIPE_FORMAT_NV12                    = 166,
   PIPE_FORMAT_NV21                    = 167,

   PIPE_FORMAT_R4A4_UNORM              = 168,
   PIPE_FORMAT_A4R4_UNORM              = 169,
   PIPE_FORMAT_R8A8_UNORM              = 170,
   PIPE_FORMAT_A8R8_UNORM              = 171,

   PIPE_FORMAT_R10G10B10A2_SSCALED     = 172,
   PIPE_FORMAT_R10G10B10A2_SNORM       = 173,

   PIPE_FORMAT_B10G10R10A2_USCALED     = 174,
   PIPE_FORMAT_B10G10R10A2_SSCALED     = 175,
   PIPE_FORMAT_B10G10R10A2_SNORM       = 176,

   PIPE_FORMAT_R8_UINT                 = 177,
   PIPE_FORMAT_R8G8_UINT               = 178,
   PIPE_FORMAT_R8G8B8_UINT             = 179,
   PIPE_FORMAT_R8G8B8A8_UINT           = 180,

   PIPE_FORMAT_R8_SINT                 = 181,
   PIPE_FORMAT_R8G8_SINT               = 182,
   PIPE_FORMAT_R8G8B8_SINT             = 183,
   PIPE_FORMAT_R8G8B8A8_SINT           = 184,

   PIPE_FORMAT_R16_UINT                = 185,
   PIPE_FORMAT_R16G16_UINT             = 186,
   PIPE_FORMAT_R16G16B16_UINT          = 187,
   PIPE_FORMAT_R16G16B16A16_UINT       = 188,

   PIPE_FORMAT_R16_SINT                = 189,
   PIPE_FORMAT_R16G16_SINT             = 190,
   PIPE_FORMAT_R16G16B16_SINT          = 191,
   PIPE_FORMAT_R16G16B16A16_SINT       = 192,

   PIPE_FORMAT_R32_UINT                = 193,
   PIPE_FORMAT_R32G32_UINT             = 194,
   PIPE_FORMAT_R32G32B32_UINT          = 195,
   PIPE_FORMAT_R32G32B32A32_UINT       = 196,

   PIPE_FORMAT_R32_SINT                = 197,
   PIPE_FORMAT_R32G32_SINT             = 198,
   PIPE_FORMAT_R32G32B32_SINT          = 199,
   PIPE_FORMAT_R32G32B32A32_SINT       = 200,

   PIPE_FORMAT_A8_UINT                 = 201,
   PIPE_FORMAT_I8_UINT                 = 202,
   PIPE_FORMAT_L8_UINT                 = 203,
   PIPE_FORMAT_L8A8_UINT               = 204,

   PIPE_FORMAT_A8_SINT                 = 205,
   PIPE_FORMAT_I8_SINT                 = 206,
   PIPE_FORMAT_L8_SINT                 = 207,
   PIPE_FORMAT_L8A8_SINT               = 208,

   PIPE_FORMAT_A16_UINT                = 209,
   PIPE_FORMAT_I16_UINT                = 210,
   PIPE_FORMAT_L16_UINT                = 211,
   PIPE_FORMAT_L16A16_UINT             = 212,

   PIPE_FORMAT_A16_SINT                = 213,
   PIPE_FORMAT_I16_SINT                = 214,
   PIPE_FORMAT_L16_SINT                = 215,
   PIPE_FORMAT_L16A16_SINT             = 216,

   PIPE_FORMAT_A32_UINT                = 217,
   PIPE_FORMAT_I32_UINT                = 218,
   PIPE_FORMAT_L32_UINT                = 219,
   PIPE_FORMAT_L32A32_UINT             = 220,

   PIPE_FORMAT_A32_SINT                = 221,
   PIPE_FORMAT_I32_SINT                = 222,
   PIPE_FORMAT_L32_SINT                = 223,
   PIPE_FORMAT_L32A32_SINT             = 224,

   PIPE_FORMAT_B10G10R10A2_UINT        = 225, 

   PIPE_FORMAT_ETC1_RGB8               = 226,

   PIPE_FORMAT_R8G8_R8B8_UNORM         = 227,
   PIPE_FORMAT_G8R8_B8R8_UNORM         = 228,

   PIPE_FORMAT_COUNT
};


struct pipe_rasterizer_state
{
   unsigned flatshade:1;
   unsigned light_twoside:1;
   unsigned clamp_vertex_color:1;
   unsigned clamp_fragment_color:1;
   unsigned front_ccw:1;
   unsigned cull_face:2;      /**< PIPE_FACE_x */
   unsigned fill_front:2;     /**< PIPE_POLYGON_MODE_x */
   unsigned fill_back:2;      /**< PIPE_POLYGON_MODE_x */
   unsigned offset_point:1;
   unsigned offset_line:1;
   unsigned offset_tri:1;
   unsigned scissor:1;
   unsigned poly_smooth:1;
   unsigned poly_stipple_enable:1;
   unsigned point_smooth:1;
   unsigned sprite_coord_mode:1;     /**< PIPE_SPRITE_COORD_ */
   unsigned point_quad_rasterization:1; /** points rasterized as quads or points */
   unsigned point_size_per_vertex:1; /**< size computed in vertex shader */
   unsigned multisample:1;         /* XXX maybe more ms state in future */
   unsigned line_smooth:1;
   unsigned line_stipple_enable:1;
   unsigned line_last_pixel:1;

   /**
    * Use the first vertex of a primitive as the provoking vertex for
    * flat shading.
    */
   unsigned flatshade_first:1;

   /**
    * When true, triangle rasterization uses (0.5, 0.5) pixel centers
    * for determining pixel ownership.
    *
    * When false, triangle rasterization uses (0,0) pixel centers for
    * determining pixel ownership.
    *
    * Triangle rasterization always uses a 'top,left' rule for pixel
    * ownership, this just alters which point we consider the pixel
    * center for that test.
    */
   unsigned gl_rasterization_rules:1;

   /**
    * When true, rasterization is disabled and no pixels are written.
    * This only makes sense with the Stream Out functionality.
    */
   unsigned rasterizer_discard:1;

   /**
    * When false, depth clipping is disabled and the depth value will be
    * clamped later at the per-pixel level before depth testing.
    * This depends on PIPE_CAP_DEPTH_CLIP_DISABLE.
    */
   unsigned depth_clip:1;

   /**
    * Enable bits for clipping half-spaces.
    * This applies to both user clip planes and shader clip distances.
    * Note that if the bound shader exports any clip distances, these
    * replace all user clip planes, and clip half-spaces enabled here
    * but not written by the shader count as disabled.
    */
   unsigned clip_plane_enable:PIPE_MAX_CLIP_PLANES;

   unsigned line_stipple_factor:8;  /**< [1..256] actually */
   unsigned line_stipple_pattern:16;

   unsigned sprite_coord_enable; /* bitfield referring to 32 GENERIC inputs */

   float line_width;
   float point_size;           /**< used when no per-vertex size */
   float offset_units;
   float offset_scale;
   float offset_clamp;
};

struct pipe_viewport_state
{
   float scale[4];
   float translate[4];
};


struct pipe_scissor_state
{
   unsigned minx:16;
   unsigned miny:16;
   unsigned maxx:16;
   unsigned maxy:16;
};
struct pipe_depth_state 
{
   unsigned enabled:1;         /**< depth test enabled? */
   unsigned writemask:1;       /**< allow depth buffer writes? */
   unsigned func:3;            /**< depth test func (PIPE_FUNC_x) */
};


struct pipe_stencil_state
{
   unsigned enabled:1;  /**< stencil[0]: stencil enabled, stencil[1]: two-side enabled */
   unsigned func:3;     /**< PIPE_FUNC_x */
   unsigned fail_op:3;  /**< PIPE_STENCIL_OP_x */
   unsigned zpass_op:3; /**< PIPE_STENCIL_OP_x */
   unsigned zfail_op:3; /**< PIPE_STENCIL_OP_x */
   unsigned valuemask:8;
   unsigned writemask:8;
};


struct pipe_alpha_state
{
   unsigned enabled:1;
   unsigned func:3;     /**< PIPE_FUNC_x */
   float ref_value;     /**< reference value */
};


struct pipe_depth_stencil_alpha_state
{
   struct pipe_depth_state depth;
   struct pipe_stencil_state stencil[2]; /**< [0] = front, [1] = back */
   struct pipe_alpha_state alpha;
};


struct pipe_rt_blend_state
{
   unsigned blend_enable:1;

   unsigned rgb_func:3;          /**< PIPE_BLEND_x */
   unsigned rgb_src_factor:5;    /**< PIPE_BLENDFACTOR_x */
   unsigned rgb_dst_factor:5;    /**< PIPE_BLENDFACTOR_x */

   unsigned alpha_func:3;        /**< PIPE_BLEND_x */
   unsigned alpha_src_factor:5;  /**< PIPE_BLENDFACTOR_x */
   unsigned alpha_dst_factor:5;  /**< PIPE_BLENDFACTOR_x */

   unsigned colormask:4;         /**< bitmask of PIPE_MASK_R/G/B/A */
};

struct pipe_blend_state
{
   unsigned independent_blend_enable:1;
   unsigned logicop_enable:1;
   unsigned logicop_func:4;      /**< PIPE_LOGICOP_x */
   unsigned dither:1;
   unsigned alpha_to_coverage:1;
   unsigned alpha_to_one:1;
   struct pipe_rt_blend_state rt[PIPE_MAX_COLOR_BUFS];
};


struct pipe_blend_color
{
   float color[4];
};

struct pipe_stencil_ref
{
   uint8_t ref_value[2];
};

struct pipe_framebuffer_state
{
   unsigned width, height;

   /** multiple color buffers for multiple render targets */
   unsigned nr_cbufs;
   struct pipe_surface *cbufs[PIPE_MAX_COLOR_BUFS];

   struct pipe_surface *zsbuf;      /**< Z/stencil buffer */
};

/**
 * Texture sampler state.
 */
struct pipe_sampler_state
{
   unsigned wrap_s:3;            /**< PIPE_TEX_WRAP_x */
   unsigned wrap_t:3;            /**< PIPE_TEX_WRAP_x */
   unsigned wrap_r:3;            /**< PIPE_TEX_WRAP_x */
   unsigned min_img_filter:2;    /**< PIPE_TEX_FILTER_x */
   unsigned min_mip_filter:2;    /**< PIPE_TEX_MIPFILTER_x */
   unsigned mag_img_filter:2;    /**< PIPE_TEX_FILTER_x */
   unsigned compare_mode:1;      /**< PIPE_TEX_COMPARE_x */
   unsigned compare_func:3;      /**< PIPE_FUNC_x */
   unsigned normalized_coords:1; /**< Are coords normalized to [0,1]? */
   unsigned max_anisotropy:6;
   unsigned seamless_cube_map:1;
   float lod_bias;               /**< LOD/lambda bias */
   float min_lod, max_lod;       /**< LOD clamp range, after bias */
   union pipe_color_union border_color;
};

/**
 * A memory object/resource such as a vertex buffer or texture.
 */
struct pipe_resource
{
   /* struct pipe_reference reference; */
   struct pipe_screen *screen; /**< screen that this texture belongs to */
   enum pipe_texture_target target; /**< PIPE_TEXTURE_x */
   enum pipe_format format;         /**< PIPE_FORMAT_x */

   unsigned width0;
   unsigned height0;
   unsigned depth0;
   unsigned array_size;

   unsigned last_level:8;    /**< Index of last mipmap level present/defined */
   unsigned nr_samples:8;    /**< for multisampled surfaces, nr of samples */
   unsigned usage:8;         /**< PIPE_USAGE_x (not a bitmask) */

   unsigned bind;            /**< bitmask of PIPE_BIND_x */
   unsigned flags;           /**< bitmask of PIPE_RESOURCE_FLAG_x */
};

/**
 * A view into a texture that can be bound to a color render target /
 * depth stencil attachment point.
 */
struct pipe_surface
{
   /* struct pipe_reference reference; */
   struct pipe_resource *texture; /**< resource into which this is a view  */
   struct pipe_context *context; /**< context this surface belongs to */
   enum pipe_format format;

   /* XXX width/height should be removed */
   unsigned width;               /**< logical width in pixels */
   unsigned height;              /**< logical height in pixels */

   unsigned writable:1;          /**< writable shader resource */

   union {
      struct {
         unsigned level;
         unsigned first_layer:16;
         unsigned last_layer:16;
      } tex;
      struct {
         unsigned first_element;
         unsigned last_element;
      } buf;
   } u;
};


/**
 * A view into a texture that can be bound to a shader stage.
 */
struct pipe_sampler_view
{
   /* struct pipe_reference reference; */
   enum pipe_format format;      /**< typed PIPE_FORMAT_x */
   struct pipe_resource *texture; /**< texture into which this is a view  */
   struct pipe_context *context; /**< context this view belongs to */
   union {
      struct {
         unsigned first_layer:16;     /**< first layer to use for array textures */
         unsigned last_layer:16;      /**< last layer to use for array textures */
         unsigned first_level:8;      /**< first mipmap level to use */
         unsigned last_level:8;       /**< last mipmap level to use */
      } tex;
      struct {
         unsigned first_element;
         unsigned last_element;
      } buf;
   } u;
   unsigned swizzle_r:3;         /**< PIPE_SWIZZLE_x for red component */
   unsigned swizzle_g:3;         /**< PIPE_SWIZZLE_x for green component */
   unsigned swizzle_b:3;         /**< PIPE_SWIZZLE_x for blue component */
   unsigned swizzle_a:3;         /**< PIPE_SWIZZLE_x for alpha component */
};

#endif

