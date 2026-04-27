#pragma once

#include "../../../Module/Core/Concurrency/ThreadBase.h"

#include "HighResolutionTimer.h"

using RenderFunc = bool(*)(void* userData);

struct RenderContext
{
	void* imageViewImpl = nullptr;
	uint64_t frameID = 0;
};

class RenderThread : public Core::Concurrency::ThreadBase
{
public:
	RenderThread();
	~RenderThread();

	void SetRenderFunction(RenderFunc func, void* userData);

	bool StartThread();
	void StopThread();

	void SetRenderFPS(double fps);
	bool IsRunning() const { return ThreadBase::IsRunning(); }

	void RequestFrame();

protected:
	void Run() override;

private:
	void ThreadRenderLoop();
	void SleepPrecise(double sleepTime_ms);

private:
	RenderFunc m_renderFunc = nullptr;
	void* m_userData = nullptr;

	SRWLOCK m_srwLock = SRWLOCK_INIT;
	CONDITION_VARIABLE m_cv = CONDITION_VARIABLE_INIT;
	alignas(4) long m_renderRequested = 0;

	uint64_t m_frameID = 0;
	bool m_isAnimating = false;

	double m_renderFps = 120.0;

	HighResolutionTimer m_timer;
};
