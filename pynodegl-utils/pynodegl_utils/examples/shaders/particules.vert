precision highp float;

in vec4 ngl_position;
uniform mat4 ngl_modelview_matrix;
uniform mat4 ngl_projection_matrix;

layout(std430, binding = 0) buffer positions_buffer {
    vec3 positions[];
};

void main()
{
    vec4 position = ngl_position + vec4(positions[gl_InstanceID], 0.0);
    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * position;
}
