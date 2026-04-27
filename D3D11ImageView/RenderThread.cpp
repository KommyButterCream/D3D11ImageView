#include "pch.h"
#include "RenderThread.h"

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

	return Start();
}

void RenderThread::StopThread()
{
	RequestStop();
	::WakeConditionVariable(&m_cv);
	Join();
}

void RenderThread::SetRenderFPS(double fps)
{
	m_renderFps = fps;
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
	const double frameTime_ms = 1000.0 / m_renderFps;

	while (!IsStopRequested())
	{
		::AcquireSRWLockExclusive(&m_srwLock);

		while (!IsStopRequested() &&
			::InterlockedCompareExchange(&m_renderRequested, 0, 1) == 0 &&
			!m_isAnimating)
		{
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

			m_isAnimating = m_renderFunc(&renderContext);
		}

		const double endTime_ms = m_timer.GetTotalTimeMiliSeconds();
		const double elapsed_ms = endTime_ms - startTime_ms;

		if (m_isAnimating)
		{
			const double sleepTime_ms = frameTime_ms - elapsed_ms;
			if (sleepTime_ms > 0)
			{
				SleepPrecise(sleepTime_ms);
			}
		}
	}
}

void RenderThread::SleepPrecise(double sleepTime_ms)
{
	constexpr double thresholdTime_ms = 2.0;
	if (sleepTime_ms >= thresholdTime_ms)
	{
		::Sleep(static_cast<DWORD>(sleepTime_ms - 1));
		sleepTime_ms = 1.0;
	}

	const double startTime_ms = m_timer.GetTotalTimeMiliSeconds();
	double elapsed_ms = 0.0;

	while (elapsed_ms < sleepTime_ms)
	{
		if (IsStopRequested())
		{
			break;
		}

		const double endTime_ms = m_timer.GetTotalTimeMiliSeconds();
		elapsed_ms = endTime_ms - startTime_ms;

		::SwitchToThread();
	}
}
