void main() {
    vec4 color = ngl_texvideo(tex0, var_tex0_coord);
    ngl_out_color = vec4(vec3(0.5 + color.rgb * var_normal), 1.0);
}
