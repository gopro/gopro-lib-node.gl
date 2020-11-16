NGL tools
=========

The [ngl-tools][ngl-tools] is a set of small C programs making use of
`libnodegl`.

[ngl-tools]: /ngl-tools


## ngl-player

`ngl-player` is a basic real time video player. If you are looking into
testing or diagnose video playback and seeking issues, this tool is likely a
good start.

**Usage**: `ngl-player [options] <media>`

The detail of available options can be obtained with `ngl-player -h`.

![ngl-player](img/ngl-player.png)

**Source**: [ngl-tools/ngl-player.c](/ngl-tools/ngl-player.c)


## ngl-render

`ngl-render` is a rendering test tool. It takes a serialized scene as input
(`input.ngl` or `stdin` if not specified) and render the specified time ranges
(by default, in a hidden window).

**Usage**: `ngl-render [-o out.raw] [-s WxH] [-w] [-d] [-z swapinterval]
-t start:duration:freq [-t start:duration:freq ...] [-i input.ngl]`

Option                      | Description
--------------------------- | ---------------------------
`-o <out.raw>`              | specify the raw output file, "-" can be used for stdout output
`-s <WxH>`                  | specify the output dimensions in `WxH` format
`-w`                        | if specified, the rendering window will be shown
`-d`                        | enable debugging (of the tool)
`-z <swapinterval>`         | specify the OpenGL swapping interval (useful in combination with `-w`); `0` (the default) means non capped while `1` corresponds to the vsync
`-t <start:duration:freq>`  | specify a time range to render in `start:duration:freq` format. All three values are floats.  `start` is the start time of the range (in seconds), `duration` is the duration of the range (also in seconds), and `freq` is the refresh frame rate.


**Example**: `ngl-serialize pynodegl_utils.examples.misc fibo - | ngl-render -t 0:60:60 -s 640x480 -o - | ffplay -f rawvideo -framerate 60 -video_size 640x480 -pixel_format rgba -`

**Source**: [ngl-tools/ngl-render.c](/ngl-tools/ngl-render.c)


## ngl-python

`ngl-python` is a `node.gl` Python scene loader. It uses the C API of Python to
execute a demo script (in order to get the scene) and render it using the
`libnodegl` C API.

**Note**: it is only available if the Python headers are present on the system
at build time.

**Usage**: `ngl-python [options] <module|script.py> <scene_func>`

The detail of available options can be obtained with `ngl-python -h`.

**Example**: `ngl-python pynodegl_utils.examples.misc fibo`

**Source**: [ngl-tools/ngl-python.c](/ngl-tools/ngl-python.c)


## ngl-serialize

`ngl-serialize` serializes a `node.gl` Python scene into the `ngl` format.
Similarly to `ngl-python`, it relies on the C API of Python to execute the
specified entry point.

**Note**: it is only available if the Python headers are present on the system
at build time.

**Usage**: `ngl-serialize <module> <scene_func> <output.ngl>`

**Example**: `ngl-serialize pynodegl_utils.examples.misc fibo -`

**Source**: [ngl-tools/ngl-serialize.c](/ngl-tools/ngl-serialize.c)


## ngl-desktop

`ngl-desktop` is a player waiting for user commands from a socket connection.
It is meant to be used with `ngl-ipc` for communicating commands.

By default, `ngl-desktop` listen for connections on `localhost` on port `1234`.

The detail of available options can be obtained with `ngl-desktop -h`.

**Example**: `ngl-desktop -x 0.0.0.0 -p 2000 --backend opengles -c 223344FF`


## ngl-ipc

`ngl-ipc` is the tool used to make queries to `ngl-desktop` instances.

The detail of available options can be obtained with `ngl-ipc -h`.

**Example**: `ngl-serialize pynodegl_utils.examples.misc fibo - | ngl-ipc -p 2000 -f - -t 5`


## ngl-probe

`ngl-probe` is a backend capabilities probing tool.

By default, `ngl-probe` prints the capabilities of every available backends,
following a YAML output layout. If the `--cap` option is specified, it will
query the specified backend if set (`-b`/`--backend`) or query the default one.

The detail of available options can be obtained with `ngl-probe -h`.


## Player keyboard controls

`ngl-player` and `ngl-python` are both scene players supporting the following
keyboard controls:

Key           | Action
------------- | ------
`ESC` or `q`  | quit the application
`SPACE`       | toggle the pause/playback
`LEFT`        | seek by -10 seconds
`RIGHT`       | seek by +10 seconds
`f`           | toggle windowed/fullscreen
`s`           | take a screenshot
`k`           | kill the current scene
`h`           | toggle the HUD
