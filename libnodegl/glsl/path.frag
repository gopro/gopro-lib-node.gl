void main()
{
    float v = ngl_tex2d(tex, var_tex_coord).r;
    float d = fill ? v : abs(v) - .005;
    float a = 1. - clamp(d / fwidth(d) + .5, 0.0, 1.0);
    ngl_out_color = vec4(vec3(a), 1.0);
}
