vec4 get_path_color(float dist, vec4 color, float glow, vec4 glow_color, float blur, float outline)
{
    float d = outline > 0.0 ? -(abs(dist) - outline) : dist;
    float a = blur > 0.0 ? smoothstep(-blur, blur, d)
                         : clamp(d / fwidth(d) + 0.5, 0.0, 1.0);

    vec4 out_color = vec4(color.rgb, color.a * a);

    if (glow > 0.0) {
        // XXX +1.0 should be *2.0 instead, but it's a hack for the
        // noise_cyberretro retro
        float glow_a = smoothstep(-0.5, 0.5, d) * (glow + 1.0);
        out_color = mix(out_color, glow_color, glow_a);
    }

    return out_color;
}
