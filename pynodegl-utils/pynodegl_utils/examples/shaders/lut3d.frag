void main()
{
    vec4 iclr = ngl_texvideo(tex0, var_tex0_coord);
    float splitdist = var_tex0_coord.x - xsplit;

    // use a condition instead of step to prevent a texture lookup
    if (splitdist < 0.0)
        ngl_out_color = iclr;
    else
        ngl_out_color = ngl_tex3d(lut3d, iclr.rgb);

    // separator
    ngl_out_color *= smoothstep(0.0, 0.003, abs(splitdist)); // separator
}
