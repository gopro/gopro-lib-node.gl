void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_tex0_coord = (tex0_coord_matrix * vec4(ngl_uvcoord, 0.0, 1.0)).xy;
    var_tex1_coord = (tex1_coord_matrix * vec4(ngl_uvcoord, 0.0, 1.0)).xy;
}
