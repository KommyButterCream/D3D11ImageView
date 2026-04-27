#pragma once

class HighResolutionTimer
{
public:
	HighResolutionTimer()
	{
		::QueryPerformanceFrequency(&m_frequency);

		Reset();
	}

	void Reset()
	{
		::QueryPerformanceCounter(&m_startTime);
		m_lastTime = m_startTime;
	}

	// 총 경과 시간 (초 단위)
	double GetTotalTimeSeconds() const
	{
		LARGE_INTEGER now;
		::QueryPerformanceCounter(&now);
		return double(now.QuadPart - m_startTime.QuadPart) / double(m_frequency.QuadPart);
	}

	double GetTotalTimeMiliSeconds() const
	{
		LARGE_INTEGER now;
		::QueryPerformanceCounter(&now);
		return double(now.QuadPart - m_startTime.QuadPart) / double(m_frequency.QuadPart) * 1000.0;
	}

	// 마지막 호출 이후 경과 시간 (초 단위)
	double GetElapsedSeconds()
	{
		LARGE_INTEGER now;
		::QueryPerformanceCounter(&now);
		double elapsed = double(now.QuadPart - m_lastTime.QuadPart) / double(m_frequency.QuadPart);
		m_lastTime = now;
		return elapsed;
	}

private:
	LARGE_INTEGER m_frequency = {};
	LARGE_INTEGER m_startTime = {};
	LARGE_INTEGER m_lastTime = {};
};
