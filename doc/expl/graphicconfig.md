# Graphic configuration

Color and alpha blending, depth, stencil and culling are controlled by the
`GraphicConfig` node.

`node.gl` is aligned with the traditional blending mode concepts from graphics
APIs. For example, color and alpha blending involves source and destination
factors for color channels or alpha, and a blending operation (addition,
subtraction, etc). This document will assume you are familiar with these
concepts.

For color and alpha blending, you will be particularly interested in the
`GraphicConfig.blend*` parameters. Similarly, depth and stencil configuration
can be respectively found in `GraphicConfig` parameters prefixed by `depth` and
`stencil`.


## Defaults

By default, `node.gl` aligns its defaults with the sane ones from OpenGL:

- Blending: *disabled*
- Blending source factor: `1`
- Blending destination factor: `0`
- Blending source factor alpha: `1`
- Blending destination factor alpha: `0`
- Blending operation: *addition*
- Blending operation alpha: *addition*
- Color write mask: *red*, *green*, *blue*, *alpha*
- Depth test: *disabled*
- Depth write mask: `1`
- Depth Function: *less than*
- Stencil test: *disabled*
- Stencil write mask: `1`
- Stencil function: *always*
- Stencil reference: `0`
- Stencil read mask: `1`
- Stencil fail: *keep*
- Stencil depth fail: *keep*
- Stencil depth pass: *keep*
- Cull mode: *none*


## Inheritance

In a graph, a `GraphicConfig` will inherit the parameters from its parent(s),
or from the previously mentioned defaults if it has none. Every parameter that
is **not set** (or set to `None`/`NULL`) will be **inherited from its
ancestor(s)**.

Sibling branches do not share graphic state except from their common ancestor:
the parenthood is not expressed by the tree visiting sequence at draw time but
by the graph structure itself. Said differently, when the drawing cursor goes
out of a branch (because it has been drawn), it will reset the graphic state to
what it was before entering that branch, and then move into the sibling branch.

While sibling branches do not share the graphic state, it is important to keep
in mind that they do share the same color buffer: the current blending
*destination* represents what was drawn by previous sibling branches, or the
initial frame buffer color if nothing was rendered yet.


## Future directions / improvements

### Grouping settings

While the inheritance mechanism makes sense for categories of settings, it is a
source of confusion within a given scope. For example, while it makes sense to
inherit from the stencil and depth parameters when changing the color blending
(because they are conceptually independent from each other), it doesn't make
much sense to inherit from the alpha blending factors from the ancestors when
setting the color blending factors.

Some solutions involve splitting `GraphicConfig` into several nodes, or have an
internal mechanism such as "if at least one setting of a given group is set,
reset to the defaults the unset settings of that group".


### Undesired inheritance

Even if explicitly set to `None`/`NULL`, the parameters will **not** be reset
to their default values but still inherited from the parent(s). There are
multiple ways we could address this issue:

1. implementing the settings grouping mechanism (as mentioned in the previous
   section), which workarounds the issue by removing the need for a reset
2. adding specific `reset` constants to `GraphicConfig` parameters
3. introducing a dedicated node with the reset-to-defaults purpose

### Blocking down inheritance automatically

In some circumstances, it may be relevant to automatically reset the graphic
state in a sub-tree. The main contender here would be the `RenderToTexture`
node.
