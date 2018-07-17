#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform Parameters {
    vec4 color0;
} parameters;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = parameters.color0;
}
