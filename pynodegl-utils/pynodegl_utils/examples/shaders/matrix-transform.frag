void main()
{
    vec2 coords = var_tex0_coord * 2.0 - 1.0;
    coords = (matrix * vec4(coords.xy, 1.0, 1.0)).xy;
    coords = (coords + 1.0) / 2.0;

    ngl_out_color = ngl_texvideo(tex0, coords);
}
