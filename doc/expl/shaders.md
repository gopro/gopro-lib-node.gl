# Shaders

In `node.gl`, shaders are written in GLSL, but inputs and outputs are
abstracted by the tree. Similarly, `#version` directive and `precision` are
controlled outside the shader strings. This means that user-provided shaders
usually only contain functions.

Additionally, various helpers are provided to help writing portable shaders,
allowing node trees to be as portable as possible.


## Vertex shader

### Builtin inputs

When constructing a graphic pipeline (`Render` node), the geometry information
needs to be transmitted to the vertex stage. To achieve that, `node.gl`
provides the following inputs to the vertex stage:

Type   | Name           | Description
-------|----------------|------------
`vec3` | `ngl_position` | geometry vertices, always available
`vec2` | `ngl_uvcoord`  | geometry uv coordinates, if provided by the geometry
`vec3` | `ngl_normal`   | geometry normals, if provided by the geometry
`int`  | `ngl_vertex_index`   | index of the current vertex
`int`  | `ngl_instance_index` | instance number of the current primitive in an instanced draw call

**Note**: these are commonly referred as `attributes` or `in` in GL lexicon.

### User inputs

To make accessible more vertex attributes, it is possible to add them in
`Render.attributes` or `Render.instance_attributes`. These attributes will be
set as inputs to the vertex shader. Since this parameter is a dictionary, the
user can access these inputs using the same name used as key parameter.

For example, given the following construct:

```python
center_buffer = ngl.BufferVec3(...)
color_buffer  = ngl.BufferVec4(...)
render = ngl.Render(geometry, program)
render.update_attributes(center=center_buffer, color=color_buffer)
```

The vertex shader will get two extra attributes:

- `center` of type `vec3`
- `color` of type `vec4`

### Builtin variables

The variables are another type of inputs made accessible to the vertex shader.
`node.gl` provides the following as builtin:

Type   | Name                    | Description
-------|-------------------------|------------
`mat4` | `ngl_modelview_matrix`  | modelview matrix
`mat4` | `ngl_projection_matrix` | projection matrix
`mat3` | `ngl_normal_matrix`     | normal matrix

**Note**: these are commonly referred as `uniforms` in GL lexicon.

The symbols will always be available in the vertex shader. The value of these
variables is controlled by a parent `Camera` node.

A typical vertex shader will do the following computation to obtain the
appropriate vertex position: `ngl_projection_matrix * ngl_modelview_matrix *
vec4(ngl_position, 1.0)`.

Similarly, the normal vector is generally obtained using `ngl_normal_matrix *
ngl_normal`.

### User variables

To make accessible more vertex variables, it is possible to add them in
`Render.vert_resources`. Since this parameter is a dictionary, the user can
access these inputs using the same name used as key parameter.

### Output position

To output the vertex position from the vertex shader, `ngl_out_pos` should be
used.

**Note**: this is commonly referred as `gl_Position` in GL lexicon.

### Outputs (to fragment)

To transmit information from the vertex stage to the fragment stage, the
outputs (vertex side) and inputs (fragment side) need to be declared as
`IO*` nodes in `Program.vert_out_vars`.

**Note**: these are commonly referred as `in`/`out` or `varying` in GL lexicon.


## Fragment shader

### Inputs

No builtin inputs are provided by the engine. User variables (see next section)
and vertex outputs declared using `Program.vert_out_vars` are the only
accessible inputs.

### User variables

To make accessible fragment variables, it is possible to add them in
`Render.frag_resources`. Since this parameter is a dictionary, the user can
access these inputs using the same name used as key parameter.

### Output color

To output the color from the fragment shader, `ngl_out_color` should be used.

**Note**: this is commonly referred as `gl_FragColor` in GL lexicon.

In some cases (typically to render to a cube map), the user may want to output
more than one color. The `Program.nb_frag_output` parameter exists for this
purpose.


## Textures

Just like variables, textures are considered *resources* for the different
stages of the pipeline. This means they need to be added to
`Render.vert_resources`, `Render.frag_resources` or `Compute.resources` to be
respectively made accessible to the vertex stage, fragment stage or compute
stage.

Textures need special care in `node.gl`, mainly because of the hardware
acceleration abstraction provided by the engine for multimedia files.

A `Texture2D` can have different types of source set to its `data_src`
parameter: it can be arbitrary user CPU data (typically `Buffer*` nodes) as
well as video frames (using the `Media` node).

To sample a `Texture2D` from within the shader, multiple options are available:

- `ngl_texvideo(name, coords)`: this picking method is safe for any type of
  `data_src`, but it is recommended to only use it with `Media`. The reason for
  this is that it adds a small overhead by checking the image layout in the
  shader: in the case of a media, that format can change dynamically due to
  various video decoding fall-back mechanisms.  On the other hand, it is the
  only way to benefit from video decoding accelerations (external samplers on
  Android, VAAPI on Linux, etc).
- `ngl_tex2d(name, coords)`: this picking method should be used if and only if
  the `data_src` is *not* a `Media`. While it may work sometimes with a
  `Media`, it definitely won't if the video gets accelerated. The only safe way
  to use this picking method with a `Media` is to have
  `Texture2D.direct_rendering` disabled, but that may cause a costly
  intermediate conversion. If the `data_src` points to a user CPU node, then it
  is the recommended method.
- `imageLoad(name, icoords)`: if pixel accurate picking is needed, this method
  can be used. `icoords` needs to be integer coordinates. To use this method,
  it is required to indicate to the program that the texture needs to be
  exposed as an image. To achieve this, a `ResourceProps` node with
  `as_image=True` must be set to the program `properties`, using the name of
  the texture as key. Note that a texture can be accessed as a sampler from a
  program but as an image from another.

Similarly, the `ngl_tex3d()` and `ngl_texcube()` exist respectively for
`Texture3D` and `TextureCube` picking.

Other variables related to textures are exposed to different stages of the shader:

Type   | Name              | Description
-------|-------------------|------------
`mat4` | `%s_coord_matrix` | uv transformation matrix of the texture associated with the render node using key `%s`, it should be applied to the geometry uv coordinates `ngl_uvcoord` to obtain the final texture coordinates. This variable is always exposed in the `vertex` stage.
`vec2` | `%s_dimensions`   | dimensions in pixels of the texture associated with the render node using key `%s`. This variable is exposed in the same stage of the texture.
`float`| `%s_ts`           | timestamp generated by the texture data source, 0.0f for images and buffers, frame timestamp for audios and videos. This variable is exposed in the same stage of the texture.


## Blocks

When using blocks as pipeline resources, their fields can be accessed within
the shaders using `<block>.<field-label>` where `<block>` is the key in the
resource nodes dictionary and `<field-label>` is the label of the field node.

For example, given the following construct:

```python
block = ngl.Block()
block.add_fields([
    UniformFloat(3.0, label='x'),
    BufferVec4(256, label='data'),
])
render = Render(geometry, program)
render.update_frag_resources(blk=block)
```

A block with an instance name `blk` will be declared in the fragment shader, so
the first and second fields respectively be accessed using `blk.x` and
`blk.data`.
