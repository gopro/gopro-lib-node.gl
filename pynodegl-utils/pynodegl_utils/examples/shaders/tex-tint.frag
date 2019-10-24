void main()
{
    vec4 t = ngl_texvideo(tex0, var_tex0_coord);
    ngl_out_color = vec4(mix(t.rgb, blend_color, mix_factor), 1.0);
}
