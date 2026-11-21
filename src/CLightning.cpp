#include "CLightning.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace
{
	std::mt19937 g_rng{ 20241121u };
}

float CLightning::Rand(float a, float b)
{
	std::uniform_real_distribution<float> dist(a, b);
	return dist(g_rng);
}

void CLightning::ScheduleNext()
{
	m_nextIn = Rand(std::max(minInterval, 0.1f), std::max(maxInterval, minInterval + 0.1f));
}

void CLightning::BuildBurst()
{
	m_pulses.clear();
	m_burstTime = 0.0f;
	m_active    = true;

	// 只闪一次：单次放电，最亮也最容易读成"闪电"
	const int count = 1;
	float start = 0.0f;
	for (int i = 0; i < count; ++i)
	{
		Pulse p;
		p.start    = start;
		p.duration = Rand(0.55f, 0.95f);
		p.amp      = 1.0f;
		m_pulses.push_back(p);
		start += p.duration;
	}
}

void CLightning::Trigger()
{
	// 正在闪的时候不重开，避免叠在一起
	if (m_active) return;
	BuildBurst();
}

void CLightning::Update(float dt)
{
	// 首帧 / 卡顿时 dt 可能很大（初始化就花了十几秒），直接吃光一次闪电
	dt = std::min(dt, 0.1f);
	if (!enabled)
	{
		m_flash  = 0.0f;
		m_active = false;
		return;
	}

	if (!m_started)
	{
		m_started = true;
		ScheduleNext();
		// 开局先给一次，不然要干等十几秒
		m_nextIn = std::min(m_nextIn, 2.0f);
	}

	if (!m_active)
	{
		m_nextIn -= dt;
		if (m_nextIn <= 0.0f)
			BuildBurst();
	}

	if (!m_active)
	{
		m_flash = 0.0f;
		return;
	}

	m_burstTime += dt;

	float sum = 0.0f;
	float burstEnd = 0.0f;
	for (const Pulse& p : m_pulses)
	{
		burstEnd = std::max(burstEnd, p.start + p.duration);

		const float t = m_burstTime - p.start;
		if (t < 0.0f || t > p.duration) continue;

		// 前沿极快（约 1 帧到达峰值），之后线性衰减到 10%：
		// 用线性而不是指数，保证任何帧率下都能看到若干帧闪光。
		// 上升时间固定 0.08s（不随 duration 缩放），这样加长闪烁时长只是拖慢衰减
		const float k   = std::min(t / 0.08f, 1.0f);
		const float rise = k * k * (3.0f - 2.0f * k);
		const float u   = t / std::max(p.duration, 1e-4f);
		const float fall = 1.0f - 0.90f * u;
		sum += p.amp * rise * fall;
	}

	m_flash = std::min(sum, 1.0f);

	// 这阵闪电放完了：留一点余辉后重新排期
	if (m_burstTime > burstEnd + 0.20f)
	{
		m_active = false;
		m_flash  = 0.0f;
		ScheduleNext();
	}
}
