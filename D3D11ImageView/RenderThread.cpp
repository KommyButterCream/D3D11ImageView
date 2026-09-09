#include "pch.h"
#include "RenderThread.h"

// timeBeginPeriod / timeEndPeriod. 폴백 경로에서만 쓴다.
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

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

	// 고해상도 대기 타이머를 얻지 못했을 때만 의미가 있다.
	// 실패해도 계속 간다. WaitFrameInterval 이 Sleep 폴백으로 동작한다.
	AcquireTimerResolution();

	return Start();
}

void RenderThread::AcquireTimerResolution()
{
	// 정상 경로(고해상도 대기 타이머)는 시스템 타이머 틱과 무관하게 0.5ms
	// 수준 정밀도가 나온다. 프로세스 타이머 해상도에 의존하지 않으므로
	// timeBeginPeriod 를 걸 이유가 없다. 걸면 전력만 더 쓴다.
	if (m_frameTimer.IsHighResolution())
		return;

	// 폴백으로 내려간 경우. 일반 대기 타이머도, WaitFrameInterval 의 Sleep
	// 경로도 시스템 타이머 틱에 묶인다(기본 15.6ms). 이때만 프로세스 타이머
	// 해상도를 1ms 로 올린다.
	// Win10 2004 부터 이 설정은 호출한 프로세스에만 적용된다.
	//
	// timeBeginPeriod 는 참조 카운트 방식이라 중복 호출하면 그만큼 풀어야 한다.
	// StartThread 가 여러 번 불려도 한 번만 걸리게 막는다.
	if (!m_timePeriodSet && ::timeBeginPeriod(1) == TIMERR_NOERROR)
	{
		m_timePeriodSet = true;
	}
}

void RenderThread::ReleaseTimerResolution()
{
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

	m_frameTimer.Cancel();
	ReleaseTimerResolution();
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

	// 매 프레임 새로 건다. SignalAfter 는 한 번만 신호하므로 남은 대기가
	// 다음 프레임으로 새지 않는다.
	if (m_frameTimer.SignalAfter(waitTime_ms))
	{
		// 타이머와 정지 이벤트를 함께 기다린다. 종료 요청이 오면 남은
		// 프레임 시간을 낭비하지 않고 바로 깨어난다.
		m_frameTimer.Wait(stopEvent);
		return;
	}

	// 타이머를 못 만든 환경 폴백. 이 경로가 살아 있다는 것은 WaitableTimer 가
	// 고해상도 타이머를 못 얻었다는 뜻이고, 그때 timeBeginPeriod(1) 을 걸어뒀다.
	const DWORD fallbackWait_ms = static_cast<DWORD>(waitTime_ms + 0.5);

	if (stopEvent)
		::WaitForSingleObject(stopEvent, fallbackWait_ms);
	else
		::Sleep(fallbackWait_ms);
}
