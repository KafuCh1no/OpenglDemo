#version 330 core

out vec2 vUV;

// 全屏三角，和 tonemap.vert 同一套写法：不用顶点缓冲
void main()
{
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));

    vUV         = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
