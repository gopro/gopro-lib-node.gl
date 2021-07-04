void main()
{
    ngl_out_pos = projection_matrix * modelview_matrix * vec4(position.xy, 0.0, 1.0);
    var_tex_coord = position.zw;
}
