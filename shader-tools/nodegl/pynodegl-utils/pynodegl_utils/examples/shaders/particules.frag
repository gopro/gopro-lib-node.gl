#version 450
precision mediump float;

layout (std140, set = 2, binding = 0) uniform UBO_1 {
	vec4 color;
}; 

layout (location = 0) out vec4 frag_color;

void main(void)
{
    frag_color = color;
}