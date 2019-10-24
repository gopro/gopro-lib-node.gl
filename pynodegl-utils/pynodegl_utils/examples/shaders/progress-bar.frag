void main()
{
    float pad    = 2.0 / 100.;
    float padw   = pad;
    float padh   = pad * ar;
    float height = 3.0 / 100. * ar;
    float thick  = 0.3 / 100.;
    float thickw = thick;
    float thickh = thick * ar;

    float x = var_uvcoord.x;
    float y = var_uvcoord.y;
    vec4 video_pix = ngl_texvideo(tex0, var_tex0_coord);

    float bar_width = 1. - padw*2. - thickw*2.;

    float t = tex0_ts/media_duration*bar_width + padw+thickw;

    if (x < t && x > padw+thickw &&
        y < 1.-padh-thickh && y > 1.-padh-height) {
        vec4 color = vec4(1., 0., 0., 1.);
        ngl_out_color = mix(video_pix, color, 0.7);
    } else {
        ngl_out_color = video_pix;
    }

    if (y < 1.-padh-thickh && y > 1.-padh-height &&
        ((x > padw && x < padw+thickw) ||
         (x > 1.-padw-thickw && x < 1.-padw))) {
        ngl_out_color = vec4(1.);
    }

    if (x < 1.-padw-thickw && x > padw+thickw &&
        ((y < 1.-padh-height && y > 1.-padh-height-thickh) ||
         (y < 1.-padh && y > 1. - padh-thickh))) {
        ngl_out_color = vec4(1.);
    }
}
