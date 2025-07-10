#version 460 core

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(push_constant) uniform uPushConstant {
    vec2 scale;
    vec2 translation;
} pc;

layout(location = 0) out struct {
    vec4 color;
    vec2 texCoords;
} outAttributes;

void main () {
    outAttributes.color = aColor;
    outAttributes.texCoords = aUV;

    gl_Position = vec4 (aPosition * pc.scale + pc.translation, 0, 1);
}
