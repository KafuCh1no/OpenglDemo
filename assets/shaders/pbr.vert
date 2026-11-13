#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 WorldPos;
out vec3 Normal;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    WorldPos = vec3(model * vec4(aPos, 1.0));
    Normal   = mat3(model) * aNormal;   // 球只有均匀缩放，直接用 model 的 3x3 即可
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
