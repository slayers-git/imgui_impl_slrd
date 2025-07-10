#version 460 core

layout(binding = 0) uniform sampler2D uTexture;

layout(location = 0) out vec4 outColor;
layout(location = 0) in struct {
    vec4 color;
    vec2 texCoords;
} iAttributes;

void main () {
    outColor = iAttributes.color * texture (uTexture,
        iAttributes.texCoords);
}
