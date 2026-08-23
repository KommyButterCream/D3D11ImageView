#pragma once

#include <stdint.h>
#include <string.h>
#include <dxgiformat.h>

#include "TileFormat.h"

// 타일 1장을 원본 이미지에서 뽑아 목적지 버퍼에 채우는 순수 함수.
//
// TileManager 의 멤버 상태를 만지지 않으므로 워커 스레드에서 호출할 수 있다.
// (워커는 이 함수와 자기 스테이징 버퍼만 사용한다)

namespace TileSampler
{
	// 타일 생산에 필요한 모든 정보. 워커 큐에 값으로 복사해 넘긴다.
	struct SampleDesc
	{
		// 소스
		const uint8_t* imageData = nullptr;
		uint32_t imageWidth = 0;
		uint32_t imageHeight = 0;
		uint32_t imageStride = 0;
		uint32_t channel = 0;

		// 타일
		uint32_t tileSize = 0;
		uint32_t lodScale = 1;      // 1 << lod
		uint32_t srcStartX = 0;
		uint32_t srcStartY = 0;

		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	};

	inline uint32_t RowPitch(const SampleDesc& desc) noexcept
	{
		return desc.tileSize * TileFormat::BytesPerPixel(desc.format);
	}

	inline size_t RequiredBytes(const SampleDesc& desc) noexcept
	{
		return static_cast<size_t>(RowPitch(desc)) * desc.tileSize;
	}

	// 타일이 이미지 경계를 넘어가는가.
	// 넘지 않으면 아래 루프가 모든 목적지 픽셀을 덮어쓰므로 clear 가 불필요하다.
	inline bool TouchesBoundary(const SampleDesc& desc) noexcept
	{
		const uint32_t span = desc.tileSize * desc.lodScale;

		return (desc.srcStartX + span > desc.imageWidth) ||
			(desc.srcStartY + span > desc.imageHeight);
	}

	// dst 는 최소 RequiredBytes(desc) 바이트여야 한다.
	// 성공하면 true. 지원하지 않는 조합이면 false.
	inline bool Sample(const SampleDesc& desc, uint8_t* dst)
	{
		if (!desc.imageData || !dst || desc.tileSize == 0)
			return false;

		const uint32_t tileSize = desc.tileSize;
		const uint32_t lodScale = desc.lodScale;
		const uint32_t srcStartX = desc.srcStartX;
		const uint32_t srcStartY = desc.srcStartY;
		const uint32_t imageWidth = desc.imageWidth;
		const uint32_t imageHeight = desc.imageHeight;
		const uint32_t imageStride = desc.imageStride;
		const uint32_t channel = desc.channel;
		const uint32_t dstRowPitch = RowPitch(desc);

		// 경계에 걸친 타일만 0 으로 밀어둔다.
		// 내부 타일은 아래에서 전부 덮어쓰므로 clear 가 순수 낭비다.
		if (TouchesBoundary(desc))
		{
			memset(dst, 0, RequiredBytes(desc));
		}

		if (desc.format == DXGI_FORMAT_R8_UNORM && channel == 1)
		{
			// Gray 8bit: 확장 없이 1바이트 그대로. LOD0 은 행 단위 memcpy.
			for (uint32_t y = 0; y < tileSize; ++y)
			{
				const uint32_t srcY = srcStartY + (y * lodScale);
				if (srcY >= imageHeight)
					break;

				uint8_t* dstRow = dst + static_cast<size_t>(y) * dstRowPitch;
				const uint8_t* srcRow = desc.imageData + static_cast<size_t>(srcY) * imageStride;

				if (lodScale == 1)
				{
					const uint32_t copyWidth = (srcStartX + tileSize <= imageWidth)
						? tileSize
						: ((imageWidth > srcStartX) ? (imageWidth - srcStartX) : 0);

					if (copyWidth > 0)
						memcpy(dstRow, srcRow + srcStartX, copyWidth);
				}
				else
				{
					for (uint32_t x = 0; x < tileSize; ++x)
					{
						const uint32_t srcX = srcStartX + (x * lodScale);
						if (srcX >= imageWidth)
							break;

						dstRow[x] = srcRow[srcX];
					}
				}
			}

			return true;
		}

		if (desc.format == DXGI_FORMAT_R16_UNORM && channel == 1)
		{
			for (uint32_t y = 0; y < tileSize; ++y)
			{
				const uint32_t srcY = srcStartY + (y * lodScale);
				if (srcY >= imageHeight)
					break;

				uint16_t* dstRow = reinterpret_cast<uint16_t*>(dst + static_cast<size_t>(y) * dstRowPitch);
				const uint16_t* srcRow = reinterpret_cast<const uint16_t*>(
					desc.imageData + static_cast<size_t>(srcY) * imageStride);

				if (lodScale == 1)
				{
					const uint32_t copyWidth = (srcStartX + tileSize <= imageWidth)
						? tileSize
						: ((imageWidth > srcStartX) ? (imageWidth - srcStartX) : 0);

					if (copyWidth > 0)
						memcpy(dstRow, srcRow + srcStartX, static_cast<size_t>(copyWidth) * 2);
				}
				else
				{
					for (uint32_t x = 0; x < tileSize; ++x)
					{
						const uint32_t srcX = srcStartX + (x * lodScale);
						if (srcX >= imageWidth)
							break;

						dstRow[x] = srcRow[srcX];
					}
				}
			}

			return true;
		}

		if (channel == 3)
		{
			// BGR 24bit -> BGRA 32bit 확장. D3D11 에 24bit 포맷이 없어 필수다.
			for (uint32_t y = 0; y < tileSize; ++y)
			{
				const uint32_t srcY = srcStartY + (y * lodScale);
				if (srcY >= imageHeight)
					break;

				uint32_t* dstRow = reinterpret_cast<uint32_t*>(dst + static_cast<size_t>(y) * dstRowPitch);
				const uint8_t* srcRow = desc.imageData + static_cast<size_t>(srcY) * imageStride;

				for (uint32_t x = 0; x < tileSize; ++x)
				{
					const uint32_t srcX = srcStartX + (x * lodScale);
					if (srcX >= imageWidth)
						break;

					const uint8_t* px = srcRow + static_cast<size_t>(srcX) * 3;
					dstRow[x] = (0xFFu << 24) | (static_cast<uint32_t>(px[2]) << 16)
						| (static_cast<uint32_t>(px[1]) << 8) | px[0];
				}
			}

			return true;
		}

		if (channel == 4)
		{
			// BGRA 32bit: 포맷이 이미 맞으므로 복사만.
			for (uint32_t y = 0; y < tileSize; ++y)
			{
				const uint32_t srcY = srcStartY + (y * lodScale);
				if (srcY >= imageHeight)
					break;

				uint32_t* dstRow = reinterpret_cast<uint32_t*>(dst + static_cast<size_t>(y) * dstRowPitch);
				const uint32_t* srcRow = reinterpret_cast<const uint32_t*>(
					desc.imageData + static_cast<size_t>(srcY) * imageStride);

				if (lodScale == 1)
				{
					const uint32_t copyWidth = (srcStartX + tileSize <= imageWidth)
						? tileSize
						: ((imageWidth > srcStartX) ? (imageWidth - srcStartX) : 0);

					if (copyWidth > 0)
						memcpy(dstRow, srcRow + srcStartX, static_cast<size_t>(copyWidth) * 4);
				}
				else
				{
					for (uint32_t x = 0; x < tileSize; ++x)
					{
						const uint32_t srcX = srcStartX + (x * lodScale);
						if (srcX >= imageWidth)
							break;

						dstRow[x] = srcRow[srcX];
					}
				}
			}

			return true;
		}

		return false;
	}
}
