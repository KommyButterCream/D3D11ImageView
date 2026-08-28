#include "pch.h"
#include "RenderThread.h"

// timeBeginPeriod / timeEndPeriod. 폴백 경로에서만 쓴다.
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

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

	// 고해상도 대기 타이머 시스템 타이머 틱과 무관하게 0.5ms
	// 수준 정밀도가 나오며, 스핀과 달리 CPU 를 쓰지 않는다.
	// 자동 리셋(동기화 타이머)이라 대기가 신호를 소비한다.
	m_frameTimer = ::CreateWaitableTimerExW(
		nullptr,
		nullptr,
		CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
		TIMER_ALL_ACCESS);

	if (m_frameTimer)
	{
		// 정상 경로. 이 타이머는 프로세스 타이머 해상도에 의존하지 않으므로
		// timeBeginPeriod 를 걸 이유가 없다. 걸면 전력만 더 쓴다.
		return true;
	}

	// CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 모드로 생성 실패한 경우에 대한 Fallback
	// 일반 대기 타이머도, WaitFrameInterval 의 Sleep 경로도 시스템 타이머 틱에
	// 묶인다(기본 15.6ms). 이때만 프로세스 타이머 해상도를 1ms 로 올린다.
	// Win10 2004 부터 이 설정은 호출한 프로세스에만 적용된다.
	m_frameTimer = ::CreateWaitableTimerExW(
		nullptr, nullptr, 0, TIMER_ALL_ACCESS);

	// timeBeginPeriod 는 참조 카운트 방식이라 중복 호출하면 그만큼 풀어야 한다.
	// StartThread 가 여러 번 불려도 한 번만 걸리게 막는다.
	if (!m_timePeriodSet && ::timeBeginPeriod(1) == TIMERR_NOERROR)
	{
		m_timePeriodSet = true;
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

	// 폴백 경로에서만 걸었다. 건 만큼만 푼다.
	if (m_timePeriodSet)
	{
		::timeEndPeriod(1);
		m_timePeriodSet = false;
	}
}

void RenderThread::StopThread()
{
	RequestStop();

	// 정지 플래그를 세운 직후 바로 깨우면 놓칠 수 있다.
	// 렌더 스레드는 m_renderLock 을 쥔 채 술어를 평가한 뒤
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
	::AcquireSRWLockExclusive(&m_renderLock);
	::ReleaseSRWLockExclusive(&m_renderLock);

	::WakeAllConditionVariable(&m_cv);

	Join();

	DestroyFrameTimer();
}

void RenderThread::SetRenderFPS(double fps)
{
	if (fps <= 0.0)
		return;

	// 실행 중에 바꿔도 다음 프레임부터 반영된다.
	// 100ns 단위 간격으로 바꿔서 넣는다. 렌더 루프는 이 값을 그대로 읽기만 한다.
	const double interval100ns = 10000000.0 / fps;

	::InterlockedExchange64(&m_frameInterval100ns,
		static_cast<LONG64>(interval100ns + 0.5));
}

void RenderThread::RequestFrame()
{
	if (::InterlockedExchange(&m_renderRequested, 1) == 1)
	{
		return;
	}

	::AcquireSRWLockExclusive(&m_renderLock);
	::ReleaseSRWLockExclusive(&m_renderLock);

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
		// 잠들었다가 깨어난 경우 애니메이션을 위한 dt 를 0 으로 설정하기 위해 잠들었다가 깨어났는지 확인
		bool sleptThisLoop = false;

		::AcquireSRWLockExclusive(&m_renderLock);

		while (!IsStopRequested() &&
			::InterlockedCompareExchange(&m_renderRequested, 0, 1) == 0 &&
			!m_isAnimating)
		{
			sleptThisLoop = true;
			::SleepConditionVariableSRW(&m_cv, &m_renderLock, INFINITE, 0);
		}

		::ReleaseSRWLockExclusive(&m_renderLock);

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
			const LONG64 interval100ns = m_frameInterval100ns;
			const double frameTime_ms = (interval100ns > 0)
				? (static_cast<double>(interval100ns) / 10000.0)
				: 0.0;

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

	// 타이머를 못 만든 환경 폴백. 이 경로가 살아 있다는 것은 CreateFrameTimer 가
	// 고해상도 타이머를 못 얻었다는 뜻이고, 그때 timeBeginPeriod(1) 을 걸어뒀다.
	const DWORD fallbackWait_ms = static_cast<DWORD>(waitTime_ms + 0.5);

	if (stopEvent)
		::WaitForSingleObject(stopEvent, fallbackWait_ms);
	else
		::Sleep(fallbackWait_ms);
}
