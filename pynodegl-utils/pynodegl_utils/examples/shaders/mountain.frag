float f(float t)
{
    // 6t^5 - 15t^4 + 10t^3 (new Perlin)
    return ((6.0*t - 15.0)*t + 10.0)*t*t*t;
}

float pick_rand1d(float pos, float off)
{
    float value_point = fract(pos + off);
    float value = ngl_tex2d(tex0, vec2(value_point, 0)).x;
    return value;
}

float noise1d(float pos)
{
    float d = float(dim);
    float s = 1.0 / d;
    float v0 = pick_rand1d(pos, 0.0 * s);
    float v1 = pick_rand1d(pos, 1.0 * s);
    float t = pos*d - floor(pos*d);
    float tx = f(t);
    float nx = mix(v0, v1, tx);
    return nx;
}

void main()
{
    float sum = 0.0;
    float max_amp = 0.0;
    float freq = 1.0;
    float amp = 1.0;
    float pos = var_tex0_coord.x/2.0 + time;
    for (int i = 0; i < nb_layers; i++) {
        float nval = noise1d(pos * freq) * amp;
        sum += nval;
        max_amp += amp;
        freq *= lacunarity;
        amp *= gain;
    }
    float n = sum / max_amp;
    float h = n*hscale + yoffset;
    float alpha_blend_h = 0.002;
    float y = 1.0 - var_tex0_coord.y;
    float alpha = smoothstep(y-alpha_blend_h, y+alpha_blend_h, h);

    ngl_out_color = vec4(mcolor.rgb, alpha);
}
