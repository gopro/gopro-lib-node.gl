#version 100
precision mediump float;
varying vec2 var_tex0_coord;
varying vec2 var_tex1_coord;
uniform sampler2D tex0_sampler;
uniform sampler2D tex1_sampler;
uniform float delta;
void main()
{
    vec4 c1 = texture2D(tex0_sampler, var_tex0_coord);
    vec4 c2 = texture2D(tex1_sampler, var_tex1_coord);
    vec4 c3 = vec4(vec3(c1.rgb * delta + c2.rgb * (1.0 - delta)), 1.0);
    gl_FragColor = c3;
}
