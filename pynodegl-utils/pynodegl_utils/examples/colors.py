import colorsys
import pynodegl as ngl
from pynodegl_utils.misc import scene
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.toolbox.utils import canvas, gradient, gradient_line


@scene(
    colorize=scene.Color(),
    exposure=scene.Range(range=[-2, 2], unit_base=1000),
    saturation=scene.Range(range=[0, 2], unit_base=1000),
    contrast=scene.Range(range=[0, 2], unit_base=1000),
    lift=scene.Color(),
    gamma=scene.Color(),
    gain=scene.Color(),
)
def colors(
    cfg,
    colorize=(1, 1, 1, 0),
    exposure=0,
    saturation=1,
    contrast=1,
    lift=(0, 0, 0, 1),
    gamma=(1, 1, 1, 1),
    gain=(1, 1, 1, 1)
):
    m = cfg.medias[0]
    cfg.duration = m.duration
    cfg.aspect_ratio = (m.width, m.height)

    c = canvas(cfg, m,
        luma_weights=ngl.UniformVec3(value=(.2126, .7152, .0722)),  # BT.709
        colorize=ngl.UniformVec4(colorize),
        exposure=ngl.UniformFloat(exposure),
        saturation=ngl.UniformFloat(saturation),
        contrast=ngl.UniformFloat(contrast),
        lift=ngl.UniformVec4(lift),
        gamma=ngl.UniformVec4(gamma),
        gain=ngl.UniformVec4(gain),
    )

    return c


@scene()
def gradient4(cfg):
    return gradient(cfg, COLORS['red'], COLORS['green'], COLORS['blue'], COLORS['white'])


@scene(c=scene.Color())
def gradient_color(cfg, c=COLORS['red']):
    return gradient(cfg, COLORS['black'], COLORS['black'], COLORS['white'], c)


@scene(n=scene.Range(range=[2, 100]))
def gradient_hue_bar_nclr_interp(cfg, n=6):
    colors = [colorsys.hls_to_rgb(i / (n - 1), .5, 1) + (1,) for i in range(n)]
    for color in colors:
        r, g, b, _ = color
        print(color)
        #print(f'#{round(r*255):02x}{round(g*255):02x}{round(b*255):02x}')
    line = gradient_line(cfg, *colors)
    return ngl.Scale(line, (1, 1/6, 1))


@scene()
def gradient_hue_bar_frag(cfg):
    vert = '''
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
    var_uvcoord = ngl_uvcoord;
}
'''
    frag = '''
float get_val(float h)
{
    if (h < 1./6.) return 6.*h;         // 0/6->1/6: X (lerp(0/6, 1/6, h))
    if (h < 3./6.) return 1.;           // 1/6->3/6: C
    if (h < 4./6.) return 4. - 6.*h;    // 3/6->4/6: X (1-lerp(3/6, 4/6, h))
    return 0.;                          // 4/6->6/6: 0
}

void main()
{
    float h = var_uvcoord.x;
    float r = get_val(fract(h + 1./3.));    // R: C,X,0,0,X,C
    float g = get_val(h);                   // G: X,C,C,X,0,0
    float b = get_val(fract(h - 1./3.));    // B: 0,0,X,C,C,X
    ngl_out_color = vec4(r, g, b, 1.);
}'''
    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    p = ngl.Program(vertex=vert, fragment=frag)
    line = ngl.Render(q, p)
    p.update_vert_out_vars(var_uvcoord=ngl.IOVec2())
    return ngl.Scale(line, (1, 1/6, 1))
