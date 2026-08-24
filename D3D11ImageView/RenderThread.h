#pragma once

#include "../../../Module/Core/Concurrency/ThreadBase.h"

#include "HighResolutionTimer.h"

#include <atomic>

using RenderFunc = bool(*)(void* userData);

struct RenderContext
{
	void* imageViewImpl = nullptr;
	uint64_t frameID = 0;

	// 렌더 스레드가 유휴 상태에서 깨어나 만드는 첫 프레임인가.
	//
	// 잠들 조건에 !m_isAnimating 이 있으므로, 이 값이 true 면 직전까지
	// 애니메이션이 없었다는 뜻이다. 이 프레임의 delta time 에는 유휴 시간이
	// 통째로 들어있으니 애니메이션 보간에 그대로 쓰면 한 번에 튄다.
	bool resumedFromIdle = false;
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

	// 다음 프레임까지 대기한다. 대기 중 정지 요청이 오면 즉시 깨어난다.
	void WaitFrameInterval(double waitTime_ms);

	bool CreateFrameTimer();
	void DestroyFrameTimer();

private:
	RenderFunc m_renderFunc = nullptr;
	void* m_userData = nullptr;

	SRWLOCK m_srwLock = SRWLOCK_INIT;
	CONDITION_VARIABLE m_cv = CONDITION_VARIABLE_INIT;
	alignas(4) long m_renderRequested = 0;

	uint64_t m_frameID = 0;
	bool m_isAnimating = false;

	// 애니메이션 중 매 프레임 읽는다. 실행 중에도 SetRenderFPS 가 먹도록
	// 루프 밖에서 한 번 계산하지 않는다.
	std::atomic<double> m_renderFps = { 120.0 };

	// 프레임 페이싱용 대기 타이머. 스핀 없이 1ms 이하 정밀도를 얻는다.
	HANDLE m_frameTimer = nullptr;

	HighResolutionTimer m_timer;
};
