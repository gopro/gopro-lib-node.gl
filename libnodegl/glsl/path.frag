#include path.glsl

void main()
{
    float dist = ngl_tex2d(tex, var_tex_coord).r;
    ngl_out_color = get_path_color(dist, color, glow, glow_color, blur, outline);
}
