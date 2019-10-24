float wave(float amp, float y, float yoff)
{
    float s = (amp + 1.0) / 2.0; // [-1;1] -> [0;1]
    float v = yoff + s/4.0;         // [0;1] -> [off;off+0.25]
    return smoothstep(v-0.005, v, y)
         - smoothstep(v, v+0.005, y);
}

float freq(float power, float y, float yoff)
{
    float p = sqrt(power);
    float v = clamp(p, 0.0, 1.0) / 4.0; // [0;+oo] -> [0;0.25]
    float a = yoff + 0.25;
    float b = a - v;
    return step(y, a) * (1.0 - step(y, b)); // y <= a && y > b
}

void main()
{
    int freq_line = 2                          // skip the 2 audio channels
                  + (10 - freq_precision) * 2; // 2x10 lines of FFT
    float fft1 = float(freq_line) + 0.5;
    float fft2 = float(freq_line) + 1.5;
    float x = var_tex0_coord.x;
    float y = var_tex0_coord.y;
    vec4 video_pix = ngl_texvideo(tex1, var_tex1_coord);
    vec2 sample_id_ch_1 = vec2(x,  0.5 / 22.);
    vec2 sample_id_ch_2 = vec2(x,  1.5 / 22.);
    vec2  power_id_ch_1 = vec2(x, fft1 / 22.);
    vec2  power_id_ch_2 = vec2(x, fft2 / 22.);
    float sample_ch_1 = ngl_tex2d(tex0, sample_id_ch_1).x;
    float sample_ch_2 = ngl_tex2d(tex0, sample_id_ch_2).x;
    float  power_ch_1 = ngl_tex2d(tex0,  power_id_ch_1).x;
    float  power_ch_2 = ngl_tex2d(tex0,  power_id_ch_2).x;
    float wave1 = wave(sample_ch_1, y, 0.0);
    float wave2 = wave(sample_ch_2, y, 0.25);
    float freq1 = freq(power_ch_1, y, 0.5);
    float freq2 = freq(power_ch_2, y, 0.75);
    vec3 audio_pix = vec3(0.0, 1.0, 0.5) * wave2
                   + vec3(0.5, 1.0, 0.0) * wave1
                   + vec3(1.0, 0.5, 0.0) * freq1
                   + vec3(1.0, 0.0, 0.5) * freq2;
    ngl_out_color = mix(video_pix, vec4(audio_pix, 1.0), overlay);
}
