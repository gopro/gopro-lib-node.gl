#version 450
#extension GL_ARB_separate_shader_objects : enable

//precision highp float;

out gl_PerVertex {
    vec4 gl_Position;
};

/* node.gl */
layout(location = 0) in vec3 ngl_position;
//layout(location = 1) in vec2 ngl_uvcoord;
//layout(location = 2) in vec3 ngl_normal;

layout(push_constant) uniform ngl_block {
    mat4 modelview_matrix;
    mat4 projection_matrix;
    //mat3 normal_matrix;
} ngl;

//uniform mat4 ngl_modelview_matrix;
//uniform mat4 ngl_projection_matrix;
//uniform mat3 ngl_normal_matrix;

//uniform mat4 tex0_coord_matrix;
//uniform vec2 tex0_dimensions;

//layout(location = 0) out vec2 var_uvcoord;
//layout(location = 1) out vec3 var_normal;
//layout(location = 2) out vec2 var_tex0_coord;

/* custom */
layout(location = 3) in vec3 color;
layout(location = 3) out vec3 fragColor;

void main()
{
    gl_Position = ngl.projection_matrix * ngl.modelview_matrix * vec4(ngl_position, 1.0);
    //var_uvcoord = ngl_uvcoord;
    //var_normal = ngl.normal_matrix * ngl_normal;
    //var_tex0_coord = (tex0_coord_matrix * vec4(ngl_uvcoord, 0, 1)).xy;
    fragColor = color;
}
