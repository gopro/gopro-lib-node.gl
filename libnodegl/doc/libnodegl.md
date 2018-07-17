libnodegl
=========

## AnimatedBuffer*

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | [`NodeList`](#parameter-types) ([AnimKeyFrameBuffer](#animkeyframebuffer)) | key frame buffers to interpolate from | 


**Source**: [node_animatedbuffer.c](/libnodegl/node_animatedbuffer.c)

List of `AnimatedBuffer*` nodes:

- `AnimatedBufferFloat`
- `AnimatedBufferVec2`
- `AnimatedBufferVec3`
- `AnimatedBufferVec4`

## AnimatedFloat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | [`NodeList`](#parameter-types) ([AnimKeyFrameFloat](#animkeyframefloat)) | float key frames to interpolate from | 


**Source**: [node_animation.c](/libnodegl/node_animation.c)


## AnimatedVec2

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | [`NodeList`](#parameter-types) ([AnimKeyFrameVec2](#animkeyframevec2)) | vec2 key frames to interpolate from | 


**Source**: [node_animation.c](/libnodegl/node_animation.c)


## AnimatedVec3

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | [`NodeList`](#parameter-types) ([AnimKeyFrameVec3](#animkeyframevec3)) | vec3 key frames to interpolate from | 


**Source**: [node_animation.c](/libnodegl/node_animation.c)


## AnimatedVec4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | [`NodeList`](#parameter-types) ([AnimKeyFrameVec4](#animkeyframevec4)) | vec4 key frames to interpolate from | 


**Source**: [node_animation.c](/libnodegl/node_animation.c)


## AnimatedQuat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | [`NodeList`](#parameter-types) ([AnimKeyFrameQuat](#animkeyframequat)) | quaternion key frames to interpolate from | 


**Source**: [node_animation.c](/libnodegl/node_animation.c)


## AnimKeyFrameFloat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | [`double`](#parameter-types) | the time key point in seconds | `0`
`value` | ✓ | [`double`](#parameter-types) | the value at time `time` | `0`
`easing` |  | [`easing`](#easing-choices) | easing interpolation | `linear`
`easing_args` |  | [`doubleList`](#parameter-types) | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameVec2

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | [`double`](#parameter-types) | the time key point in seconds | `0`
`value` | ✓ | [`vec2`](#parameter-types) | the value at time `time` | (`0`,`0`)
`easing` |  | [`easing`](#easing-choices) | easing interpolation | `linear`
`easing_args` |  | [`doubleList`](#parameter-types) | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameVec3

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | [`double`](#parameter-types) | the time key point in seconds | `0`
`value` | ✓ | [`vec3`](#parameter-types) | the value at time `time` | (`0`,`0`,`0`)
`easing` |  | [`easing`](#easing-choices) | easing interpolation | `linear`
`easing_args` |  | [`doubleList`](#parameter-types) | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameVec4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | [`double`](#parameter-types) | the time key point in seconds | `0`
`value` | ✓ | [`vec4`](#parameter-types) | the value at time `time` | (`0`,`0`,`0`,`0`)
`easing` |  | [`easing`](#easing-choices) | easing interpolation | `linear`
`easing_args` |  | [`doubleList`](#parameter-types) | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameQuat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | [`double`](#parameter-types) | the time key point in seconds | `0`
`quat` | ✓ | [`vec4`](#parameter-types) | the quat at time `time` | (`0`,`0`,`0`,`0`)
`easing` |  | [`easing`](#easing-choices) | easing interpolation | `linear`
`easing_args` |  | [`doubleList`](#parameter-types) | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameBuffer

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | [`double`](#parameter-types) | the time key point in seconds | `0`
`data` |  | [`data`](#parameter-types) | the data at time `time` | 
`easing` |  | [`easing`](#easing-choices) | easing interpolation | `linear`
`easing_args` |  | [`doubleList`](#parameter-types) | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## Buffer*

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`count` |  | [`int`](#parameter-types) | number of elements | `0`
`data` |  | [`data`](#parameter-types) | buffer of `count` elements | 
`filename` |  | [`string`](#parameter-types) | filename from which the buffer will be read, cannot be used with `data` | 
`stride` |  | [`int`](#parameter-types) | stride of 1 element, in bytes | `0`


**Source**: [node_buffer.c](/libnodegl/node_buffer.c)

List of `Buffer*` nodes:

- `BufferByte`
- `BufferBVec2`
- `BufferBVec3`
- `BufferBVec4`
- `BufferInt`
- `BufferIVec2`
- `BufferIVec3`
- `BufferIVec4`
- `BufferShort`
- `BufferSVec2`
- `BufferSVec3`
- `BufferSVec4`
- `BufferUByte`
- `BufferUBVec2`
- `BufferUBVec3`
- `BufferUBVec4`
- `BufferUInt`
- `BufferUIVec2`
- `BufferUIVec3`
- `BufferUIVec4`
- `BufferUShort`
- `BufferUSVec2`
- `BufferUSVec3`
- `BufferUSVec4`
- `BufferFloat`
- `BufferVec2`
- `BufferVec3`
- `BufferVec4`

## Camera

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | scene to observe through the lens of the camera | 
`eye` |  | [`vec3`](#parameter-types) | eye position | (`0`,`0`,`0`)
`center` |  | [`vec3`](#parameter-types) | center position | (`0`,`0`,`-1`)
`up` |  | [`vec3`](#parameter-types) | up vector | (`0`,`1`,`0`)
`perspective` |  | [`vec4`](#parameter-types) | the 4 following values: *fov*, *aspect*, *near clipping plane*, *far clipping plane* | (`0`,`0`,`0`,`0`)
`eye_transform` |  | [`Node`](#parameter-types) ([Rotate](#rotate), [Transform](#transform), [Translate](#translate), [Scale](#scale)) | `eye` transformation chain | 
`center_transform` |  | [`Node`](#parameter-types) ([Rotate](#rotate), [Transform](#transform), [Translate](#translate), [Scale](#scale)) | `center` transformation chain | 
`up_transform` |  | [`Node`](#parameter-types) ([Rotate](#rotate), [Transform](#transform), [Translate](#translate), [Scale](#scale)) | `up` transformation chain | 
`fov_anim` |  | [`Node`](#parameter-types) ([AnimatedFloat](#animatedfloat)) | field of view animation (first field of `perspective`) | 
`pipe_fd` |  | [`int`](#parameter-types) | pipe file descriptor where the rendered raw RGBA buffer is written | `0`
`pipe_width` |  | [`int`](#parameter-types) | width (in pixels) of the raw image buffer when using `pipe_fd` | `0`
`pipe_height` |  | [`int`](#parameter-types) | height (in pixels) of the raw image buffer when using `pipe_fd` | `0`
`hflip` |  | [`bool`](#parameter-types) | horizontal flip | `unset`


**Source**: [node_camera.c](/libnodegl/node_camera.c)


## Circle

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`radius` |  | [`double`](#parameter-types) | circle radius | `1`
`npoints` |  | [`int`](#parameter-types) | number of points | `16`


**Source**: [node_circle.c](/libnodegl/node_circle.c)


## Compute

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`nb_group_x` | ✓ | [`int`](#parameter-types) | number of work groups to be executed in the x dimension | `0`
`nb_group_y` | ✓ | [`int`](#parameter-types) | number of work groups to be executed in the y dimension | `0`
`nb_group_z` | ✓ | [`int`](#parameter-types) | number of work groups to be executed in the z dimension | `0`
`program` | ✓ | [`Node`](#parameter-types) ([ComputeProgram](#computeprogram)) | compute program to be executed | 
`textures` |  | [`NodeDict`](#parameter-types) ([Texture2D](#texture2d)) | input and output textures made accessible to the compute `program` | 
`uniforms` |  | [`NodeDict`](#parameter-types) ([UniformFloat](#uniformfloat), [UniformVec2](#uniformvec2), [UniformVec3](#uniformvec3), [UniformVec4](#uniformvec4), [UniformQuat](#uniformquat), [UniformInt](#uniformint), [UniformMat4](#uniformmat4)) | uniforms made accessible to the compute `program` | 
`buffers` |  | [`NodeDict`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec3](#buffer), [BufferIVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec3](#buffer), [BufferUIVec4](#buffer)) | input and output buffers made accessible to the compute `program` | 


**Source**: [node_compute.c](/libnodegl/node_compute.c)


## ComputeProgram

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`compute` | ✓ | [`string`](#parameter-types) | compute shader | 


**Source**: [node_computeprogram.c](/libnodegl/node_computeprogram.c)


## Geometry

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`vertices` | ✓ | [`Node`](#parameter-types) ([BufferVec3](#buffer), [AnimatedBufferVec3](#animatedbuffer)) | vertice coordinates defining the geometry | 
`uvcoords` |  | [`Node`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec3](#animatedbuffer)) | coordinates used for UV mapping of each `vertices` | 
`normals` |  | [`Node`](#parameter-types) ([BufferVec3](#buffer), [AnimatedBufferVec3](#animatedbuffer)) | normal vectors of each `vertices` | 
`indices` |  | [`Node`](#parameter-types) ([BufferUByte](#buffer), [BufferUInt](#buffer), [BufferUShort](#buffer)) | indices defining the drawing order of the `vertices`, auto-generated if not set | 
`draw_mode` |  | [`draw_mode`](#draw_mode-choices) | drawing mode | `triangles`


**Source**: [node_geometry.c](/libnodegl/node_geometry.c)


## GraphicConfig

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | scene to which the graphic configuration will be applied | 
`blend` |  | [`bool`](#parameter-types) | enable blending | `unset`
`blend_src_factor` |  | [`blend_factor`](#blend_factor-choices) | blend source factor | `unset`
`blend_dst_factor` |  | [`blend_factor`](#blend_factor-choices) | blend destination factor | `unset`
`blend_src_factor_a` |  | [`blend_factor`](#blend_factor-choices) | alpha blend source factor | `unset`
`blend_dst_factor_a` |  | [`blend_factor`](#blend_factor-choices) | alpha blend destination factor | `unset`
`blend_op` |  | [`blend_operation`](#blend_operation-choices) | blend operation | `unset`
`blend_op_a` |  | [`blend_operation`](#blend_operation-choices) | alpha blend operation | `unset`
`color_write_mask` |  | [`component`](#component-choices) | color write mask | `unset`
`depth_test` |  | [`bool`](#parameter-types) | enable depth testing | `unset`
`depth_write_mask` |  | [`bool`](#parameter-types) | depth write mask | `unset`
`depth_func` |  | [`function`](#function-choices) | passes if `<function>(depth, stored_depth)` | `unset`
`stencil_test` |  | [`bool`](#parameter-types) | enable stencil testing | `unset`
`stencil_write_mask` |  | [`int`](#parameter-types) | stencil write mask | `-1`
`stencil_func` |  | [`function`](#function-choices) | passes if `<function>(stencil_ref & stencil_read_mask, stencil & stencil_read_mask)` | `unset`
`stencil_ref` |  | [`int`](#parameter-types) | stencil reference value to compare against | `-1`
`stencil_read_mask` |  | [`int`](#parameter-types) | stencil read mask | `-1`
`stencil_fail` |  | [`stencil_operation`](#stencil_operation-choices) | operation to execute if stencil test fails | `unset`
`stencil_depth_fail` |  | [`stencil_operation`](#stencil_operation-choices) | operation to execute if depth test fails | `unset`
`stencil_depth_pass` |  | [`stencil_operation`](#stencil_operation-choices) | operation to execute if stencil and depth test pass | `unset`


**Source**: [node_graphicconfig.c](/libnodegl/node_graphicconfig.c)


## Group

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`children` |  | [`NodeList`](#parameter-types) | a set of scenes | 


**Source**: [node_group.c](/libnodegl/node_group.c)


## HUD

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | scene to benchmark | 
`measure_window` |  | [`int`](#parameter-types) | window size for measures | `60`
`refresh_rate` |  | [`rational`](#parameter-types) | refresh data buffer every `update_rate` second | 
`export_filename` |  | [`string`](#parameter-types) | path to export file (CSV) | 
`bg_color` |  | [`vec4`](#parameter-types) | Background buffer color | (`0`,`0`,`0`,`1`)


**Source**: [node_hud.c](/libnodegl/node_hud.c)


## Identity

**Source**: [node_identity.c](/libnodegl/node_identity.c)


## Media

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`filename` | ✓ | [`string`](#parameter-types) | path to input media file | 
`sxplayer_min_level` |  | [`string`](#parameter-types) | sxplayer min logging level | 
`time_anim` |  | [`Node`](#parameter-types) ([AnimatedFloat](#animatedfloat)) | time remapping animation (must use a `linear` interpolation) | 
`audio_tex` |  | [`bool`](#parameter-types) | load the audio and expose it as a stereo waves and frequencies buffer | `0`
`max_nb_packets` |  | [`int`](#parameter-types) | maximum number of packets in sxplayer demuxing queue | `1`
`max_nb_frames` |  | [`int`](#parameter-types) | maximum number of frames in sxplayer decoding queue | `1`
`max_nb_sink` |  | [`int`](#parameter-types) | maximum number of frames in sxplayer filtering queue | `1`
`max_pixels` |  | [`int`](#parameter-types) | maximum number of pixels per frame | `0`


**Source**: [node_media.c](/libnodegl/node_media.c)


## Program

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`vertex` |  | [`data`](#parameter-types) | vertex SPIR-V shader | 
`fragment` |  | [`data`](#parameter-types) | fragment SPIR-V shader | 


**Source**: [node_program.c](/libnodegl/node_program.c)


## Quad

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`corner` |  | [`vec3`](#parameter-types) | origin coordinates of `width` and `height` vectors | (`-0.5`,`-0.5`,`0`)
`width` |  | [`vec3`](#parameter-types) | width vector | (`1`,`0`,`0`)
`height` |  | [`vec3`](#parameter-types) | height vector | (`0`,`1`,`0`)
`uv_corner` |  | [`vec2`](#parameter-types) | origin coordinates of `uv_width` and `uv_height` vectors | (`0`,`0`)
`uv_width` |  | [`vec2`](#parameter-types) | UV coordinates width vector | (`1`,`0`)
`uv_height` |  | [`vec2`](#parameter-types) | UV coordinates height vector | (`0`,`1`)


**Source**: [node_quad.c](/libnodegl/node_quad.c)


## Render

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`geometry` | ✓ | [`Node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) | geometry to be rasterized | 
`program` |  | [`Node`](#parameter-types) ([Program](#program)) | program to be executed | 
`textures` |  | [`NodeDict`](#parameter-types) ([Texture2D](#texture2d), [Texture3D](#texture3d)) | textures made accessible to the `program` | 
`uniforms` |  | [`NodeDict`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [UniformFloat](#uniformfloat), [UniformVec2](#uniformvec2), [UniformVec3](#uniformvec3), [UniformVec4](#uniformvec4), [UniformQuat](#uniformquat), [UniformInt](#uniformint), [UniformMat4](#uniformmat4)) | uniforms made accessible to the `program` | 
`attributes` |  | [`NodeDict`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer)) | extra vertex attributes made accessible to the `program` | 
`buffers` |  | [`NodeDict`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec3](#buffer), [BufferIVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec3](#buffer), [BufferUIVec4](#buffer)) | buffers made accessible to the `program` | 


**Source**: [node_render.c](/libnodegl/node_render.c)


## RenderToTexture

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | scene to be rasterized to `color_texture` and optionally to `depth_texture` | 
`color_texture` | ✓ | [`Node`](#parameter-types) ([Texture2D](#texture2d)) | destination color texture | 
`depth_texture` |  | [`Node`](#parameter-types) ([Texture2D](#texture2d)) | destination depth (and potentially combined stencil) texture | 
`samples` |  | [`int`](#parameter-types) | number of samples used for multisampling anti-aliasing | `0`


**Source**: [node_rtt.c](/libnodegl/node_rtt.c)


## Rotate

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | scene to rotate | 
`angle` |  | [`double`](#parameter-types) | rotation angle in degrees | `0`
`axis` |  | [`vec3`](#parameter-types) | rotation axis | (`0`,`0`,`1`)
`anchor` |  | [`vec3`](#parameter-types) | vector to the center point of the rotation | (`0`,`0`,`0`)
`anim` |  | [`Node`](#parameter-types) ([AnimatedFloat](#animatedfloat)) | `angle` animation | 


**Source**: [node_rotate.c](/libnodegl/node_rotate.c)


## Scale

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | scene to scale | 
`factors` |  | [`vec3`](#parameter-types) | scaling factors (how much to scale on each axis) | (`0`,`0`,`0`)
`anchor` |  | [`vec3`](#parameter-types) | vector to the center point of the scale | (`0`,`0`,`0`)
`anim` |  | [`Node`](#parameter-types) ([AnimatedVec3](#animatedvec3)) | `factors` animation | 


**Source**: [node_scale.c](/libnodegl/node_scale.c)


## Texture2D

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`wrap_s` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the s dimension (horizontal) | `clamp_to_edge`
`wrap_t` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the t dimension (vertical) | `clamp_to_edge`
`direct_rendering` |  | [`bool`](#parameter-types) | whether direct rendering is enabled or not for media playback | `unset`
`immutable` |  | [`bool`](#parameter-types) | whether the texture is immutable or not | `0`


**Source**: [node_texture.c](/libnodegl/node_texture.c)


## Texture3D

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`wrap_s` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the s dimension (horizontal) | `clamp_to_edge`
`wrap_t` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the t dimension (vertical) | `clamp_to_edge`
`wrap_r` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the r dimension (depth) | `clamp_to_edge`
`immutable` |  | [`bool`](#parameter-types) | whether the texture is immutable or not | `0`


**Source**: [node_texture.c](/libnodegl/node_texture.c)


## TimeRangeFilter

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | time filtered scene | 
`ranges` |  | [`NodeList`](#parameter-types) ([TimeRangeModeOnce](#timerangemodeonce), [TimeRangeModeNoop](#timerangemodenoop), [TimeRangeModeCont](#timerangemodecont)) | key frame time filtering events | 
`prefetch_time` |  | [`double`](#parameter-types) | `child` is prefetched `prefetch_time` seconds in advance | `1`
`max_idle_time` |  | [`double`](#parameter-types) | `child` will not be released if it is required in the next incoming `max_idle_time` seconds | `4`


**Source**: [node_timerangefilter.c](/libnodegl/node_timerangefilter.c)


## TimeRangeModeCont

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`start_time` | ✓ | [`double`](#parameter-types) | starting time for the scene to be drawn | `0`


**Source**: [node_timerangemodes.c](/libnodegl/node_timerangemodes.c)


## TimeRangeModeNoop

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`start_time` | ✓ | [`double`](#parameter-types) | starting time for the scene to stop being drawn | `0`


**Source**: [node_timerangemodes.c](/libnodegl/node_timerangemodes.c)


## TimeRangeModeOnce

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`start_time` | ✓ | [`double`](#parameter-types) | starting time for the scene to be drawn once | `0`
`render_time` | ✓ | [`double`](#parameter-types) | chosen time to draw | `0`


**Source**: [node_timerangemodes.c](/libnodegl/node_timerangemodes.c)


## Transform

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | scene to apply the transform to | 
`matrix` |  | [`mat4`](#parameter-types) | transformation matrix | 


**Source**: [node_transform.c](/libnodegl/node_transform.c)


## Translate

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | scene to translate | 
`vector` |  | [`vec3`](#parameter-types) | translation vector | (`0`,`0`,`0`)
`anim` |  | [`Node`](#parameter-types) ([AnimatedVec3](#animatedvec3)) | `vector` animation | 


**Source**: [node_translate.c](/libnodegl/node_translate.c)


## Triangle

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`edge0` | ✓ | [`vec3`](#parameter-types) | first edge coordinate of the triangle | (`0`,`0`,`0`)
`edge1` | ✓ | [`vec3`](#parameter-types) | second edge coordinate of the triangle | (`0`,`0`,`0`)
`edge2` | ✓ | [`vec3`](#parameter-types) | third edge coordinate of the triangle | (`0`,`0`,`0`)
`uv_edge0` |  | [`vec2`](#parameter-types) | UV coordinate associated with `edge0` | (`0`,`0`)
`uv_edge1` |  | [`vec2`](#parameter-types) | UV coordinate associated with `edge1` | (`0`,`1`)
`uv_edge2` |  | [`vec2`](#parameter-types) | UV coordinate associated with `edge2` | (`1`,`1`)


**Source**: [node_triangle.c](/libnodegl/node_triangle.c)


## UniformInt

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | [`int`](#parameter-types) | value exposed to the shader | `0`


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformMat4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | [`mat4`](#parameter-types) | value exposed to the shader | 
`transform` |  | [`Node`](#parameter-types) ([Rotate](#rotate), [Transform](#transform), [Translate](#translate), [Scale](#scale)) | `value` transformation chain | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformFloat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | [`double`](#parameter-types) | value exposed to the shader | `0`
`anim` |  | [`Node`](#parameter-types) ([AnimatedFloat](#animatedfloat)) | `value` animation | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformVec2

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | [`vec2`](#parameter-types) | value exposed to the shader | (`0`,`0`)
`anim` |  | [`Node`](#parameter-types) ([AnimatedVec2](#animatedvec2)) | `value` animation | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformVec3

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | [`vec3`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`)
`anim` |  | [`Node`](#parameter-types) ([AnimatedVec3](#animatedvec3)) | `value` animation | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformVec4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | [`vec4`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`,`0`)
`anim` |  | [`Node`](#parameter-types) ([AnimatedVec4](#animatedvec4)) | `value` animation | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformQuat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | [`vec4`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`,`0`)
`anim` |  | [`Node`](#parameter-types) ([AnimatedQuat](#animatedquat)) | `value` animation | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)

Parameter types
===============

Type | Description
---- | -----------
`int` | Integer
`bool` | Boolean (map to `int` in C)
`i64` | 64-bit integer
`double` | Double-precision float
`string` | String
`data` | Agnostic data buffer
`vec2` | 2 single-precision floats
`vec3` | 3 single-precision floats
`vec4` | 4 single-precision floats
`mat4` | 4x4 single-precision floats
`Node` | node.gl Node
`NodeList` | List of node.gl Node
`doubleList` | List of double-precision floats
`NodeDict` | Dictionary mapping arbitrary string identifiers to node.gl Nodes
`select` | Selection of one constant (expressed as a string)
`flags` | Combination of constants (expressed as strings), using `+` as separator. Can be empty for none.
`rational` | Rational number (expressed as 2 integers, respectively as numerator and denominator)

Constants for choices parameters
================================

## easing choices

Constant | Description
-------- | -----------
`linear` | `linear(x)=x`
`quadratic_in` | `quadratic(x)=x²`
`quadratic_out` | `quadratic_out(x)=1-quadratic(1-x)`
`quadratic_in_out` | `quadratic_in_out(x)=quadratic(2x)/2` if `x<½` else `1-quadratic(2*(1-x))/2`
`quadratic_out_in` | `quadratic_out_in(x)=(1-quadratic(1-2x))/2` if `x<½` else `(1+quadratic(2x-1))/2`
`cubic_in` | `cubic(x)=x³`
`cubic_out` | `cubic_out(x)=1-cubic(1-x)`
`cubic_in_out` | `cubic_in_out(x)=cubic(2x)/2` if `x<½` else `1-cubic(2*(1-x))/2`
`cubic_out_in` | `cubic_out_in(x)=(1-cubic(1-2x))/2` if `x<½` else `(1+cubic(2x-1))/2`
`quartic_in` | `quartic(x)=x⁴`
`quartic_out` | `quartic_out(x)=1-quartic(1-x)`
`quartic_in_out` | `quartic_in_out(x)=quartic(2x)/2` if `x<½` else `1-quartic(2*(1-x))/2`
`quartic_out_in` | `quartic_out_in(x)=(1-quartic(1-2x))/2` if `x<½` else `(1+quartic(2x-1))/2`
`quintic_in` | `quintic(x)=x⁵`
`quintic_out` | `quintic(x)=1-quintic(1-x)`
`quintic_in_out` | `quintic_in_out(x)=quintic(2x)/2` if `x<½` else `1-quintic(2*(1-x))/2`
`quintic_out_in` | `quintic_out_in(x)=(1-quintic(1-2x))/2` if `x<½` else `(1+quintic(2x-1))/2`
`sinus_in` | `sinus(x)=1-cos(x*π/2)`
`sinus_out` | `sinus_out(x)=1-sinus(1-x)`
`sinus_in_out` | `sinus_in_out(x)=sinus(2x)/2` if `x<½` else `1-sinus(2*(1-x))/2`
`sinus_out_in` | `sinus_out_in(x)=(1-sinus(1-2x))/2` if `x<½` else `(1+sinus(2x-1))/2`
`exp_in` | `exp(x,a=1024)=(pow(a,x)-1)/(a-1)`
`exp_out` | `exp_out(x,a=1024)=1-exp(1-x,a)`
`exp_in_out` | `exp_in_out(x,a=1024)=exp(2x,a)/2` if `x<½` else `1-exp(2*(1-x),a)/2`
`exp_out_in` | `exp_out_in(x,a=1024)=(1-exp(1-2x,a))/2` if `x<½` else `(1+exp(2x-1,a))/2`
`circular_in` | `circular(x)=1-√(1-x²)`
`circular_out` | `circular_out(x)=1-circular(1-x)`
`circular_in_out` | `circular_in_out(x)=circular(2x)/2` if `x<½` else `1-circular(2*(1-x))/2`
`circular_out_in` | `circular_out_in(x)=(1-circular(1-2x))/2` if `x<½` else `(1+circular(2x-1))/2`
`bounce_in` | bouncing from right to left 4 times
`bounce_out` | diagonally mirrored version of `bounce_in()`
`elastic_in` | elastic effect from weak to strong
`elastic_out` | mirrored `elastic_in` effect
`back_in` | mirrored `back_out` effect
`back_out` | overstep target value and smoothly converge back to it
`back_in_out` | combination of `back_in` then `back_out`
`back_out_in` | combination of `back_out` then `back_in`

## draw_mode choices

Constant | Description
-------- | -----------
`points` | points
`line_strip` | line strip
`lines` | lines
`triangle_strip` | triangle strip
`triangle_fan` | triangle fan
`triangles` | triangles

## blend_factor choices

Constant | Description
-------- | -----------
`unset` | unset
`zero` | `0`
`one` | `1`
`src_color` | `src_color`
`one_minus_src_color` | `1 - src_color`
`dst_color` | `dst_color`
`one_minus_dst_color` | `1 - dst_color`
`src_alpha` | `src_alpha`
`one_minus_src_alpha` | `1 - src_alpha`
`dst_alpha` | `dst_alpha`
`one_minus_dst_alpha` | `1 - dst_alpha`

## blend_operation choices

Constant | Description
-------- | -----------
`unset` | unset
`add` | `src + dst`
`sub` | `src - dst`
`revsub` | `dst - src`
`min` | `min(src, dst)`
`max` | `max(src, dst)`

## component choices

Constant | Description
-------- | -----------
`r` | red
`g` | green
`b` | blue
`a` | alpha

## function choices

Constant | Description
-------- | -----------
`unset` | unset
`never` | `f(a,b) = 0`
`less` | `f(a,b) = a < b`
`equal` | `f(a,b) = a == b`
`lequal` | `f(a,b) = a ≤ b`
`greater` | `f(a,b) = a > b`
`notequal` | `f(a,b) = a ≠ b`
`gequal` | `f(a,b) = a ≥ b`
`always` | `f(a,b) = 1`

## stencil_operation choices

Constant | Description
-------- | -----------
`unset` | unset
`keep` | keeps the current value
`zero` | sets the stencil buffer value to 0
`replace` | sets the stencil buffer value to ref, as specified by the stencil function
`incr` | increments the current stencil buffer value and clamps it
`incr_wrap` | increments the current stencil buffer value and wraps it
`decr` | decrements the current stencil buffer value and clamps it
`decr_wrap` | decrements the current stencil buffer value and wraps it
`decr_invert` | bitwise inverts the current stencil buffer value

## wrap choices

Constant | Description
-------- | -----------
`clamp_to_edge` | clamp to edge wrapping
`mirrored_repeat` | mirrored repeat wrapping
`repeat` | repeat pattern wrapping
