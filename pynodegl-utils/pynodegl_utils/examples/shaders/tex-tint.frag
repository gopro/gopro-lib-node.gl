#version 100
precision mediump float;
uniform sampler2D tex0_sampler;
varying vec2 var_tex0_coord;
uniform vec3 blend_color;
uniform float mix_factor;
void main()
{
    vec4 t = texture2D(tex0_sampler, var_tex0_coord);
    gl_FragColor = vec4(mix(t.rgb, blend_color, mix_factor), 1.0);
}
