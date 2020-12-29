void main()
{
    vec4 position = vec4(ngl_position, 1.0) + vec4(mix(translate_a, translate_b, time), 0.0, 0.0);
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * position;
    var_tex0_coord = (tex0_coord_matrix * vec4(ngl_uvcoord + uv_offset, 0.0, 1.0)).xy;
}
