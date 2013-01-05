
This document contains some examples of disassembled shaders.

For details of the shader ISA see `rnndb/isa.xml`.

Vertex shader examples
========================

Basic vertex shader
--------------------

    uniform mat4 modelviewMatrix;
    uniform mat4 modelviewprojectionMatrix;
    uniform mat3 normalMatrix;
    
    attribute vec4 in_position;
    attribute vec3 in_normal;
    attribute vec4 in_color;
    
    vec4 lightSource = vec4(2.0, 2.0, 20.0, 0.0);
    
    varying vec4 vVaryingColor;
    
    void main()
    {
        gl_Position = modelviewprojectionMatrix * in_position;
        vec3 vEyeNormal = normalMatrix * in_normal;
        vec4 vPosition4 = modelviewMatrix * in_position;
        vec3 vPosition3 = vPosition4.xyz / vPosition4.w;
        vec3 vLightDir = normalize(lightSource.xyz - vPosition3);
        float diff = max(0.0, dot(vEyeNormal, vLightDir));
        vVaryingColor = vec4(diff * in_color.rgb, 1.0);
    }

Becomes:

    MOV t3.xy, void, void, u4.wwww
    MOV t3.z, void, void, u5.wwww
    MUL t4, u0, t0.xxxx, void
    MAD t4, u1, t0.yyyy, t4
    MAD t4, u2, t0.zzzz, t4
    MAD t4, u3, t0.wwww, t4
    MUL t5.xyz, u4.xyzz, t1.xxxx, void
    MAD t5.xyz, u5.xyzz, t1.yyyy, t5.xyzz
    MAD t1.xyz, u6.xyzz, t1.zzzz, t5.xyzz
    MUL t5, u7, t0.xxxx, void
    MAD t5, u8, t0.yyyy, t5
    MAD t5, u9, t0.zzzz, t5
    MAD t0, u10, t0.wwww, t5
    RCP t1.w, void, void, t0.wwww
    MAD t0.xyz, -t0.xyzz, t1.wwww, t3.xyzz
    DP3 t3.xyz, t0.xyzz, t0.xyzz, void
    RSQ t3.xyz, void, void, t3.xxxx
    MUL t0.xyz, t0.xyzz, t3.xyzz, void
    DP3 t0.x, t1.xyzz, t0.xyzz, void
    SELECT.LT t0.x, u6.wwww, t0.xxxx, u6.wwww
    MUL t0.xyz, t0.xxxx, t2.xyzz, void
    MOV t0.w, void, void, u11.xxxx
    ADD t4.z, t4.zzzz, void, t4.wwww
    MUL t4.z, t4.zzzz, u11.yyyy, void

Uniform mapping:

    u0 - u3          modelviewprojectionMatrix
    u4.xyz - u6.xyz  normalMatrix
    u7 - u10         modelviewMatrix
    u11.xy           1.0, 0.5 (constants used internally)
    u4.w - u6.w      2.0, 20.0, 0.0 (lightSource components, and a zero)

Vertex shader with texture coordinates
---------------------------------------

    uniform mat4 modelviewMatrix;
    uniform mat4 modelviewprojectionMatrix;
    uniform mat3 normalMatrix;
   
    attribute vec4 in_position;
    attribute vec3 in_normal;
    attribute vec2 in_coord;
    
    vec4 lightSource = vec4(2.0, 2.0, 20.0, 0.0);
    
    varying vec4 vVaryingColor;
    varying vec2 coord;
    
    void main()
    {
        gl_Position = modelviewprojectionMatrix * in_position;
        vec3 vEyeNormal = normalMatrix * in_normal;
        vec4 vPosition4 = modelviewMatrix * in_position;
        vec3 vPosition3 = vPosition4.xyz / vPosition4.w;
        vec3 vLightDir = normalize(lightSource.xyz - vPosition3);
        float diff = max(0.0, dot(vEyeNormal, vLightDir));
        vVaryingColor = vec4(diff * vec3(1.0, 1.0, 1.0), 1.0);
        coord = in_coord;
    }

Becomes:

    MOV t3.xy, void, void, u4.wwww
    MOV t3.z, void, void, u5.wwww
    MUL t4, u0, t0.xxxx, void
    MAD t4, u1, t0.yyyy, t4
    MAD t4, u2, t0.zzzz, t4
    MAD t4, u3, t0.wwww, t4
    MUL t5.xyz, u4.xyzz, t1.xxxx, void
    MAD t5.xyz, u5.xyzz, t1.yyyy, t5.xyzz
    MAD t1.xyz, u6.xyzz, t1.zzzz, t5.xyzz
    MUL t5, u7, t0.xxxx, void
    MAD t5, u8, t0.yyyy, t5
    MAD t5, u9, t0.zzzz, t5
    MAD t0, u10, t0.wwww, t5
    RCP t1.w, void, void, t0.wwww
    MAD t0.xyz, -t0.xyzz, t1.wwww, t3.xyzz
    DP3 t3.xyz, t0.xyzz, t0.xyzz, void
    RSQ t3.xyz, void, void, t3.xxxx
    MUL t0.xyz, t0.xyzz, t3.xyzz, void
    DP3 t0.x, t1.xyzz, t0.xyzz, void
    SELECT.LT t0.xyz, u6.wwww, t0.xxxx, u6.wwww
    MOV t0.w, void, void, u11.xxxx
    MOV t1.xy, void, void, t2.xyyy
    ADD t4.z, t4.zzzz, void, t4.wwww
    MUL t4.z, t4.zzzz, u11.yyyy, void

Pixel shaders
==============

Empty (passthrough)
--------------------

    precision mediump float;
    
    varying vec4 vVaryingColor;
    
    void main()
    {
        gl_FragColor = vVaryingColor;
    }

Becomes:

    NOP void, void, void, void

Texture sampling
------------------

    precision mediump float;
    
    varying vec4 vVaryingColor;
    varying vec2 coord;
    
    uniform sampler2D in_texture;
    
    void main()
    {
        gl_FragColor = 3.0 * vVaryingColor * texture2D(in_texture, coord);
    }

Becomes:

    MUL t1, u0.xxxx, t1, void
    TEXLD t2, tex0, t2.xyyy, void, void
    MUL t1, t1, t2, void

