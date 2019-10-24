void main()
{
    vec4 c1 = ngl_texvideo(tex0, var_tex0_coord);
    vec4 c2 = ngl_texvideo(tex1, var_tex1_coord);
    vec4 c3 = vec4(vec3(c1.rgb * delta + c2.rgb * (1.0 - delta)), 1.0);
    ngl_out_color = c3;
}
