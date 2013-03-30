Directory contents
===================

- `egl`: egl demos, to check GL functionality

- `replay`: original replay command stream tests (very low level)

- `fb`: attempts at rendering to framebuffer using `etna_pipe` (high level gallium-like interface)

- `fb_rawshader`: same as `fb`, but using manually assembled shaders. &lt;GC1000 only.

- `fb_old`: attempts at rendering to framebuffer using raw state queueing (lower level interface)

- `include_*`: different versions of Vivante GPL kernel headers

- `lib`: C files shared between demos and tests, generic math and GL context utilities etc

- `driver`: `etna_pipe` driver and generated hardware header files

- `resources`: meshes, textures used in demos


