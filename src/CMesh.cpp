#include "CMesh.h"

#include <glad/glad.h>

CMesh::~CMesh()
{
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

void CMesh::Upload(const std::vector<float>& vertices,
    const std::vector<unsigned int>& indices)
{
    if (m_vao == 0)
    {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);
    }

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
        vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
        indices.data(), GL_STATIC_DRAW);

    // 顶点布局：pos3 | normal3 | color3 | uv2  = 11 个 float = 44 字节
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);                    // pos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));  // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));  // color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(9 * sizeof(float)));  // uv
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    m_indexCount = (unsigned int)indices.size();   // ★ 记住
}

void CMesh::Draw() const
{
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)m_indexCount, GL_UNSIGNED_INT, 0);
}
