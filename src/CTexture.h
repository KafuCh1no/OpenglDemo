#pragma once

#include <string>

// 2D 纹理
class CTexture
{
public:
	CTexture() = default;
	// isColor：颜色贴图（sRGB）传 true；法线/粗糙度等数据贴图传 false
	explicit CTexture(const std::string& path, bool isColor = true);
	~CTexture();

	// 持有 GL 句柄，禁止拷贝
	CTexture(const CTexture&) = delete;
	CTexture& operator=(const CTexture&) = delete;

	bool Load(const std::string& path, bool isColor = true);
	void Bind(int unit = 0) const;

	unsigned int GetID() const { return m_id; }
	int  GetWidth() const { return m_width; }
	int  GetHeight() const { return m_height; }
	bool IsValid() const { return m_id != 0; }

private:
	void Release();

	unsigned int m_id = 0;
	int m_width = 0;
	int m_height = 0;
};
