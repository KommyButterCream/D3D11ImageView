#include "pch.h"
#include "RenderThread.h"

// Win10 1803 이전 SDK 헤더에는 없다. 값은 winbase.h 정의와 동일.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

RenderThread::RenderThread()
	: ThreadBase(L"RenderThread")
{
}

RenderThread::~RenderThread()
{
	StopThread();
}

void RenderThread::SetRenderFunction(RenderFunc func, void* userData)
{
	m_renderFunc = func;
	m_userData = userData;
}

bool RenderThread::StartThread()
{
	m_frameID = 0;
	m_isAnimating = false;
	::InterlockedExchange(&m_renderRequested, 0);
	m_timer.Reset();

	// 실패해도 계속 간다. WaitFrameInterval 이 Sleep 폴백으로 동작한다.
	CreateFrameTimer();

	return Start();
}

bool RenderThread::CreateFrameTimer()
{
	if (m_frameTimer)
		return true;

	// 고해상도 대기 타이머(Win10 1803+). timeBeginPeriod 에 의존하지 않고
	// 0.5ms 수준 정밀도가 나오며, 스핀과 달리 CPU 를 쓰지 않는다.
	// 자동 리셋(동기화 타이머)이라 대기가 신호를 소비한다.
	m_frameTimer = ::CreateWaitableTimerExW(
		nullptr,
		nullptr,
		CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
		TIMER_ALL_ACCESS);

	if (!m_frameTimer)
	{
		// 고해상도 플래그를 모르는 환경이면 일반 대기 타이머로 후퇴한다.
		m_frameTimer = ::CreateWaitableTimerExW(
			nullptr, nullptr, 0, TIMER_ALL_ACCESS);
	}

	return m_frameTimer != nullptr;
}

void RenderThread::DestroyFrameTimer()
{
	if (m_frameTimer)
	{
		::CancelWaitableTimer(m_frameTimer);
		::CloseHandle(m_frameTimer);
		m_frameTimer = nullptr;
	}
}

void RenderThread::StopThread()
{
	RequestStop();

	// 정지 플래그를 세운 직후 바로 깨우면 놓칠 수 있다.
	// 렌더 스레드는 m_srwLock 을 쥔 채 술어를 평가한 뒤
	// SleepConditionVariableSRW 로 진입하는데, 그 사이 구간에
	// WakeConditionVariable 이 끼면 대기자가 아직 없어 무효화되고
	// 스레드는 INFINITE 로 잠들어 Join 이 영구 대기한다.
	//
	// 락을 한 번 통과시키면 두 경우 중 하나가 보장된다.
	//   - 렌더 스레드보다 먼저 잡는 경우: 렌더 스레드는 RequestStop 이
	//     반영된 상태로 술어를 평가하므로 대기에 들어가지 않는다.
	//   - 렌더 스레드가 놓은 뒤에 잡는 경우: 대기 등록이 이미 끝났으므로
	//     Wake 가 전달된다.
	// RequestFrame 이 쓰는 것과 같은 처리다.
	::AcquireSRWLockExclusive(&m_srwLock);
	::ReleaseSRWLockExclusive(&m_srwLock);

	::WakeAllConditionVariable(&m_cv);

	Join();

	DestroyFrameTimer();
}

void RenderThread::SetRenderFPS(double fps)
{
	if (fps <= 0.0)
		return;

	// 실행 중에 바꿔도 다음 프레임부터 반영된다.
	m_renderFps.store(fps, std::memory_order_relaxed);
}

void RenderThread::RequestFrame()
{
	if (::InterlockedExchange(&m_renderRequested, 1) == 1)
	{
		return;
	}

	::AcquireSRWLockExclusive(&m_srwLock);
	::ReleaseSRWLockExclusive(&m_srwLock);

	::WakeConditionVariable(&m_cv);
}

void RenderThread::Run()
{
	ThreadRenderLoop();
}

void RenderThread::ThreadRenderLoop()
{
	while (!IsStopRequested())
	{
		// 이번 루프에서 실제로 잠들었는지 기록한다.
		//
		// 잠들었다 = 애니메이션이 돌고 있지 않았다(술어가 !m_isAnimating 이므로).
		// 그 다음 프레임은 새 애니메이션의 첫 프레임이고, 유휴 시간은 애니메이션
		// 시간에 포함되면 안 된다. 이걸 알려주지 않으면 콜백이 유휴 시간 전체를
		// dt 로 받아서(엔진 타이머가 0.1초로 클램프) 보간이 한 번에 튄다.
		bool sleptThisLoop = false;

		::AcquireSRWLockExclusive(&m_srwLock);

		while (!IsStopRequested() &&
			::InterlockedCompareExchange(&m_renderRequested, 0, 1) == 0 &&
			!m_isAnimating)
		{
			sleptThisLoop = true;
			::SleepConditionVariableSRW(&m_cv, &m_srwLock, INFINITE, 0);
		}

		::ReleaseSRWLockExclusive(&m_srwLock);

		if (IsStopRequested())
		{
			break;
		}

		const double startTime_ms = m_timer.GetTotalTimeMiliSeconds();

		if (m_renderFunc)
		{
			++m_frameID;

			RenderContext renderContext = {};
			renderContext.imageViewImpl = m_userData;
			renderContext.frameID = m_frameID;
			renderContext.resumedFromIdle = sleptThisLoop;

			m_isAnimating = m_renderFunc(&renderContext);
		}

		const double endTime_ms = m_timer.GetTotalTimeMiliSeconds();
		const double elapsed_ms = endTime_ms - startTime_ms;

		if (m_isAnimating)
		{
			// 매 프레임 다시 읽는다. 실행 중 SetRenderFPS 변경이 반영된다.
			const double fps = m_renderFps.load(std::memory_order_relaxed);
			const double frameTime_ms = (fps > 0.0) ? (1000.0 / fps) : 0.0;

			WaitFrameInterval(frameTime_ms - elapsed_ms);
		}
	}
}

void RenderThread::WaitFrameInterval(double waitTime_ms)
{
	if (waitTime_ms <= 0.0)
		return;

	HANDLE stopEvent = GetStopEvent();

	if (m_frameTimer)
	{
		// 음수 = 상대 시간, 단위는 100ns.
		LARGE_INTEGER dueTime = {};
		dueTime.QuadPart = -static_cast<LONGLONG>(waitTime_ms * 10000.0);

		if (dueTime.QuadPart == 0)
			dueTime.QuadPart = -1;

		if (::SetWaitableTimer(m_frameTimer, &dueTime, 0, nullptr, nullptr, FALSE))
		{
			// 타이머와 정지 이벤트를 함께 기다린다. 종료 요청이 오면 남은
			// 프레임 시간을 낭비하지 않고 바로 깨어난다.
			HANDLE handles[2] = { m_frameTimer, stopEvent };
			const DWORD handleCount = stopEvent ? 2u : 1u;

			::WaitForMultipleObjects(handleCount, handles, FALSE, INFINITE);
			return;
		}
	}

	// 타이머를 못 만든 환경 폴백. timeBeginPeriod(1) 이 걸려 있으므로
	// 1ms 해상도는 확보된다.
	const DWORD fallbackWait_ms = static_cast<DWORD>(waitTime_ms + 0.5);

	if (stopEvent)
		::WaitForSingleObject(stopEvent, fallbackWait_ms);
	else
		::Sleep(fallbackWait_ms);
}
