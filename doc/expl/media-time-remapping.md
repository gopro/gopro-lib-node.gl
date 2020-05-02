Media time remapping
====================

When using one or more `Media` nodes in a demo, it is often needed to map the
`node.gl` time (referred in this document as `t`) to a different media time
(referred in this document as `Tm`). This time remapping is controlled by the
`Media.time_anim` parameter and `TimeRangeFilter` nodes.

All the concepts explained here are summarized in the demo
`medias.time_remapping`.

## Time anim key frames

`Media.time_anim` is an animation composed of at least one animation key frame.
Each key frame associates a `node.gl` time to a media time: `f(t) = Tm` with
`f()` being the time interpolation function.

### Example:

```python
anim = ngl.AnimatedTime([
    ngl.AnimKeyFrameFloat(5, 4),
    ngl.AnimKeyFrameFloat(7, 10),
])
```

In this example, for `t=5` we have `Tm=4`, and for `t=7` we have `Tm=10`. This
means `10-4=6` seconds of the media will be played starting at `t=5` in `7-5=2`
seconds (which means a faster playback).

Since the linear interpolation is handled by `AnimatedFloat`, it has the same
properties out-of-bound. In this case, we have `Tm=4` for `t≤5` and `Tm=10` for
`t≥7`.

**Note**: specifying only one key frame is possible and will simply define the
initial starting time of the video for a given time `t`.


### Limitations

There are a few limitations due to the streaming nature of medias:

- time must be monotonically incrementing (reverse playback is forbidden)
- only linear interpolation is allowed
- you are obviously limited by the decoding speed of your host, so you can't
  accelerate the speedup infinitely
- seeking is exact, so a performance hit may be observed while doing so


## Time range filters

While `Media.time_anim` does define the time remapping, there is still a need
to define the active range of the media. This is controlled through
`TimeRangeFilter` nodes.

The time range filters will allow the prefetch and release mechanisms of the
sub-tree (in our case, it will typically be a `Render` using a `Media` node as
`data_src`).

The typical use case for a video showing up randomly in a demo is to define 3
time ranges, such as:


```python
timefilter = ngl.TimeRangeFilter(my_render, ranges=[
    ngl.TimeRangeModeNoop(0),
    ngl.TimeRangeModeCont(2),
    ngl.TimeRangeModeNoop(9),
])
```

In this case, `my_render` will be visible between `t=2` and `t=9`.


## Illustrated graph

![Media time remapping](media-time-remapping.png)

Associated code:

```python
import pynodegl as ngl
from pynodegl_utils.misc import scene


@scene()
def media_time_remapping(cfg):

    # Time remapping
    animkf = [
        ngl.AnimKeyFrameFloat(5, 4),
        ngl.AnimKeyFrameFloat(7, 10),
    ]

    # Basic media playback tree
    q = ngl.Quad()
    m = ngl.Media(cfg.medias[0].filename, time_anim=ngl.AnimatedTime(animkf))
    t = ngl.Texture2D(data_src=m)
    r = ngl.Render(q)
    r.update_frag_resources(tex0=t)

    # Time range filter
    tf = ngl.TimeRangeFilter(r, ranges=[
        ngl.TimeRangeModeNoop(0),
        ngl.TimeRangeModeCont(2),
        ngl.TimeRangeModeNoop(9),
    ])

    return tf
```

## Behind the scene

For media playback `node.gl` relies on the `sxplayer` project. Internally, the
`sxplayer` context is configured with an `initial_seek` and a `trim_duration`
based on the `Media.time_anim` user configuration. These settings respectively
help getting the first frame quickly and closing the media and resources faster
when reaching the end of the trim:

- `initial_seek` notably makes sure a seek command is queued to the demuxer
  before any packet is extracted.
- `trim_duration` prevents the `sxplayer` queues to be filled until a
  `TimeRangeFilter` triggers a release emptying these queues.

Additionally, the `TimeRangeFilter` will cause asynchronous calls starting and
stopping (*prefetch* and *release*) the multimedia pipeline in background
(demuxer, decoder, ...).

Coupled with hardware acceleration, these two main mechanism help getting great
performances at a minimal memory cost.
