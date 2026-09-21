// D3D11ImageView 테스트 호스트
//
// 합성 이미지를 생성해 뷰어에 넣고, 단축키로 크기/채널을 바꿔가며
// 렌더링 동작(밉맵, LOD 전환, 타일 이음새, 오버레이 정렬)을 육안으로 검증한다.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <shlwapi.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <memory>
#include <new>
#include <vector>

#include "D3D11ImageView.h"
#include "../Overlay Renderer/OverlayTypes.h"

#pragma comment(lib, "Shlwapi.lib")

namespace
{
	constexpr wchar_t kWindowClass[] = L"D3D11ImageViewTestHostWnd";

	// 테스트 이미지 크기 프리셋.
	// Single/Tiled 판정 경계와 피라미드 임계값(약 20,000px)을 걸치도록 골랐다.
	struct SizePreset
	{
		uint32_t width;
		uint32_t height;
		const wchar_t* label;
	};

	constexpr SizePreset kSizePresets[] =
	{
		{  1024,  1024, L"1024^2 (Single)"                    },
		{  4096,  4096, L"4096^2 (Single)"                    },
		{  8192,  8192, L"8192^2 (Single/Tiled 경계)"         },
		{ 16300, 16300, L"16300^2 (Tiled)"                    },
		{ 30000,  2000, L"30000x2000 (라인스캔, 변 초과)"     },
		{ 40000, 40000, L"40000^2 (Tiled, 피라미드 임계 초과)" },
	};

	constexpr uint32_t kSizePresetCount = static_cast<uint32_t>(std::size(kSizePresets));

	const wchar_t* ChannelLabel(uint32_t channel, uint32_t bitDepth)
	{
		if (channel == 1)
		{
			return (bitDepth == 16) ? L"Gray 16bit" : L"Gray 8bit";
		}

		switch (channel)
		{
		case 3:  return L"BGR 24bit";
		case 4:  return L"BGRA 32bit";
		default: return L"?";
		}
	}

	// 합성 이미지.
	// 검증 목적별로 패턴을 섞는다.
	//   체커보드   : 축소 시 에일리어싱/모아레 확인 (밉맵 효과)
	//   그라디언트 : 색/채널 순서 확인
	//   1px 점     : 밉 필터가 미세 결함을 씻어버리는지 확인
	//   격자선     : 타일 경계 이음새와 좌표 정렬 확인
	class SyntheticImage
	{
	public:
		bool Build(uint32_t width, uint32_t height, uint32_t channel, uint32_t bitDepth)
		{
			// 16bit 은 Gray 만 뷰어가 받는다.
			if (bitDepth != 8 && bitDepth != 16)
				return false;
			if (bitDepth == 16 && channel != 1)
				return false;

			const uint32_t bytesPerChannel = bitDepth / 8;
			const size_t stride = static_cast<size_t>(width) * channel * bytesPerChannel;
			const size_t bytes = stride * height;

			try
			{
				m_buffer.assign(bytes, static_cast<uint8_t>(0));
			}
			catch (const std::bad_alloc&)
			{
				Release();
				return false;
			}

			m_width = width;
			m_height = height;
			m_channel = channel;
			m_bitDepth = bitDepth;
			m_stride = static_cast<uint32_t>(stride);

			constexpr uint32_t kCheckerSize = 64;    // 체커 한 칸
			constexpr uint32_t kGridSpacing = 512;   // 격자선 간격 = 타일 크기
			constexpr uint32_t kDefectSpacing = 1024;

			const uint32_t widthDivisor = (width > 1) ? (width - 1) : 1;
			const uint32_t heightDivisor = (height > 1) ? (height - 1) : 1;

			for (uint32_t y = 0; y < height; ++y)
			{
				uint8_t* row = m_buffer.data() + static_cast<size_t>(y) * stride;

				const uint32_t checkerY = y / kCheckerSize;
				const bool onGridRow = (y % kGridSpacing) == 0;
				const uint32_t gradY = (y * 255u) / heightDivisor;

				for (uint32_t x = 0; x < width; ++x)
				{
					const uint32_t checkerX = x / kCheckerSize;
					const bool checker = ((checkerX + checkerY) & 1u) != 0;
					const uint32_t gradX = (x * 255u) / widthDivisor;

					// 그라디언트를 바탕으로 체커를 얹는다.
					uint8_t r = static_cast<uint8_t>(checker ? gradX : (gradX / 3));
					uint8_t g = static_cast<uint8_t>(checker ? gradY : (gradY / 3));
					uint8_t b = static_cast<uint8_t>(checker ? 200 : 60);

					// 격자선 (타일 경계 정렬 확인용)
					if (onGridRow || (x % kGridSpacing) == 0)
					{
						r = 0; g = 255; b = 0;
					}

					// 1픽셀 결함점 (밉 필터 검증용)
					if ((x % kDefectSpacing) == (kDefectSpacing / 2) &&
						(y % kDefectSpacing) == (kDefectSpacing / 2))
					{
						r = 255; g = 0; b = 255;
					}

					if (bitDepth == 16)
					{
						// Gray 16bit. 8bit 값을 << 8 로 늘리지 않고 전 범위를
						// 새로 계산한다. 그래야 상태바에 0~65535 원본값이
						// 표시되는지, 8bit 경로를 잘못 타고 있지 않은지
						// 눈으로 구분할 수 있다.
						const uint32_t luma8 = (r * 77u + g * 150u + b * 29u) >> 8;
						const uint16_t luma16 = static_cast<uint16_t>((luma8 * 65535u) / 255u);

						uint16_t* px16 = reinterpret_cast<uint16_t*>(row) + x;
						px16[0] = luma16;
						continue;
					}

					uint8_t* px = row + static_cast<size_t>(x) * channel;
					if (channel == 1)
					{
						// 휘도로 환산
						px[0] = static_cast<uint8_t>((r * 77u + g * 150u + b * 29u) >> 8);
					}
					else if (channel == 3)
					{
						px[0] = b; px[1] = g; px[2] = r;
					}
					else
					{
						px[0] = b; px[1] = g; px[2] = r; px[3] = 255;
					}
				}
			}

			return true;
		}

		void Release()
		{
			m_buffer.clear();
			m_buffer.shrink_to_fit();
			m_width = 0;
			m_height = 0;
			m_channel = 0;
			m_bitDepth = 8;
			m_stride = 0;
		}

		const uint8_t* Data() const { return m_buffer.empty() ? nullptr : m_buffer.data(); }
		uint32_t Width()   const { return m_width; }
		uint32_t Height()  const { return m_height; }
		uint32_t Stride()  const { return m_stride; }
		uint32_t Channel() const { return m_channel; }
		uint32_t BitDepth() const { return m_bitDepth; }
		size_t   Bytes()   const { return m_buffer.size(); }

	private:
		std::vector<uint8_t> m_buffer;
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_channel = 0;
		uint32_t m_bitDepth = 8;
		uint32_t m_stride = 0;
	};

	struct HostState
	{
		std::unique_ptr<D3D11ImageView> viewer;
		SyntheticImage image;

		uint32_t sizeIndex = 0;
		uint32_t channel = 1;
		uint32_t bitDepth = 8;
		uint32_t stressCount = 0;
		bool stressOffscreen = false;
		bool liveFeed = false;
		bool stressFilledRect = false;
		bool overlayVisible = true;
		bool lastLoadFailed = false;

		SyntheticImage& Active() { return image; }
	};

	HostState g_state;

	// 셰이더는 런타임에 L"../Shaders/*.cso" 로 로드된다.
	// 실행 파일이 <sln>\x64\Debug 에 있으므로 작업 디렉터리를 <sln>\x64 로 맞춰야
	// ../Shaders 가 <sln>\Shaders 로 해석된다.
	void FixWorkingDirectory()
	{
		wchar_t modulePath[MAX_PATH] = {};
		if (::GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0)
		{
			return;
		}

		::PathRemoveFileSpecW(modulePath);   // <sln>\x64\Debug
		::PathRemoveFileSpecW(modulePath);   // <sln>\x64

		::SetCurrentDirectoryW(modulePath);
	}

	void UpdateTitle(HWND hWnd)
	{
		const SizePreset& preset = kSizePresets[g_state.sizeIndex];

		wchar_t title[512] = {};
		if (g_state.lastLoadFailed)
		{
			::swprintf_s(title,
				L"D3D11ImageView TestHost  |  %s  %s  |  메모리 할당 실패 - 더 작은 크기를 선택하세요",
				preset.label, ChannelLabel(g_state.channel, g_state.bitDepth));
		}
		else
		{
			const double mib = static_cast<double>(g_state.Active().Bytes()) / (1024.0 * 1024.0);
			::swprintf_s(title,
				L"D3D11ImageView TestHost  |  %s  %s  |  %.1f MiB  |  OVL=%u%s  |  "
				L"LIVE=%d  |  [1-6] 크기  [G/C/A] 채널  [B] 8/16bit  [P] 부하  [F] 화면밖  [L] 라이브  [D] 디바이스로스트",
				preset.label, ChannelLabel(g_state.channel, g_state.bitDepth), mib,
				g_state.stressCount, g_state.stressOffscreen ? L"(off)" : L"",
				g_state.liveFeed ? 1 : 0);
		}

		::SetWindowTextW(hWnd, title);
	}

	// 이미지 좌표계 오버레이. ImageSpace 정렬과 반픽셀 오프셋을 확인한다.
	void RebuildOverlay()
	{
		if (!g_state.viewer)
		{
			return;
		}

		g_state.viewer->ImageOverlayClear();
		g_state.viewer->ROIClear();

		const uint32_t width = g_state.Active().Width();
		const uint32_t height = g_state.Active().Height();
		if (width == 0 || height == 0)
		{
			return;
		}

		const float widthF = static_cast<float>(width);
		const float heightF = static_cast<float>(height);

		// 반투명 채움 사각형 - 알파 블렌딩 확인
		OverlayStyle fillStyle = {};
		fillStyle.fillColor = { 255, 0, 0, 96 };
		fillStyle.strokeColor = { 255, 255, 0, 255 };
		fillStyle.strokeWidth = 1.0f;
		fillStyle.transparentFill = false;
		fillStyle.UpdateD2DColors();

		const Rect2f fillRect
		{
			widthF * 0.25f, heightF * 0.25f,
			widthF * 0.45f, heightF * 0.45f
		};
		g_state.viewer->ImageOverlayAdd(&fillRect, 1, fillStyle);

		// 이미지 원점 0,0 에 붙는 8x8 사각형 - 좌표 정렬 확인용
		OverlayStyle originStyle = {};
		originStyle.strokeColor = { 0, 255, 255, 255 };
		originStyle.strokeWidth = 1.0f;
		originStyle.transparentFill = true;
		originStyle.UpdateD2DColors();

		const Rect2f originRect{ 0.0f, 0.0f, 8.0f, 8.0f };
		g_state.viewer->ImageOverlayAdd(&originRect, 1, originStyle);

		// 반투명 폴리곤 - 지오메트리 경로 확인
		OverlayStyle polyStyle = {};
		polyStyle.fillColor = { 0, 128, 255, 110 };
		polyStyle.strokeColor = { 255, 255, 255, 255 };
		polyStyle.strokeWidth = 2.0f;
		polyStyle.transparentFill = false;
		polyStyle.UpdateD2DColors();

		constexpr int kPolygonVertexCount = 7;

		Polygon2f polygon;
		if (polygon.Reserve(kPolygonVertexCount))
		{
			const float centerX = widthF * 0.7f;
			const float centerY = heightF * 0.7f;
			const float radius = ((widthF < heightF) ? widthF : heightF) * 0.12f;

			for (int i = 0; i < kPolygonVertexCount; ++i)
			{
				const float angle = 6.2831853f * static_cast<float>(i) / kPolygonVertexCount;
				polygon.AddVertex(Point2f{ centerX + radius * ::cosf(angle),
										   centerY + radius * ::sinf(angle) });
			}

			g_state.viewer->ImageOverlayAdd(&polygon, 1, polyStyle);
		}

		// 부하 측정용 대량 오버레이.
		//
		// 머신비전에서 검사 결과를 수천~수십만 개 찍는 상황을 흉내낸다.
		// OverlayRenderLayer::Render 는 매 프레임 전체 개체를 선형 순회하며
		// 컬링하므로, 개수에 따른 프레임 비용 증가를 여기서 관측한다.
		if (g_state.stressCount > 0)
		{
			OverlayStyle pointStyle = {};
			pointStyle.strokeColor = { 255, 200, 0, 255 };
			pointStyle.strokeWidth = 1.0f;
			pointStyle.transparentFill = true;
			pointStyle.UpdateD2DColors();

			// 화면 안/밖이 섞이도록 이미지 전체에 흩뿌린다.
			// (전부 화면 안이면 컬링이 아무것도 걸러내지 않아 최악값이 나오고,
			//  전부 밖이면 컬링만 측정된다. 둘 다 보려면 섞어야 한다)
			std::vector<Point2f> points;
			points.reserve(g_state.stressCount);

			// stressOffscreen: 이미지 훨씬 밖(음수 좌표)에 배치해서 컬링이
			// 100% 걸러내게 만든다. 이때 남는 비용이 순수 선형 순회 비용이고,
			// 화면 안 배치와의 차이가 실제 D2D 드로우 비용이다.
			uint32_t seed = 12345u;
			for (uint32_t i = 0; i < g_state.stressCount; ++i)
			{
				seed = seed * 1664525u + 1013904223u;
				const float fx = static_cast<float>((seed >> 16) & 0xFFFF) / 65535.0f;
				seed = seed * 1664525u + 1013904223u;
				const float fy = static_cast<float>((seed >> 16) & 0xFFFF) / 65535.0f;

				if (g_state.stressOffscreen)
				{
					points.push_back(Point2f{ -1000000.0f - fx * widthF,
											  -1000000.0f - fy * heightF });
				}
				else
				{
					points.push_back(Point2f{ fx * widthF, fy * heightF });
				}
			}

			if (g_state.stressFilledRect)
			{
				// 채움 사각형은 경로 지오메트리를 쓴다.
				// 지오메트리 캐시 효과를 측정하는 경로.
				OverlayStyle rectStyle = {};
				rectStyle.fillColor = { 255, 160, 0, 90 };
				rectStyle.strokeColor = { 255, 220, 0, 255 };
				rectStyle.strokeWidth = 1.0f;
				rectStyle.transparentFill = false;
				rectStyle.UpdateD2DColors();

				std::vector<Rect2f> rects;
				rects.reserve(points.size());
				for (const Point2f& p : points)
				{
					rects.push_back(Rect2f{ p.x, p.y, p.x + 24.0f, p.y + 24.0f });
				}
				g_state.viewer->ImageOverlayAdd(rects.data(), rects.size(), rectStyle);
			}
			else
			{
			g_state.viewer->ImageOverlayAdd(points.data(), points.size(), pointStyle);
			}
		}

		// 편집 가능한 ROI - 핸들 크기와 줌 보정 확인
		const Rect2f roiRect
		{
			widthF * 0.05f, heightF * 0.60f,
			widthF * 0.25f, heightF * 0.80f
		};
		g_state.viewer->ROISet(L"roi.rect", L"ROI Rect", roiRect,
			RGB(0, 255, 128), true, true, 14);
	}

	void ReloadImage(HWND hWnd)
	{
		const SizePreset& preset = kSizePresets[g_state.sizeIndex];

		// ★ 버퍼를 해제/재할당하기 전에 뷰어의 참조를 반드시 끊는다.
		//
		// UpdateImage 는 포인터를 복사하지 않고 빌려간다(ImageBase::Attach).
		// 렌더 스레드와 타일 워커가 그 메모리를 비동기로 읽으므로,
		// 이 호출 없이 해제하면 use-after-free 로 죽는다.
		// DetachImage 는 워커 배수까지 마친 뒤 반환한다.
		if (g_state.viewer)
		{
			g_state.viewer->DetachImage();
		}

		// 이제 안전하게 해제 -> 피크 메모리를 한 세대로 억제할 수 있다.
		g_state.image.Release();

		g_state.lastLoadFailed = !g_state.image.Build(preset.width, preset.height, g_state.channel, g_state.bitDepth);

		if (!g_state.lastLoadFailed && g_state.viewer)
		{
			g_state.viewer->UpdateImage(
				g_state.image.Data(),
				g_state.image.Width(),
				g_state.image.Height(),
				g_state.image.Stride(),
				g_state.image.Channel(),
				g_state.image.BitDepth());

			RebuildOverlay();
		}

		UpdateTitle(hWnd);
	}

	// 단축키 처리.
	//
	// 뷰어가 자식 창으로 포커스를 가져가므로 WM_KEYDOWN 이 부모 WndProc 에
	// 도달하지 않는다. 두 창이 같은 스레드에 있으므로 메시지 루프에서
	// 가로채는 것이 가장 확실하다.
	bool HandleHotkey(HWND hWnd, WPARAM key)
	{
		switch (key)
		{
		case '1': case '2': case '3':
		case '4': case '5': case '6':
		{
			const uint32_t index = static_cast<uint32_t>(key - '1');
			if (index < kSizePresetCount)
			{
				g_state.sizeIndex = index;
				ReloadImage(hWnd);
			}
			return true;
		}

		// 채널 전환. Gray 가 아니면 16bit 를 뷰어가 거절하므로 8 로 되돌린다.
		case 'G': g_state.channel = 1; ReloadImage(hWnd); return true;
		case 'C': g_state.channel = 3; g_state.bitDepth = 8; ReloadImage(hWnd); return true;
		case 'A': g_state.channel = 4; g_state.bitDepth = 8; ReloadImage(hWnd); return true;

		// 비트깊이 토글. 16bit 는 Gray 전용이므로 채널도 함께 1 로 맞춘다.
		case 'B':
			g_state.bitDepth = (g_state.bitDepth == 8) ? 16u : 8u;
			if (g_state.bitDepth == 16)
			{
				g_state.channel = 1;
			}
			ReloadImage(hWnd);
			return true;

		// 비전 카메라 급전 시뮬레이션 토글.
		// 켜지면 메시지 루프 유휴 시간에 UpdateImage 를 계속 호출한다.
		case 'L':
			g_state.liveFeed = !g_state.liveFeed;
			UpdateTitle(hWnd);
			return true;

		// 부하 오버레이를 화면 밖/안으로 토글 (컬링 효과 분리 측정)
		case 'F':
			g_state.stressOffscreen = !g_state.stressOffscreen;
			RebuildOverlay();
			UpdateTitle(hWnd);
			return true;

		// 부하 도형을 점 <-> 채움 사각형으로 토글
		case 'T':
			g_state.stressFilledRect = !g_state.stressFilledRect;
			RebuildOverlay();
			UpdateTitle(hWnd);
			return true;

		// 오버레이 부하 단계 순환: 0 -> 1k -> 10k -> 100k -> 0
		case 'P':
		{
			switch (g_state.stressCount)
			{
			case 0:      g_state.stressCount = 1000;   break;
			case 1000:   g_state.stressCount = 10000;  break;
			case 10000:  g_state.stressCount = 100000; break;
			default:     g_state.stressCount = 0;      break;
			}
			RebuildOverlay();
			UpdateTitle(hWnd);
			return true;
		}

		case 'R': ReloadImage(hWnd); return true;

		case 'D':
			// 디바이스 로스트 강제 유발. 복구 후에도 화면/오버레이/ROI 가
			// 그대로 보여야 한다.
			if (g_state.viewer)
			{
				const bool ok = g_state.viewer->SimulateDeviceLost();
				::OutputDebugStringW(ok ? L"[TestHost] device lost: OK\n"
										: L"[TestHost] device lost: FAILED\n");
			}
			return true;

		case 'O':
			g_state.overlayVisible = !g_state.overlayVisible;
			if (g_state.viewer)
			{
				g_state.viewer->ImageOverlayShow(g_state.overlayVisible);
				g_state.viewer->InvalidateFrame();
			}
			return true;

		case VK_ESCAPE:
			::DestroyWindow(hWnd);
			return true;

		default:
			return false;
		}
	}

	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_CREATE:
		{
			RECT clientRect = {};
			::GetClientRect(hWnd, &clientRect);

			g_state.viewer = std::make_unique<D3D11ImageView>();
			if (!g_state.viewer->Initialize(hWnd, clientRect, WS_CHILD | WS_VISIBLE, nullptr))
			{
				::MessageBoxW(hWnd,
					L"D3D11ImageView 초기화에 실패했습니다.\n"
					L"Shaders 폴더(.cso)가 빌드되었는지 확인하세요.",
					L"TestHost", MB_ICONERROR);
				return -1;
			}

			ReloadImage(hWnd);
			return 0;
		}

		case WM_SIZE:
		{
			if (g_state.viewer)
			{
				HWND viewerWnd = g_state.viewer->GetHWND();
				if (viewerWnd)
				{
					::MoveWindow(viewerWnd, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
				}
			}
			return 0;
		}

		case WM_KEYDOWN:
			HandleHotkey(hWnd, wParam);
			return 0;

		case WM_DESTROY:
			g_state.viewer.reset();
			g_state.image.Release();
			::PostQuitMessage(0);
			return 0;

		default:
			break;
		}

		return ::DefWindowProcW(hWnd, message, wParam, lParam);
	}
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
	FixWorkingDirectory();

	WNDCLASSEXW wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEXW);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = reinterpret_cast<HBRUSH>(::GetStockObject(DKGRAY_BRUSH));
	wcex.lpszClassName = kWindowClass;

	if (::RegisterClassExW(&wcex) == 0)
	{
		return 1;
	}

	HWND hWnd = ::CreateWindowExW(
		0, kWindowClass, L"D3D11ImageView TestHost",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1600, 1000,
		nullptr, nullptr, hInstance, nullptr);

	if (hWnd == nullptr)
	{
		return 1;
	}

	::ShowWindow(hWnd, SW_SHOW);
	::UpdateWindow(hWnd);

	MSG msg = {};
	bool running = true;

	while (running)
	{
		// liveFeed 가 켜져 있으면 유휴 시간에 UpdateImage 를 계속 호출해야
		// 하므로 PeekMessage 루프로 돌린다. 꺼져 있으면 GetMessage 처럼
		// 블로킹해서 CPU 를 쓰지 않는다.
		while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				running = false;
				break;
			}

			// 뷰어 자식 창이 포커스를 가져가므로 단축키는 여기서 먼저 처리한다.
			//
			// 다만 Ctrl 이 눌린 조합은 뷰어 것이다(Ctrl +/-/0/1).
			// 호스트 단축키는 전부 수식어 없는 글자/숫자라 겹치지 않는다.
			const bool ctrlDown = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;

			if (!ctrlDown && msg.message == WM_KEYDOWN && HandleHotkey(hWnd, msg.wParam))
			{
				continue;
			}

			::TranslateMessage(&msg);
			::DispatchMessageW(&msg);
		}

		if (!running)
		{
			break;
		}

		if (g_state.liveFeed && g_state.viewer && g_state.Active().Data())
		{
			// 비전 카메라 급전 시뮬레이션.
			//
			// 같은 버퍼를 계속 넘긴다 (카메라가 링버퍼를 재사용하는 것과 동일).
			// 60fps 로 조절한다. 최대 속도로 밀면 이 스레드가 코어 하나를
			// 통째로 태워서 뷰어 쪽 CPU 비용을 측정할 수 없다.
			static LARGE_INTEGER freq = {};
			static LARGE_INTEGER next = {};
			if (freq.QuadPart == 0)
			{
				::QueryPerformanceFrequency(&freq);
				::QueryPerformanceCounter(&next);
			}

			LARGE_INTEGER now = {};
			::QueryPerformanceCounter(&now);

			if (now.QuadPart >= next.QuadPart)
			{
				next.QuadPart = now.QuadPart + freq.QuadPart / 60;

				g_state.viewer->UpdateImage(
					g_state.Active().Data(),
					g_state.Active().Width(),
					g_state.Active().Height(),
					g_state.Active().Stride(),
					g_state.Active().Channel(),
					g_state.Active().BitDepth());
			}
			else
			{
				::Sleep(1);
			}
		}
		else if (!g_state.liveFeed)
		{
			// 할 일이 없으면 메시지가 올 때까지 잔다.
			::WaitMessage();
		}
	}

	return static_cast<int>(msg.wParam);
}
