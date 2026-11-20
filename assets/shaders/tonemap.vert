#version 330 core

out vec2 vUV;

// 全屏三角：不依赖顶点缓冲，直接用 gl_VertexID 算位置。
//   ID 0 -> (0,0)   ID 1 -> (2,0)   ID 2 -> (0,2)
// 三个点连成一个把屏幕整个盖住的大三角形，比两个三角形少一条对角线接缝。
void main()
{
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));

    vUV         = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
