#pragma once

#include <stdint.h>
#include <dxgiformat.h>

// 소스 이미지 채널/비트깊이 -> 텍스처 포맷 결정과, Single/Tiled 모드 판정.
//
// 배경:
//   - 백버퍼는 D2D interop 요구사항 때문에 항상 B8G8R8A8_UNORM 이지만,
//     SRV 로 바인딩하는 소스 텍스처 포맷은 그 제약을 받지 않는다.
//   - Gray 는 R8_UNORM / R16_UNORM 으로 두면 VRAM 과 업로드 대역폭이 1/4 로 줄고
//     변환 자체가 불필요해진다. 3채널만 24bit DXGI 포맷이 없어 확장이 필수다.

namespace TileFormat
{
	// Feature Level 11.0/11.1 의 Texture2D 한 변 최대 크기.
	constexpr uint32_t kMaxTextureDim = 16384;

	// Single 모드(전량 상주)에 허용할 뷰어 1개당 VRAM 예산.
	// D3D11 에는 부분 상주 텍스처가 없어(밉 체인이 생성 시 전부 커밋됨)
	// 이 예산을 넘으면 부분 상주 수단인 타일링으로 가야 한다.
	constexpr uint64_t kSingleResidentBudgetBytes = 128ull * 1024ull * 1024ull;

	// 밉 체인 오버헤드 배수(1 + 1/4 + 1/16 + ... = 4/3).
	constexpr uint64_t kMipChainNumerator = 4;
	constexpr uint64_t kMipChainDenominator = 3;

	inline DXGI_FORMAT ResolveTextureFormat(uint32_t channel, uint32_t bitDepth) noexcept
	{
		if (channel == 1)
		{
			return (bitDepth > 8) ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
		}

		// 3채널(BGR)은 24bit DXGI 포맷이 없어 4채널로 확장해야 한다.
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	}

	inline uint32_t BytesPerPixel(DXGI_FORMAT format) noexcept
	{
		switch (format)
		{
		case DXGI_FORMAT_R8_UNORM:        return 1;
		case DXGI_FORMAT_R16_UNORM:       return 2;
		case DXGI_FORMAT_B8G8R8A8_UNORM:  return 4;
		default:                          return 4;
		}
	}

	// 단일 채널 포맷은 픽셀 셰이더에서 .r 을 3채널로 복제해야 한다.
	inline bool IsSingleChannel(DXGI_FORMAT format) noexcept
	{
		return format == DXGI_FORMAT_R8_UNORM || format == DXGI_FORMAT_R16_UNORM;
	}

	// 소스 채널을 그대로 텍스처에 넣을 수 있는가.
	// 3채널만 CPU/CS 확장이 필요하다.
	inline bool NeedsChannelExpansion(uint32_t channel) noexcept
	{
		return channel == 3;
	}

	// Single 모드(텍스처 한 장 전량 상주)를 쓸 수 있는지 판정.
	//
	//   1) 변 단위 하드 한계  - 면적이 아니라 각 변으로 판단해야 한다.
	//                          (30000x2000 은 면적은 작지만 변이 초과)
	//   2) 실제 생성할 mip 구성을 포함한 상주 예산
	//      - generateMipMaps == false 이면 mip 0만 계산한다.
	//      - true 이면 전체 mip chain의 4/3 오버헤드를 포함한다.
	inline bool CanUseSingleTexture(uint32_t width, uint32_t height,
		DXGI_FORMAT format, uint32_t concurrentViews = 1,
		bool generateMipMaps = false) noexcept
	{
		if (width == 0 || height == 0)
			return false;

		if (width > kMaxTextureDim || height > kMaxTextureDim)
			return false;

		const uint64_t base =
			static_cast<uint64_t>(width) * height * BytesPerPixel(format);
		const uint64_t resident = generateMipMaps
			? base * kMipChainNumerator / kMipChainDenominator
			: base;

		const uint32_t views = (concurrentViews == 0) ? 1 : concurrentViews;

		return resident <= (kSingleResidentBudgetBytes / views);
	}
}
