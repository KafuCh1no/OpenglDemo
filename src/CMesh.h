#pragma once

#include <vector>

class CMesh
{
public:
    CMesh() = default;
    ~CMesh();

    CMesh(const CMesh&) = delete;
    CMesh& operator=(const CMesh&) = delete;

    void Upload(const std::vector<float>& vertices,
        const std::vector<unsigned int>& indices);
    void Draw() const;

private:
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    unsigned int m_ebo = 0;
    unsigned int m_indexCount = 0;
};