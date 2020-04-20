#version 100
precision mediump float;

uniform float ar;
uniform sampler2D tex0_sampler;
uniform float tex0_ts;
uniform float media_duration;
varying vec2 var_tex0_coord;
varying vec2 var_uvcoord;

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
    vec4 video_pix = texture2D(tex0_sampler, var_tex0_coord);

    float bar_width = 1. - padw*2. - thickw*2.;

    float t = tex0_ts/media_duration*bar_width + padw+thickw;

    if (x < t && x > padw+thickw &&
        y < 1.-padh-thickh && y > 1.-padh-height) {
        vec4 color = vec4(1., 0., 0., 1.);
        gl_FragColor = mix(video_pix, color, 0.7);
    } else {
        gl_FragColor = video_pix;
    }

    if (y < 1.-padh-thickh && y > 1.-padh-height &&
        ((x > padw && x < padw+thickw) ||
         (x > 1.-padw-thickw && x < 1.-padw))) {
        gl_FragColor = vec4(1.);
    }

    if (x < 1.-padw-thickw && x > padw+thickw &&
        ((y < 1.-padh-height && y > 1.-padh-height-thickh) ||
         (y < 1.-padh && y > 1. - padh-thickh))) {
        gl_FragColor = vec4(1.);
    }
}
