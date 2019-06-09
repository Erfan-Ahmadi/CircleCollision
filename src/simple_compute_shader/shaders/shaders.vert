#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject
{
	mat4 view;
	mat4 proj;
} ubo;

layout(location = 0) in vec2	inPos;
layout(location = 1) in vec2	inInstancePos;
layout(location = 2) in vec3	inInstanceColor;
layout(location = 3) in float	inInstanceScale;

layout(location = 0) out vec3 fragColor;

void main()
{
	gl_Position = ubo.proj * ubo.view * vec4(inPos * inInstanceScale + inInstancePos, 0.0, 1.0);
	fragColor = inInstanceColor;
}