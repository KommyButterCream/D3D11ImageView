#pragma once

#include <stdint.h>

// 표시용 LUT.
//
// 1채널(Gray) 이미지 전용이다. 컬러 이미지는 이미 표시용 공간으로 나온
// 결과라 LUT 를 걸 이유가 없고, 의사색을 씌우면 실제 색을 버리게 된다.
//
// 자동 대비(퍼센타일 스트레치)와 컬러맵을 **한 테이블에 함께 굽는다**.
// 그래서 셰이더는 텍스처 페치 한 번으로 둘 다 처리한다.
//
//   LUT[i] = colormap( clamp( (i - lo) / (hi - lo), 0, 1 ) )
//
// i 는 원본 픽셀 값이다(8bit 는 0..255, 16bit 는 0..65535).
enum class LutPreset : int32_t
{
	// 색 없이 대비만 편다. "LUT 켜기 = 자동 대비" 가 되는 기본값.
	Grayscale = 0,

	// 반전. 밝은 배경의 어두운 결함은 뒤집어 보면 눈이 더 잘 잡는다.
	Inverted,

	// 검정 → 빨강 → 노랑 → 흰색.
	// 휘도가 단조 증가라 "밝을수록 값이 크다" 는 직관이 유지된다.
	Hot,

	// 지각 균등(perceptually uniform)이고 색각이상에도 안전하다.
	// 그라디언트를 왜곡 없이 읽어야 할 때 가장 안전한 선택.
	Viridis,

	// 가장 익숙하지만 휘도가 단조롭지 않다. 청록 부근에서 밝기가 튀어
	// 실제로 없는 경계가 있는 것처럼 보이고, 색각이상자에게 취약하다.
	// 익숙함 때문에 넣되 기본값으로는 쓰지 않는다.
	Jet,

	Count
};

// 스트레치에 쓸 원본 값 범위.
struct LutRange
{
	uint32_t lo = 0;
	uint32_t hi = 0;

	bool IsValid() const { return hi > lo; }
};

class LutTable
{
public:
	// 프리셋 이름. 메뉴 항목 텍스트로 그대로 쓴다.
	static const wchar_t* GetPresetName(LutPreset preset);

	// 원본 값의 최댓값. 8bit -> 255, 16bit -> 65535.
	//
	// 히스토그램 구간 수는 이 값 + 1 이다.
	static uint32_t GetDomainMax(uint32_t bitDepth);

	// 히스토그램 구간 수. 8 -> 256, 16 -> 65536.
	static uint32_t GetBinCount(uint32_t bitDepth);

	// ★ LUT 텍스처의 항목 수. 히스토그램 구간 수와 다를 수 있다.
	//
	// D3D11 의 2D 텍스처 최대 변이 16384 라서 16bit 를 65536 항목으로 만들면
	// CreateTexture2D 가 실패한다(그리고 LUT 가 조용히 꺼진다). 그래서
	// 16bit 는 16384 항목으로 줄여 담는다.
	//
	// 손실은 없다고 봐도 된다. 항목 하나가 원본 값 4개를 덮는데, 결과는
	// 어차피 8bit 백버퍼로 나가고 셰이더가 선형 보간으로 읽기 때문이다.
	static uint32_t GetTextureEntryCount(uint32_t bitDepth);

	// 히스토그램 퍼센타일로 스트레치 범위를 구한다.
	//
	// min/max 를 쓰면 안 된다. 핫픽셀 하나, 데드픽셀 하나로 스트레치가
	// 통째로 망가지는데 실제 센서에서 흔한 일이다. 양끝을 clipRatio 만큼
	// 잘라내면 그런 이상치에 흔들리지 않는다.
	//
	// 큰 이미지는 서브샘플링한다. 퍼센타일에는 100만 샘플이면 통계적으로
	// 과분하고, 4096x4096 을 전부 훑으면 프레임을 붙잡는다.
	//
	// channel 이 1 이 아니면 실패한다(LUT 는 Gray 전용).
	// 평탄한 이미지처럼 범위를 못 구하면 전체 범위를 돌려준다.
	static LutRange ComputeRange(
		const uint8_t* data,
		uint32_t width,
		uint32_t height,
		uint32_t stride,
		uint32_t channel,
		uint32_t bitDepth,
		float clipRatio = 0.001f);   // 양끝 0.1%

	// LUT 엔트리를 채운다.
	//
	// out 은 entryCount * 4 바이트여야 한다(RGBA8).
	//
	// 항목 i 는 원본 값 (i * domainMax / (entryCount - 1)) 에 대응한다.
	// entryCount 와 domainMax 를 따로 받는 이유는 16bit 때문이다 — 항목 수는
	// 텍스처 한계로 16384 인데 원본 값은 65535 까지 간다.
	//
	// range 가 유효하지 않으면 스트레치 없이 전체 범위로 굽는다.
	static void Build(
		LutPreset preset,
		uint32_t entryCount,
		uint32_t domainMax,
		const LutRange& range,
		uint8_t* out);

	// 정규화된 t(0~1)를 프리셋 색으로. 테스트와 미리보기용.
	static void Sample(LutPreset preset, float t, uint8_t rgb[3]);
};
