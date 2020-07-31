Using the C API
===============

All the C API is documented in the installed header [nodegl.h][nodegl-header].
Since the header documentation doesn't provide the big picture on how things
fit together, this how-to will serve as a guide.

Every public function and structure uses the `ngl_` prefix as namespace.
Similarly, macro, constants and enums use the `NGL_` prefix.  `NGLI_FOURCC()`
typically is **not** part of the public API and you should not use it.

**Note on the private API**: The `I` in `NGLI_FOURCC()` stands for *Internal*.
The `ngli_*` symbols share this meaning and correspond to symbols shared inside
the project but never exposed to the user (you will never see them in the
public header).

[nodegl-header]: /libnodegl/nodegl.h


## Compilation and linking with node.gl

In order to compile a source code using the `node.gl` C-API, the main and only
header has to be included:

```c
#include <nodegl.h>
```

This is a C header, so if you are writing code in another language, you must
use the appropriate method to include it. For example in C++, it will look
like this:

```c++
extern "C" {
#include <nodegl.h>
}
```

If you are using a standard build tool-chain, you will need `pkg-config` to
obtain the compilation and linker flags:

```shell
pkg-config --cflags --libs libnodegl
```

Check the `pkg-config` documentation and your build system for more
information.

## Create and configure the context

Allocating the context can be made at any time using `ngl_create()`:

```c
    struct ngl_ctx *ctx = ngl_create();
    if (!ctx)
        return -1;

    struct ngl_config config = {...};
    int ret = ngl_set_configure(ctx, &config);
    if (ret < 0)
        return ret;
```

## Constructing a scene

### Method 1: de-serializing an existing scene

When a scene is already available under the `node.gl` serialized form (`.ngl`),
obtaining the scene is a simple call:

```c
    struct ngl_node *scene = ngl_node_deserialize(str);
    if (!scene)
        return -1;
```

### Method 2: getting the scene from Python

This is a bit more complex and depends on how your scene is crafted in Python.
When you have access to your Python node object, you will have to get its
`cptr` attribute and convert it from `Long` to `Ptr` to obtain the native
`ngl_node` pointer.

You probably want to look at how [ngl-python][ngl-python] performs this
operation.

[ngl-python]: /doc/ref/ngl-tools.md#ngl-python

### Method 3: crafting the scene in C

The most straightforward method though, is to craft a scene directly using the
C API.

In this section, we will assume that you are familiar with the Python binding
and how a basic `Render` node works. If not, you are encouraged to check out
the [starter tutorial][tuto-start] before reading any further.

[tuto-start]: /doc/tuto/start.md

To create a node, you only need the `ngl_node_create()` function, which later
needs to be de-referenced with `ngl_node_unrefp()`.

Following is a function returning basic scene rendering a video into a
rectangle geometry:

```c
static struct ngl_node *get_scene(const char *filename)
{
    static const float corner[3] = {-1.0, -1.0, 0.0};
    static const float width[3]  = { 2.0,  0.0, 0.0};
    static const float height[3] = { 0.0,  2.0, 0.0};

    struct ngl_node *media   = ngl_node_create(NGL_NODE_MEDIA);
    struct ngl_node *texture = ngl_node_create(NGL_NODE_TEXTURE2D);
    struct ngl_node *quad    = ngl_node_create(NGL_NODE_QUAD);
    struct ngl_node *program = ngl_node_create(NGL_NODE_PROGRAM);
    struct ngl_node *render  = ngl_node_create(NGL_NODE_RENDER);

    ngl_node_param_set(media, "filename", filename);
    ngl_node_param_set(texture, "data_src", media);
    ngl_node_param_set(quad, "corner", corner);
    ngl_node_param_set(quad, "width",  width);
    ngl_node_param_set(quad, "height", height);
    ngl_node_param_set(program, "vertex",   vertex);
    ngl_node_param_set(program, "fragment", fragment);
    ngl_node_param_set(render, "geometry", quad);
    ngl_node_param_set(render, "program", program);
    ngl_node_param_set(render, "textures", "tex0", texture);

    ngl_node_unrefp(&media);
    ngl_node_unrefp(&texture);
    ngl_node_unrefp(&quad);
    ngl_node_unrefp(&program);

    return render;
}
```
**Note**: When a node is referenced by another through parameters, its
reference counter is incremented because the parent holds a reference to its
children.  As a result, you **must release your own references** using
`ngl_node_unrefp()`.

#### Node parameters

Two functions exists to set node parameters:

```c
int ngl_node_param_add(struct ngl_node *node, const char *key,
                       int nb_elems, void *elems);
```

```c
int ngl_node_param_set(struct ngl_node *node, const char *key, ...);
```

The former is to be used for all list-based parameters (`NodeList` or
`doubleList`), and the latter for all the others, even the `NodeDict`. In that
particular case of `NodeDict`, the argument following `key` (identifying the
parameter) is not the value but the key associated with the node (look for
`tex0` in the `get_scene()` example above).

## Drawing

First step is to associate the scene with the `node.gl` context:

```c
    struct ngl_ctx *ctx = ...;
    struct ngl_scene *scene = get_scene(...);

    ngl_set_scene(ctx, scene);
```

Then you are responsible for handling the time yourself and requesting draws
using `ngl_draw()` with the desired time.

Rendering 10 seconds at 60 frames per second (at maximum speed) looks like
this:

```c
    for (int i = 0; i < 60*10; i++) {
        const double t = i / 60.;
        ngl_draw(ctx, t);
    }
```

If you are dealing with real time rendering, the drawing callback needs to be
called at regular interval (graphic system API often comes with a vsync
callback) and use the current time of the reference clock to compute the
drawing time.

Of course, the desired drawing time does not need to be called in a monotonic
manner, any time can be requested. Beware that this may involve heavy
operations such as media seeking, which may cause a delay in the rendering.

## Exit

At the end of the rendering, you need to destroy the scene by unreferencing the
root node and destroying the `node.gl` context:

```c
    ngl_node_unrefp(&scene);
    ngl_freep(&ctx);
```
