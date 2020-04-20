#version 100
precision mediump float;
uniform mat4 matrix;
uniform sampler2D tex0_sampler;
varying vec2 var_tex0_coord;

void main()
{
    vec2 coords = var_tex0_coord * 2.0 - 1.0;
    coords = (matrix * vec4(coords.xy, 1.0, 1.0)).xy;
    coords = (coords + 1.0) / 2.0;

    gl_FragColor = texture2D(tex0_sampler, coords);
}
