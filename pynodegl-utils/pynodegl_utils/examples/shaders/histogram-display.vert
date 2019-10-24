void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
    var_uvcoord = ngl_uvcoord;
    var_tex0_coord = (tex0_coord_matrix * vec4(ngl_uvcoord, 0., 1.)).xy;
}
