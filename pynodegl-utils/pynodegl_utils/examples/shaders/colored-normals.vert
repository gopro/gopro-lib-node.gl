void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
    var_normal = ngl_normal_matrix * ngl_normal;
}
