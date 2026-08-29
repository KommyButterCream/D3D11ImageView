#include "pch.h"
#include "D3D11ImageView_Impl.h"

#include "../Render Layer/ImageRenderLayer.h"
#include "../../../Module/D3D11ImageIO/D3D11ImageIO/D3D11ImageIO.h"
#include "../../../Module/D3D11Engine/Core/D3D11RenderContext.h"
#include "../../../Module/D3D11Engine/Core/D3D11RenderEngine.h"

#include <commdlg.h>

#pragma comment(lib, "comdlg32.lib")

using namespace Core::ImageType;

namespace
{
	// 대화상자 필터. 이중 널로 끝나야 한다.
	//
	// 형식마다 자기 것을 맨 앞에 둔다. 그래야 그 항목으로 열었을 때 기본
	// 선택이 맞고, nFilterIndex 를 따로 계산할 필요가 없다.
	const wchar_t* GetFilter(SaveImageFormat format)
	{
		switch (format)
		{
		case SaveImageFormat::Jpeg:
			return L"JPEG (*.jpg)\0*.jpg;*.jpeg\0PNG (*.png)\0*.png\0BMP (*.bmp)\0*.bmp\0";

		case SaveImageFormat::Bmp:
			return L"BMP (*.bmp)\0*.bmp\0PNG (*.png)\0*.png\0JPEG (*.jpg)\0*.jpg;*.jpeg\0";

		case SaveImageFormat::Png:
		default:
			return L"PNG (*.png)\0*.png\0JPEG (*.jpg)\0*.jpg;*.jpeg\0BMP (*.bmp)\0*.bmp\0";
		}
	}

	const wchar_t* GetDefaultExtension(SaveImageFormat format)
	{
		switch (format)
		{
		case SaveImageFormat::Jpeg: return L"jpg";
		case SaveImageFormat::Bmp:  return L"bmp";
		case SaveImageFormat::Png:
		default:                    return L"png";
		}
	}

	// ImageBase 는 PixelType 만 노출한다. 채널당 비트 수로 되돌린다.
	uint32_t GetBitDepth(PixelType pixelType)
	{
		switch (pixelType)
		{
		case PixelType::U16: return 16;
		case PixelType::F32: return 32;
		case PixelType::U8:
		default:             return 8;
		}
	}
}

bool D3D11ImageView_Impl::SaveImage(const wchar_t* filePath)
{
	if (!filePath || filePath[0] == L'\0')
		return false;

	return SaveAttachedImage(filePath);
}

// 붙어 있는 이미지를 저장한다.
//
// 두 경로가 있다.
//   RawImage 입력  : 원본이 CPU 에 있다. 그대로 인코더에 넘긴다.
//                    채널/비트깊이가 원본 그대로 보존된다.
//   Texture 입력   : CPU 원본이 없다. GPU 에서 읽어 내린다(BGRA).
//
// 어느 쪽이든 화면이 아니라 원본을 쓴다. 줌 배율이나 ROI 는 결과에 없다.
bool D3D11ImageView_Impl::SaveAttachedImage(const wchar_t* filePath)
{
	if (!m_imageLayer)
		return false;

	if (!m_imageIO)
	{
		m_imageIO = std::make_unique<D3D11ImageIO>();
	}

	if (FAILED(m_imageIO->Initialize()))
		return false;

	// 렌더 스레드가 이미지를 갈아 끼우는 도중에 읽으면 안 된다.
	// Render() 와 같은 락이라 여기를 통과하면 프레임 밖임이 보장된다.
	::AcquireSRWLockExclusive(&m_renderLock);

	HRESULT hr = E_FAIL;

	const ImageBase* image = m_imageLayer->GetImage();

	if (image && !image->IsEmpty())
	{
		hr = m_imageIO->SaveImageToFile(
			image->ImageBuffer(),
			static_cast<uint32_t>(image->Width()),
			static_cast<uint32_t>(image->Height()),
			static_cast<uint32_t>(image->Stride()),
			static_cast<uint32_t>(image->Channel()),
			GetBitDepth(image->GetPixelType()),
			filePath);
	}
	else if (m_imageLayer->GetInputSource() == ImageInputSource::Texture ||
		m_imageLayer->GetInputSource() == ImageInputSource::SharedTexture)
	{
		ID3D11Texture2D* texture = m_imageLayer->GetSingleTexture();

		auto* engine = m_renderContext
			? static_cast<D3D11RenderEngine*>(m_renderContext->GetEngine())
			: nullptr;

		if (texture && engine)
		{
			hr = m_imageIO->SaveTextureToFile(
				engine->GetD3DDevice(),
				engine->GetD3DDeviceContext(),
				texture,
				filePath);
		}
	}

	::ReleaseSRWLockExclusive(&m_renderLock);

	return SUCCEEDED(hr);
}

// 커맨드가 도착한 그 자리에서 대화상자를 열면 안 된다. 이유는 헤더 주석 참고.
void D3D11ImageView_Impl::PostSaveImageRequest(SaveImageFormat format)
{
	const HWND hWnd = GetHWND();

	if (!hWnd || !::IsWindow(hWnd))
		return;

	::PostMessageW(hWnd, WM_D3IV_SAVE_IMAGE, static_cast<WPARAM>(format), 0);
}

LRESULT D3D11ImageView_Impl::OnSaveImageRequested(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	SaveImageWithDialog(static_cast<SaveImageFormat>(wParam));

	return 0L;
}

bool D3D11ImageView_Impl::SaveImageWithDialog(SaveImageFormat format)
{
	// 저장할 게 없으면 대화상자를 띄우지 않는다. 사용자가 경로까지 다 고른
	// 다음에 "이미지가 없습니다" 를 만나는 것보다 낫다.
	if (!m_imageLayer)
		return false;

	const ImageBase* image = m_imageLayer->GetImage();
	const ImageInputSource source = m_imageLayer->GetInputSource();

	const bool hasCpuImage = (image && !image->IsEmpty());
	const bool hasTexture =
		(source == ImageInputSource::Texture || source == ImageInputSource::SharedTexture) &&
		m_imageLayer->GetSingleTexture() != nullptr;

	if (!hasCpuImage && !hasTexture)
		return false;

	wchar_t path[MAX_PATH] = L"image";

	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetHWND();
	ofn.lpstrFilter = GetFilter(format);
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = path;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = L"이미지 저장";

	// lpstrDefExt 가 있으면 사용자가 확장자를 안 적었을 때 붙여 준다.
	// 컨테이너는 최종 확장자가 정하므로 이게 형식까지 맞춰 주는 셈이다.
	ofn.lpstrDefExt = GetDefaultExtension(format);

	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST |
		OFN_NOCHANGEDIR | OFN_EXPLORER;

	// GetSaveFileNameW 는 자기 메시지 루프를 돌린다. 여기까지 오면 마우스
	// 처리는 이미 끝나 있으므로 재진입해도 안전하다.
	if (!::GetSaveFileNameW(&ofn))
	{
		// 사용자가 취소했거나 대화상자가 실패했다. 둘을 구분해야 하면
		// CommDlgExtendedError() 가 0 인지로 본다(0 이면 단순 취소).
		return false;
	}

	return SaveAttachedImage(path);
}
