void main() {
    var_tex_coord = uvcoord;
    var_glyph_id = gl_VertexID / 4;
    mat4 transform = chr_transform[var_glyph_id];
    ngl_out_pos = projection_matrix * transform * modelview_matrix * vec4(position, 1.0);
}
