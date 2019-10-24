void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
    var_color = edge_color;
}
