#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>

// 着色器封装：读文件 -> 编译 -> 链接 -> 设置 uniform
class CShader
{
public:
    unsigned int ID = 0;

    CShader(const std::string& vertexPath, const std::string& fragmentPath);

    void use() const;

    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;

    // ---- 数组版：name 要带下标，如 "lightPos[0]"，一次填满整个数组 ----
    void setVec3Array(const std::string& name, const glm::vec3* data, int count) const;
    void setFloatArray(const std::string& name, const float* data, int count) const;
    void setIntArray(const std::string& name, const int* data, int count) const;
    void setMat4Array(const std::string& name, const glm::mat4* data, int count) const;

private:
    void checkCompileErrors(unsigned int shader, const std::string& type);
};
