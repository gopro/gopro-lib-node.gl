#include path.glsl

void main()
{
    float dist = ngl_tex2d(tex, var_tex_coord).r;
    ngl_out_color = get_path_color(
        dist,
        chr_color[var_glyph_id],
        chr_glow[var_glyph_id],
        chr_glow_color[var_glyph_id],
        chr_blur[var_glyph_id],
        chr_outline[var_glyph_id]);
}
