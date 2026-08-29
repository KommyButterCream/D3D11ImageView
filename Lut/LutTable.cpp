// pch.h 를 쓰지 않는다.
//
// 순수 CPU 코드라 D3D 의존이 없고, 그래서 하네스가 솔루션 빌드 없이
// 이 파일만 직접 컴파일해 검증할 수 있다.
#include "LutTable.h"

#include <vector>
#include <algorithm>
#include <math.h>

namespace
{
	inline float Clamp01(float v)
	{
		if (v < 0.0f) return 0.0f;
		if (v > 1.0f) return 1.0f;
		return v;
	}

	inline uint8_t ToByte(float v)
	{
		// 0.5 를 더해 반올림한다. 그냥 자르면 흰색이 254 에서 멈춘다.
		const float scaled = Clamp01(v) * 255.0f + 0.5f;
		return static_cast<uint8_t>(scaled);
	}

	struct ColorStop
	{
		float t;
		float r, g, b;
	};

	// 제어점 사이를 선형 보간한다.
	void SampleStops(const ColorStop* stops, size_t count, float t, float out[3])
	{
		t = Clamp01(t);

		if (t <= stops[0].t)
		{
			out[0] = stops[0].r; out[1] = stops[0].g; out[2] = stops[0].b;
			return;
		}

		for (size_t i = 1; i < count; ++i)
		{
			if (t <= stops[i].t)
			{
				const ColorStop& a = stops[i - 1];
				const ColorStop& b = stops[i];
				const float span = b.t - a.t;
				const float k = (span > 0.0f) ? (t - a.t) / span : 0.0f;

				out[0] = a.r + (b.r - a.r) * k;
				out[1] = a.g + (b.g - a.g) * k;
				out[2] = a.b + (b.b - a.b) * k;
				return;
			}
		}

		const ColorStop& last = stops[count - 1];
		out[0] = last.r; out[1] = last.g; out[2] = last.b;
	}

	// matplotlib viridis 를 1/16 간격 제어점으로 근사한다.
	//
	// 원본은 256개 항목이지만, 여기서 선형 보간해도 눈으로 구분되지 않는다.
	// 지각 균등성과 색각이상 안전성이라는 성질은 그대로 유지된다.
	const ColorStop kViridis[] = {
		{ 0.0000f, 0.267004f, 0.004874f, 0.329415f },
		{ 0.0625f, 0.277018f, 0.050344f, 0.375715f },
		{ 0.1250f, 0.282623f, 0.140926f, 0.457517f },
		{ 0.1875f, 0.278826f, 0.199430f, 0.509420f },
		{ 0.2500f, 0.253935f, 0.265254f, 0.529983f },
		{ 0.3125f, 0.229739f, 0.322361f, 0.545706f },
		{ 0.3750f, 0.206756f, 0.371758f, 0.553117f },
		{ 0.4375f, 0.184586f, 0.423943f, 0.556295f },
		{ 0.5000f, 0.163625f, 0.471133f, 0.558148f },
		{ 0.5625f, 0.144759f, 0.519093f, 0.556572f },
		{ 0.6250f, 0.127568f, 0.566949f, 0.550556f },
		{ 0.6875f, 0.120638f, 0.625828f, 0.533488f },
		{ 0.7500f, 0.134692f, 0.658636f, 0.517649f },
		{ 0.8125f, 0.181983f, 0.708124f, 0.485380f },
		{ 0.8750f, 0.266941f, 0.748751f, 0.440573f },
		{ 0.9375f, 0.477504f, 0.821444f, 0.318195f },
		{ 1.0000f, 0.993248f, 0.906157f, 0.143936f },
	};

	void SampleHot(float t, float out[3])
	{
		// MATLAB 의 hot. 빨강이 먼저 3/8 구간에서 차오르고, 그 다음 초록,
		// 마지막에 파랑이 올라오면서 흰색이 된다.
		constexpr float n = 3.0f / 8.0f;

		out[0] = Clamp01(t / n);
		out[1] = Clamp01((t - n) / n);
		out[2] = Clamp01((t - 2.0f * n) / (1.0f - 2.0f * n));
	}

	void SampleJet(float t, float out[3])
	{
		// MATLAB 의 jet 을 사다리꼴 세 개로 표현한 표준 근사.
		out[0] = Clamp01(1.5f - fabsf(4.0f * t - 3.0f));
		out[1] = Clamp01(1.5f - fabsf(4.0f * t - 2.0f));
		out[2] = Clamp01(1.5f - fabsf(4.0f * t - 1.0f));
	}
}

const wchar_t* LutTable::GetPresetName(LutPreset preset)
{
	switch (preset)
	{
	case LutPreset::Grayscale: return L"Grayscale";
	case LutPreset::Inverted:  return L"Inverted";
	case LutPreset::Hot:       return L"Hot";
	case LutPreset::Viridis:   return L"Viridis";
	case LutPreset::Jet:       return L"Jet";
	default:                   return L"Grayscale";
	}
}

uint32_t LutTable::GetDomainMax(uint32_t bitDepth)
{
	return (bitDepth > 8) ? 65535u : 255u;
}

uint32_t LutTable::GetBinCount(uint32_t bitDepth)
{
	return GetDomainMax(bitDepth) + 1u;
}

uint32_t LutTable::GetTextureEntryCount(uint32_t bitDepth)
{
	// D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION == 16384.
	// 이걸 넘기면 CreateTexture2D 가 실패하고 LUT 가 조용히 껐진다.
	constexpr uint32_t kMaxTextureWidth = 16384u;

	const uint32_t binCount = GetBinCount(bitDepth);

	return (binCount > kMaxTextureWidth) ? kMaxTextureWidth : binCount;
}

void LutTable::Sample(LutPreset preset, float t, uint8_t rgb[3])
{
	float c[3] = { 0.0f, 0.0f, 0.0f };

	switch (preset)
	{
	case LutPreset::Grayscale:
		c[0] = c[1] = c[2] = Clamp01(t);
		break;

	case LutPreset::Inverted:
		c[0] = c[1] = c[2] = 1.0f - Clamp01(t);
		break;

	case LutPreset::Hot:
		SampleHot(Clamp01(t), c);
		break;

	case LutPreset::Viridis:
		SampleStops(kViridis, _countof(kViridis), t, c);
		break;

	case LutPreset::Jet:
		SampleJet(Clamp01(t), c);
		break;

	default:
		c[0] = c[1] = c[2] = Clamp01(t);
		break;
	}

	rgb[0] = ToByte(c[0]);
	rgb[1] = ToByte(c[1]);
	rgb[2] = ToByte(c[2]);
}

LutRange LutTable::ComputeRange(
	const uint8_t* data,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	uint32_t channel,
	uint32_t bitDepth,
	float clipRatio)
{
	LutRange range;

	// LUT 는 Gray 전용이다.
	if (!data || width == 0 || height == 0 || channel != 1)
		return range;

	if (bitDepth != 8 && bitDepth != 16)
		return range;

	const uint32_t binCount = GetBinCount(bitDepth);

	// 서브샘플링. 각 축에서 최대 1024개만 본다 -> 최대 100만 샘플.
	const uint32_t stepX = (width > 1024u) ? (width / 1024u) : 1u;
	const uint32_t stepY = (height > 1024u) ? (height / 1024u) : 1u;

	std::vector<uint32_t> histogram(binCount, 0u);
	uint64_t sampleCount = 0;

	if (bitDepth == 8)
	{
		for (uint32_t y = 0; y < height; y += stepY)
		{
			const uint8_t* row = data + static_cast<size_t>(y) * stride;

			for (uint32_t x = 0; x < width; x += stepX)
			{
				++histogram[row[x]];
				++sampleCount;
			}
		}
	}
	else
	{
		for (uint32_t y = 0; y < height; y += stepY)
		{
			const uint16_t* row = reinterpret_cast<const uint16_t*>(
				data + static_cast<size_t>(y) * stride);

			for (uint32_t x = 0; x < width; x += stepX)
			{
				++histogram[row[x]];
				++sampleCount;
			}
		}
	}

	if (sampleCount == 0)
		return range;

	const uint64_t clipCount = static_cast<uint64_t>(
		static_cast<double>(sampleCount) * static_cast<double>(clipRatio));

	// 아래쪽 clipRatio 를 넘어서는 첫 번째 값
	uint64_t accumulated = 0;
	uint32_t lo = 0;

	for (uint32_t i = 0; i < binCount; ++i)
	{
		accumulated += histogram[i];

		if (accumulated > clipCount)
		{
			lo = i;
			break;
		}
	}

	// 위쪽 clipRatio 를 넘어서는 첫 번째 값(뒤에서부터)
	accumulated = 0;
	uint32_t hi = binCount - 1;

	for (uint32_t i = binCount; i-- > 0; )
	{
		accumulated += histogram[i];

		if (accumulated > clipCount)
		{
			hi = i;
			break;
		}
	}

	// 평탄한 이미지거나 클리핑이 과했다. 전체 범위로 돌린다.
	if (hi <= lo)
	{
		lo = 0;
		hi = binCount - 1;
	}

	range.lo = lo;
	range.hi = hi;

	return range;
}

void LutTable::Build(
	LutPreset preset,
	uint32_t entryCount,
	uint32_t domainMax,
	const LutRange& range,
	uint8_t* out)
{
	if (!out || entryCount == 0 || domainMax == 0)
		return;

	// 범위를 못 구했으면 스트레치 없이 전체 범위로 굽는다.
	const uint32_t lo = range.IsValid() ? range.lo : 0u;
	const uint32_t hi = range.IsValid() ? range.hi : domainMax;

	const float span = static_cast<float>(hi) - static_cast<float>(lo);
	const float invSpan = (span > 0.0f) ? (1.0f / span) : 0.0f;

	// 항목 -> 원본 값. 8bit 는 1:1 이고, 16bit 는 항목 하나가 값 4개를 덮는다.
	const float toValue = (entryCount > 1)
		? (static_cast<float>(domainMax) / static_cast<float>(entryCount - 1))
		: 0.0f;

	for (uint32_t i = 0; i < entryCount; ++i)
	{
		const float value = static_cast<float>(i) * toValue;

		// 자동 대비: [lo, hi] 를 [0, 1] 로 편다. 밖은 잘린다.
		const float t = (invSpan > 0.0f)
			? Clamp01((value - static_cast<float>(lo)) * invSpan)
			: 0.0f;

		uint8_t rgb[3] = {};
		Sample(preset, t, rgb);

		uint8_t* entry = out + static_cast<size_t>(i) * 4;
		entry[0] = rgb[0];   // R
		entry[1] = rgb[1];   // G
		entry[2] = rgb[2];   // B
		entry[3] = 255;      // A
	}
}
