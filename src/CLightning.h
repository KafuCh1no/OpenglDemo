#pragma once

#include <vector>

// 月光闪电：不新增光源、不新增阴影 Pass，
// 只是把平行光的 intensity 按随机时间轴放大成一阵阵闪光。
class CLightning
{
public:
	// 每帧调用，dt 单位秒；内部维护"下一次闪电"的倒计时
	void Update(float dt);

	// 手动触发一次（调试 / 玩法用）
	void Trigger();

	// 当前归一化闪光强度 0..1
	float Flash() const { return m_flash; }

	// 月光亮度倍率：平时 1.0，闪电峰值 1.0 + gain
	float MoonMultiplier() const { return 1.0f + m_flash * gain; }

	bool  enabled     = true;
	float gain        = 8.0f;    // 闪光峰值 = 月光亮度 × (1 + gain)
	float minInterval = 4.0f;    // 两次闪电之间的最短间隔（秒）
	float maxInterval = 14.0f;   // 最长间隔（秒）

private:
	struct Pulse
	{
		float start    = 0.0f;   // 相对于这阵闪电开始的偏移
		float duration = 0.0f;
		float amp      = 1.0f;
	};

	void BuildBurst();           // 生成一阵闪电（1~3 次放电）
	void ScheduleNext();         // 安排下一次闪电的时间
	float Rand(float a, float b);

	std::vector<Pulse> m_pulses;
	float m_burstTime = 0.0f;    // 当前这阵闪电已经过去多久
	float m_nextIn    = 0.0f;    // 距离下一次闪电还有多久
	float m_flash     = 0.0f;
	bool  m_active    = false;
	bool  m_started   = false;
};
