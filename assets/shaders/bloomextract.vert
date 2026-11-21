#version 330 core

out vec2 vUV;

// 全屏三角：gl_VertexID 直接算位置，不传顶点缓冲
//   ID 0 -> (0,0)   ID 1 -> (2,0)   ID 2 -> (0,2)
void main()
{
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));

    vUV         = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
