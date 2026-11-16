#include "CTexture.h"

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>

CTexture::CTexture(const std::string& path, bool isColor)
{
	Load(path, isColor);
}

CTexture::~CTexture()
{
	Release();
}

void CTexture::Release()
{
	if (m_id)
	{
		glDeleteTextures(1, &m_id);
		m_id = 0;
	}
}

bool CTexture::Load(const std::string& path, bool isColor)
{
	// stb 默认把图上下翻转，OpenGL 的 UV 原点在左下角，需要翻转
	stbi_set_flip_vertically_on_load(true);

	int w = 0, h = 0, channels = 0;
	// 统一转成 4 通道 RGBA
	unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
	if (!data)
	{
		std::cout << "[CTexture] failed to load: " << path
			<< "  reason: " << stbi_failure_reason() << "\n";
		return false;
	}

	Release();
	glGenTextures(1, &m_id);
	glBindTexture(GL_TEXTURE_2D, m_id);

	GLenum internalFormat = isColor ? GL_SRGB8_ALPHA8 : GL_RGBA8;
	// 内部格式用 GL_RGBA8（sRGB 管线可换成 GL_SRGB8_ALPHA8）
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	// 平铺：UV 超过 [0,1] 时重复采样
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(data);

	m_width = w;
	m_height = h;
	return true;
}


void CTexture::Bind(int unit) const
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, m_id);
}
