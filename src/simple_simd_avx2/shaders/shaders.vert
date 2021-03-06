#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject
{
	mat4 view;
	mat4 proj;
} ubo;

layout(location = 0) in vec2	inPos;
layout(location = 1) in float	inInstanceXPos;
layout(location = 2) in float	inInstanceYPos;
layout(location = 3) in vec3	inInstanceColor;
layout(location = 4) in float	inInstanceScale;

layout(location = 0) out vec3 fragColor;

void main()
{
	gl_Position = ubo.proj * ubo.view * vec4(inPos * inInstanceScale + vec2(inInstanceXPos, inInstanceYPos), 0.0, 1.0);
	fragColor = inInstanceColor;
}