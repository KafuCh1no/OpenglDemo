#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 shadowMatrix;      // 当前面的 proj * view
uniform mat4 model;             // 局部 -> 世界

out vec3 vWorldPos;

void main()
{
    vec4 w = model * vec4(aPos, 1.0);
    vWorldPos   = w.xyz;        // frag 里要用它算到光源的距离
    gl_Position = shadowMatrix * w;
}
