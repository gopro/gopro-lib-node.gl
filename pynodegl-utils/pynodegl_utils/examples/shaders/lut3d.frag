precision mediump float;

uniform mediump sampler2D tex0_sampler;
uniform mediump sampler3D lut3d_sampler;
uniform float xsplit;

in vec2 var_tex0_coord;
out vec4 oclr;

void main()
{
    vec4 iclr = texture(tex0_sampler, var_tex0_coord);
    float splitdist = var_tex0_coord.x - xsplit;

    // use a condition instead of step to prevent a texture lookup
    if (splitdist < 0.0)
        oclr = iclr;
    else
        oclr = texture(lut3d_sampler, iclr.rgb);

    // separator
    oclr *= smoothstep(0.0, 0.003, abs(splitdist)); // separator
}
