#include "CShader.h"

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

CShader::CShader(const std::string& vertexPath, const std::string& fragmentPath)
{
    // 1. 读文件
    std::ifstream vFile(vertexPath);
    std::ifstream fFile(fragmentPath);
    if (!vFile.is_open() || !fFile.is_open()) {
        std::cout << "[CShader] failed to open shader file: "
                  << vertexPath << " or " << fragmentPath << "\n";
        return;
    }
    std::stringstream vStream, fStream;
    vStream << vFile.rdbuf();
    fStream << fFile.rdbuf();
    std::string vCode = vStream.str();
    std::string fCode = fStream.str();
    const char* vSrc = vCode.c_str();
    const char* fSrc = fCode.c_str();

    // 2. 编译
    unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vSrc, nullptr);
    glCompileShader(vertex);
    checkCompileErrors(vertex, "VERTEX");

    unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fSrc, nullptr);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");

    // 3. 链接
    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

void CShader::use() const
{
    glUseProgram(ID);
}

void CShader::setBool(const std::string& name, bool value) const
{
    glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}

void CShader::setInt(const std::string& name, int value) const
{
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void CShader::setFloat(const std::string& name, float value) const
{
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void CShader::setVec3(const std::string& name, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
}

void CShader::setMat4(const std::string& name, const glm::mat4& mat) const
{
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}

void CShader::setVec3Array(const std::string& name, const glm::vec3* data, int count) const
{
    if (count <= 0) return;
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), count, glm::value_ptr(*data));
}

void CShader::setFloatArray(const std::string& name, const float* data, int count) const
{
    if (count <= 0) return;
    glUniform1fv(glGetUniformLocation(ID, name.c_str()), count, data);
}

void CShader::setIntArray(const std::string& name, const int* data, int count) const
{
    if (count <= 0) return;
    glUniform1iv(glGetUniformLocation(ID, name.c_str()), count, data);
}

void CShader::checkCompileErrors(unsigned int shader, const std::string& type)
{
    int success;
    char infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            std::cout << "[CShader] compile error (" << type << "):\n" << infoLog << "\n";
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
            std::cout << "[CShader] link error:\n" << infoLog << "\n";
        }
    }
}
