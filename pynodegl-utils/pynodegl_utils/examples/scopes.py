import math
import array
import pynodegl as ngl
from pynodegl_utils.misc import scene
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.toolbox.utils import blend, canvas


@scene(alpha=scene.Range(range=[0, 1], unit_base=100))
def histogram(cfg, alpha=0.8):
    m = cfg.medias[0]
    cfg.duration = m.duration
    cfg.aspect_ratio = (m.width, m.height)

    source_w, source_h = m.width, m.height
    c, source_texture = canvas(cfg, m)

    # Proxy
    use_proxy = True
    source_write = None
    if use_proxy:
        source_w, source_h, source_write, source_texture = proxy(cfg, c)

    # Histogram data
    hdata = ngl.Block()
    hdata.add_fields(
        ngl.BufferUInt(256, label='r'),
        ngl.BufferUInt(256, label='g'),
        ngl.BufferUInt(256, label='b'),
    )

    # Reset buffers to 0
    clear_threads = 128
    clear_workgroups = 256 // clear_threads
    assert clear_workgroups * clear_threads == 256
    compute_program = ngl.ComputeProgram(cfg.get_comp('histogram-clear'), workgroup_size=(clear_threads, 1, 1))
    compute_program.update_properties(hist=ngl.ResourceProps(writable=True))
    compute_clear = ngl.Compute(workgroup_count=(clear_workgroups, 1, 1), program=compute_program, label='histogram-clear')
    compute_clear.update_resources(hist=hdata)

    # Compute histogram
    exec_threads = (16, 16, 1)
    exec_workgroups = (
        math.ceil(source_w / exec_threads[0]),
        math.ceil(source_h / exec_threads[1]),
        1
    )
    compute_program = ngl.ComputeProgram(cfg.get_comp('histogram-exec'), workgroup_size=exec_threads)
    compute_program.update_properties(hist=ngl.ResourceProps(writable=True))
    compute_program.update_properties(source=ngl.ResourceProps(as_image=True))
    compute_hist = ngl.Compute(workgroup_count=exec_workgroups, program=compute_program, label='histogram-exec')
    compute_hist.update_resources(hist=hdata, source=source_texture)

    # Sorted histogram (used to get the medians)
    sdata = ngl.Block()
    sdata.add_fields(
        ngl.BufferUInt(256, label='r'),
        ngl.BufferUInt(256, label='g'),
        ngl.BufferUInt(256, label='b'),
    )
    sort_program = ngl.ComputeProgram(cfg.get_comp('sort'), workgroup_size=(1, 1, 1))
    sort_program.update_properties(odata=ngl.ResourceProps(writable=True))
    compute_sort = ngl.Compute(workgroup_count=(1, 1, 1), program=sort_program, label='histogram-sort')
    compute_sort.update_resources(idata=hdata, odata=sdata)

    # Display histogram
    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    p = ngl.Program(vertex=cfg.get_vert('histogram-display'), fragment=cfg.get_frag('histogram-display'))
    p.update_vert_out_vars(var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2())
    render_hist = ngl.Render(q, p, label='histogram')
    render_hist.update_frag_resources(hist=hdata, shist=sdata, alpha=ngl.UniformFloat(alpha))

    # Blend the media and the histogram together
    display = blend(canvas, render_hist)

    root = ngl.Group()
    if source_write is not None:
        root.add_children(source_write)
    root.add_children(compute_clear, compute_hist, compute_sort, display)
    return root


@scene(
    alpha=scene.Range(range=[0, 1], unit_base=100),
    parade=scene.Bool(),
)
def waveform(cfg, alpha=1.0, parade=True):
    m = cfg.medias[0]
    cfg.duration = m.duration
    cfg.aspect_ratio = (m.width, m.height)

    source_w, source_h = m.width, m.height
    c, source_texture = canvas(cfg, m)

    # Proxy
    use_proxy = True
    source_write = None
    if use_proxy:
        source_w, source_h, source_write, source_texture = proxy(cfg, c)

    # Waveform data
    data_w, data_h = source_w, 256
    hdata = ngl.Block(label='waveform_block')
    hdata.add_fields(
        ngl.BufferUInt(data_h * data_w, label='r'),
        ngl.BufferUInt(data_h * data_w, label='g'),
        ngl.BufferUInt(data_h * data_w, label='b'),
        ngl.BufferUInt(data_h * data_w, label='y'),
        #ngl.UniformUInt(label='max_rgb'),
        #ngl.UniformUInt(label='max_luma'),
    )

    # Reset buffers to 0
    clear_threads = (16, 16, 1)
    clear_workgroups = (
        math.ceil(data_w / clear_threads[0]),
        math.ceil(data_h / clear_threads[1]),
        1
    )
    compute_program = ngl.ComputeProgram(cfg.get_comp('waveform-clear'), workgroup_size=clear_threads)
    compute_program.update_properties(hist=ngl.ResourceProps(writable=True))
    compute_clear = ngl.Compute(workgroup_count=clear_workgroups, program=compute_program, label='waveform-clear')
    compute_clear.update_resources(
        hist=hdata,
        data_w=ngl.UniformUInt(data_w),
        data_h=ngl.UniformUInt(data_h),
    )

    # Compute histogram
    # XXX: we could probably have that same shader compute the global histogram
    # as well
    exec_threads = (16, 16, 1)
    exec_workgroups = (
        math.ceil(source_w / exec_threads[0]),
        math.ceil(source_h / exec_threads[1]),
        1
    )
    compute_program = ngl.ComputeProgram(cfg.get_comp('waveform-exec'), workgroup_size=exec_threads)
    compute_program.update_properties(hist=ngl.ResourceProps(writable=True))
    compute_program.update_properties(source=ngl.ResourceProps(as_image=True))
    compute_hist = ngl.Compute(workgroup_count=exec_workgroups, program=compute_program, label='waveform-exec')
    compute_hist.update_resources(
        hist=hdata,
        source=source_texture,
        luma_weights=ngl.UniformVec3(value=(.2126, .7152, .0722)),  # BT.709
    )

    # Sorted histogram (used to get the medians)
    sort_threads = (64, 1, 1)
    sort_workgroups = (math.ceil(data_w / sort_threads[0]), 1, 1)
    sdata = ngl.Block()
    sdata.add_fields(
        ngl.BufferUInt(data_h * data_w, label='r'),
        ngl.BufferUInt(data_h * data_w, label='g'),
        ngl.BufferUInt(data_h * data_w, label='b'),
    )
    sort_program = ngl.ComputeProgram(cfg.get_comp('sort'), workgroup_size=sort_threads)
    sort_program.update_properties(odata=ngl.ResourceProps(writable=True))
    compute_sort = ngl.Compute(workgroup_count=sort_workgroups, program=sort_program, label='histogram-sort')
    compute_sort.update_resources(idata=hdata, odata=sdata)

    # Display waveform
    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    p = ngl.Program(vertex=cfg.get_vert('waveform-display'), fragment=cfg.get_frag('waveform-display'))
    p.update_vert_out_vars(var_uvcoord=ngl.IOVec2(), var_tex0_coord=ngl.IOVec2())
    render = ngl.Render(q, p, label='waveform')
    render.update_frag_resources(
        hist=hdata,
        shist=sdata,
        alpha=ngl.UniformFloat(alpha),
        max_width=ngl.UniformUInt(source_w - 1),
        parade=ngl.UniformBool(parade),
        luma_weights=ngl.UniformVec3(value=(.2126, .7152, .0722)),  # BT.709
    )

    # Blend the media and the histogram together
    display = blend(c, render)

    root = ngl.Group()
    if source_write is not None:
        root.add_children(source_write)
    root.add_children(compute_clear, compute_hist, compute_sort, display)
    return root
