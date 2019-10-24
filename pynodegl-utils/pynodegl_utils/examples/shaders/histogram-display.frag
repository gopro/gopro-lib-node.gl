void main()
{
    vec4 img  = ngl_texvideo(tex0, var_tex0_coord);
    uint x = clamp(uint(var_uvcoord.x * 255.0 + 0.5), 0U, 255U);
    vec3 rgb = vec3(hist.r[x], hist.g[x], hist.b[x]) / float(hist.maximum);
    float y = var_uvcoord.y;
    vec3 ymax = vec3(1.0/3.0, 2.0/3.0, 1.0);
    vec3 v = rgb * 1.0/3.0; // [0;+oo] -> [0;1/3]
    vec3 yzero = ymax - v;
    vec3 yval = step(y, ymax) * (1.0 - step(y, yzero)); // y <= ymax && y > yzero
    vec4 hcolor = vec4(yval, 1.0);
    ngl_out_color = mix(img, hcolor, .6);
}
