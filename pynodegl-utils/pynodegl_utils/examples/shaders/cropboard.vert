#version 100

precision highp float;

attribute vec4 ngl_position;
attribute vec2 ngl_uvcoord;
uniform mat4 ngl_modelview_matrix;
uniform mat4 ngl_projection_matrix;

uniform mat4 tex0_coord_matrix;

varying vec2 var_tex0_coord;

uniform float time;
attribute vec2 uv_offset;
attribute vec2 translate_a;
attribute vec2 translate_b;

void main()
{
    vec4 position = ngl_position + vec4(mix(translate_a, translate_b, time), 0.0, 0.0);
    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * position;
    var_tex0_coord = (tex0_coord_matrix * vec4(ngl_uvcoord + uv_offset, 0.0, 1.0)).xy;
}
