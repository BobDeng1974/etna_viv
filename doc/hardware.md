GCxxx hardware
===============

Major optional blocks. Each of these can be present or not depending on the specific chip.

- 2D engine
- Composition engine (multi source blit)
- 3D engine

Feature bits
=================

Variants are somewhat different from NV; what features are supported is not so much determined by the model number 
(which only determines the performance), but determined by various properties that can be found in
read-only registers in the hardware:

 1) Chip features and minor feature flags
 2) Chip specs (number of instructions, pipelines, ...)
 3) Chip model (GC800, GC2000, ...)
 4) Chip revision of the form 0x1234

Generally the chip feature flags are used to distinguish functionality, as well as the specs, and not do much the model 
and revision. Unlike NV, which parametrizes everything on the model and revision, for GC this is left for bugfixes 
(but even these sometimes have their own feature bit).

For an overview of the feature bits see the enumerations in `state.xml`.

Modules
==============
(from Vivante SoCIP 2011 presentation [1])

            ------------------
            | Host Interface |
            ------------------
                    |
         ----------------------
         |  Memory controller |
         ----------------------
          |         |    |    |
          |        \/   \|    |
          |       ---- -----  |
          |       |3D| |Tex|  ----
         \/         Shader--->|  |
       ----        ----       |PE|
       |FE|------->|DE|------>|  |
       ----        ----       ----

Functional blocks, indicated by two-letter abbreviations:

- FE Graphics Pipeline Front End (also: DMA engine, Fetch Engine)
- PE Pixel Engine (can be version 1.0 / 2.0)
- SH SHader (up to 256 threads per shader)
- PA Primitive Assembly (clipping, perspective division, viewport transformation)
- SE Setup Engine (depth offset, scissor, clipping)
- RA RAsterizer (multisampling, clipping, culling, varying interpolation, generate fragments)
- TX Texture
- VG Vector Graphics (not available)
- IM ? (unknown bit in idle state, may group a few other modules)
- FP Fragment Processor (not available, probably was present in older GLES1 HW)
- MC Memory Controller
- HI Host Interface
- DE 2D drawing and scaling engine
- RS Resolve (resolves rendered image to memory, this is a copy and fill engine)
  - VR Video raster (YUV tiler)
  - TS Tile Status

These abbreviations are used in `state.xml` where appropriate.

[1] http://www.socip.org/socip/speech/pdf/2-Vivante-SoCIP%202011%20Presentation.pdf

Operations
-----------

Modules are programmed and kicked off using state updates, queued through the FE.

The GC320 technical manual [1] described quite a few operations, but only for the 2D part (DE).

Hands-on Workshop: Graphics Development on the i.MX 6 Series [2] has some tips specific to programming Vivante 3D hardware,
including OpenCL, but is very high level.

Thread walker = Rectangle walker? (seems to have to do with OpenCL)

[1] http://www.vivantecorp.com/Vivante_GC320_Technical_Reference_Manual_V1.0_A.pdf
[2] http://2012ftf.ccidnet.com/pdf/0049.pdf

Connections 
-------------
Follows the OpenGL pipeline design [3].

- FE2VS (FE-VS) fetch engine to vertex shader: attributes
- RA2SH (RA-PS) rasterizer to shader engine: varyings
- SH2PE (PS-PE) shader to pixel engine: color output

Overall:

    FE -> VS -> PA -> SE -> RA -> PS -> PE -> RS

How does PA/SE fit in this picture? Connection seems to be VS -> PA -> SE -> RA [1]

- PA assembles 3D primitives from vertices, culls based on trivial rejection and clips based on near Z-plane
- PA transforms from 3D view frustum into 2D screen space
- SE determines rasterization starting point for each primitive, and also culls based on trivial rejection
- RA performs per-tile, per-subtile, per-quad and per-pixel clipping

  [1] METHOD FOR DISTRIBUTED CLIPPING OUTSIDE OF VIEW VOLUME 
    http://www.freepatentsonline.com/y2010/0271370.html
  [2] Efficient tile-based rasterization
    http://www.google.com/patents/US8009169
  [3] OpenGL ES2 pipeline structure
    http://www.khronos.org/opengles/2_X/

Command stream
-------------------

Commands and data are sent to the GPU through the FE (Front End interface). The 
command stream of the front-end interface has a specific format described in this section.

Overall format

    OOOOOxxx xxxxxxxx xxxxxxxx xxxxxxxx  Command (O=Opcode, x=argument)
    arg0
    ..
    argN-1

Opcodes
    00001 Update state
    00010 End
    00011 NOP
    00100 Start DE ([15-8] rect count, 1 parameter 0xDEADDEED 2 parameter words describing target rect)
    00101 Draw primitives
    00110 Draw indexed primitives
    00111 Wait ([15-0] count)
    01000 Link ([15-0] number of bytes, arg address)
    01001 Stall (argument seems same format as state 0380C)
    01010 Call 
    01011 Return
    01101 Chip select

Arguments are always padded to 2 32-bit words. Number of arguments depends on the opcode, and 
sometimes on the first word of the command.

Commands (also see `cmdstream.xml`)

    00001FCC CCCCCCCC AAAAAAAA AAAAAAAA  Update state

      F    Fixed point flag
      C    Count
      A    Base address / 4

Fixed point flag: convert a 16.16 fixed point float in the command stream
   to a floating point value in the state.

Synchronization
----------------
There are various states related to synchronization, either between different modules in the GPU
and the GPU and the CPU (through the FE).

- `SEMAPHORE_TOKEN`
- `STALL_TOKEN`
- `STALL` command in command stream
(usually from PE to FE)

The following sequence of states is common:

    GLOBAL.SEMAPHORE_TOKEN := FROM=RA,TO=PE
    GLOBAL.STALL_TOKEN := FROM=RA,TO=PE

The first state load arms the semaphore, the second one stalls the FROM module until the TO module has raised its semaphore. In 
this example it stalls the rasterizer until the pixel engine has completed the commands up until now. 

The `STALL` command is used to stall the command queue until the semaphore has been received. The stall command has
one argument that has the same format as the `_TOKEN` states above, except that the FROM module is always the FE. 

XXX (cwabbott) usually, isa's have some sort of texture barrier or sync operation to be able to load textures asyncronously
(mali does it w/ pipeline registers) i'm wondering where that is in the vivante isa

Resolve
-----------
The resolve module is a glorified copy and fill engine. It can copy blocks of pixels
from one GPU address to another, optionally tiling/detiling. The source and destination address
can be the same for pixel format conversions, or to fill in tiles that were not touched
during the rendering process with the background color.

Tile status (Fast clear)
-------------------------
A render target is divided in tiles, and every tile has a couple of status flags.

An auxilary buffer for each render surface keeps track of tile status flags, allocated with `gcvSURF_TILE_STATUS`.

One of these flags is the `clear` flag, that signifies that the tile has been cleared.
`fast clear` happens by setting the clear bit for each tile instead of clearing the actual surface
data.

Tile size is dependent on the hardware, and so is the number of bits per tile (can be two or four).

The tile status bits are cleared using RS, by clearing a small surface with the value
0x55555555. When clearing, only the destination address and stride needs to be set,
the source is ignored.

Shader ISA
================

Vivante GPUs have unified shader ISA, this means that vertex and pixel shaders share the same 
instruction set. See `isa.xml` for details about the instructions, this section only provides a high-level overview.

- One operation consists of 4 32-bit words. This have a fixed format, which only differs very little per opcode. Which
instruction fields are used does differ per opcode.

- Four-component SIMD processor

- Older GPUs have floating point operations only, the newer ones have support for integer operations in the context of OpenCL. 
  The split is around GC1000, though this being Vivante there is likely some feature bit for it.

- Instructions can have up to three source operands (`SRC0_*`, `SRC1_*`, `SRC2_*`), and one destination operand (`DST_`). 
   In addition to that, there is a specific operand for texture sampling (`TEX_*`).

- For every operand there are these properties:
  - `USE`: the operand is enabled (1) or not (0)
  - `REG`: register number to read or write
  - `SWIZ`: arbitrary swizzle from four to four components (source operands only)
  - `COMPS`: which components to affect (destination operand only)
  - `AMODE`: addressing mode; this can either be direct or indexed through the X,Y,Z,W component of the address register
  - `RGROUP`: choses the register group to read from (source operands only). Register groups are the temporaries, uniforms, and
     possibly others.

- Registers:
  - N temporary registers (actual number depends on the hardware, seems to be at least 64)
  - 1 address register

Temporary registers are also used for shader inputs (attributes, varyings) and outputs (colors, positions). They are set to
the input values before the shader executes, and should have the output values when the shader ends. If the output
should be the same as the input (passthrough) an empty shader can be used.

Programming pecularities
=========================

- The FE can convert from 16.16 fixed point format to 32 bit float. This is enabled by the `fixp` bit
  in the `LOAD_STATE` command. This is mostly useful for older ARM CPUs without native floating point
  support. The blob driver uses it for some states (viewport scaling, offset, scissor, ...)
  but not others (uniforms etc). 

  - Some of the states in states.xml are labeled as format "fixp" even though the FE does conversion and
    their actual format is float, and they could be written as float as well when this is faster
    from the driver perspective. This needs to be checked.

Masked state
-------------

Many groups of state bits, especially in the PE, have mask bits. These have been named `*_MASK`.
When the mask bit belonging to a group of state bits is set on a state write, the accompanying
state bits will be unaffected. If the mask bit is unset, the state bits will be written.

This allows setting state either per group of bits, or all at once. For example, it allows setting only
the destination alpha function (`ALPHA_CONFIG.DST_FUNC_ALPHA`) without affecting the 
other bits in that state word.

If masking functionality is not desired, as it is often practical to simply write all bits at once, simply keep all the `_MASK`
bits at zero.

Texture tiling
----------------
RGBA/RGBx textures and render targets are stored in a 4x4 tiled format.

    Tile 1        Tile 2       ... Tile w-1
    0  1  2  3    16 17 18 19
    4  5  6  7    20 21 22 23
    8  9  10 11   24 25 26 27
    12 13 14 15   28 29 30 31

The stride of these tiled surfaces is the number of bytes between one row of tiles and the next. So for a surface of width
512, it is `(512/4)*16*4=8192`.

Render buffers
---------------

It appears that render buffers pixel sizes are padded to a multiple of 64, ie, a width of 400 becomes 448 and 800 becomes 832.

