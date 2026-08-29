// LutTable 회귀 하네스
//
// GPU 없이 도는 부분만 검증한다. 퍼센타일 범위 계산과 테이블 굽기가 대상이다.
//
// 빌드: Lut/Test/build.bat

#include <Windows.h>
#include <stdio.h>
#include <vector>
#include <math.h>

#include "../LutTable.h"

namespace
{
	int g_fail = 0;
	int g_total = 0;

	void Expect(bool ok, const char* what)
	{
		++g_total;
		printf("%-6s %s\n", ok ? "PASS" : "FAIL", what);
		if (!ok) ++g_fail;
	}

	// 8bit Gray 이미지를 만든다. stride 는 ImageBase 처럼 넉넉히 준다.
	struct Gray8
	{
		std::vector<uint8_t> data;
		uint32_t width, height, stride;

		Gray8(uint32_t w, uint32_t h, uint8_t fill)
			: width(w), height(h), stride(((w + 31) / 32) * 32)
		{
			data.assign(static_cast<size_t>(stride) * h, fill);
		}

		void Set(uint32_t x, uint32_t y, uint8_t v)
		{
			data[static_cast<size_t>(y) * stride + x] = v;
		}

		LutRange Range(float clip = 0.001f) const
		{
			return LutTable::ComputeRange(data.data(), width, height, stride, 1, 8, clip);
		}
	};

	struct Gray16
	{
		std::vector<uint8_t> data;
		uint32_t width, height, stride;

		Gray16(uint32_t w, uint32_t h, uint16_t fill)
			: width(w), height(h), stride(((w * 2 + 31) / 32) * 32)
		{
			data.assign(static_cast<size_t>(stride) * h, 0);
			for (uint32_t y = 0; y < h; ++y)
				for (uint32_t x = 0; x < w; ++x)
					Set(x, y, fill);
		}

		void Set(uint32_t x, uint32_t y, uint16_t v)
		{
			auto* row = reinterpret_cast<uint16_t*>(data.data() + static_cast<size_t>(y) * stride);
			row[x] = v;
		}

		LutRange Range(float clip = 0.001f) const
		{
			return LutTable::ComputeRange(data.data(), width, height, stride, 1, 16, clip);
		}
	};

	// 테이블에서 한 항목의 RGB 를 읽는다.
	void Entry(const std::vector<uint8_t>& table, uint32_t i, uint8_t out[3])
	{
		out[0] = table[static_cast<size_t>(i) * 4 + 0];
		out[1] = table[static_cast<size_t>(i) * 4 + 1];
		out[2] = table[static_cast<size_t>(i) * 4 + 2];
	}

	bool IsGrayEntry(const std::vector<uint8_t>& table, uint32_t i)
	{
		uint8_t c[3];
		Entry(table, i, c);
		return c[0] == c[1] && c[1] == c[2];
	}
}

void TestPresetNames()
{
	printf("\n[프리셋]\n");

	Expect(wcscmp(LutTable::GetPresetName(LutPreset::Grayscale), L"Grayscale") == 0, "Grayscale");
	Expect(wcscmp(LutTable::GetPresetName(LutPreset::Inverted), L"Inverted") == 0, "Inverted");
	Expect(wcscmp(LutTable::GetPresetName(LutPreset::Hot), L"Hot") == 0, "Hot");
	Expect(wcscmp(LutTable::GetPresetName(LutPreset::Viridis), L"Viridis") == 0, "Viridis");
	Expect(wcscmp(LutTable::GetPresetName(LutPreset::Jet), L"Jet") == 0, "Jet");

	Expect(LutTable::GetBinCount(8) == 256, "8bit 히스토그램 구간 256");
	Expect(LutTable::GetBinCount(16) == 65536, "16bit 히스토그램 구간 65536");
	Expect(LutTable::GetDomainMax(8) == 255, "8bit 최댓값 255");
	Expect(LutTable::GetDomainMax(16) == 65535, "16bit 최댓값 65535 (65536 이 아니다)");

	// ★ D3D11 의 2D 텍스처 최대 변은 16384 이다.
	// 65536 으로 만들면 CreateTexture2D 가 실패하고 LUT 가 조용히 껐진다.
	// 실제로 그랬고, 8bit 만 동작해서 한참 몰랐다.
	Expect(LutTable::GetTextureEntryCount(8) == 256, "8bit 텍스처 항목 256");
	Expect(LutTable::GetTextureEntryCount(16) <= 16384,
		"16bit 텍스처 항목이 D3D11 한계(16384) 안에 있음");
}

void TestEndpoints()
{
	printf("\n[프리셋 양 끝점]\n");

	uint8_t c[3];

	LutTable::Sample(LutPreset::Grayscale, 0.0f, c);
	Expect(c[0] == 0 && c[1] == 0 && c[2] == 0, "Grayscale t=0 은 검정");
	LutTable::Sample(LutPreset::Grayscale, 1.0f, c);
	Expect(c[0] == 255 && c[1] == 255 && c[2] == 255, "Grayscale t=1 은 흰색 (254 에서 멈추지 않음)");

	LutTable::Sample(LutPreset::Inverted, 0.0f, c);
	Expect(c[0] == 255, "Inverted t=0 은 흰색");
	LutTable::Sample(LutPreset::Inverted, 1.0f, c);
	Expect(c[0] == 0, "Inverted t=1 은 검정");

	LutTable::Sample(LutPreset::Hot, 0.0f, c);
	Expect(c[0] == 0 && c[1] == 0 && c[2] == 0, "Hot t=0 은 검정");
	LutTable::Sample(LutPreset::Hot, 1.0f, c);
	Expect(c[0] == 255 && c[1] == 255 && c[2] == 255, "Hot t=1 은 흰색");

	LutTable::Sample(LutPreset::Viridis, 0.0f, c);
	Expect(c[2] > c[1], "Viridis t=0 은 남보라 (B > G)");
	LutTable::Sample(LutPreset::Viridis, 1.0f, c);
	Expect(c[0] > 200 && c[1] > 200 && c[2] < 100, "Viridis t=1 은 노랑");
}

// Hot 은 휘도가 단조 증가해야 한다. 이게 Jet 대신 Hot 을 기본 컬러맵으로
// 권하는 근거이므로, 회귀로 못 박아 둔다.
void TestHotIsMonotonicLuminance()
{
	printf("\n[Hot 휘도 단조성]\n");

	float prev = -1.0f;
	bool monotonic = true;
	float worstDrop = 0.0f;

	for (int i = 0; i <= 256; ++i)
	{
		uint8_t c[3];
		LutTable::Sample(LutPreset::Hot, i / 256.0f, c);

		// Rec.601 휘도
		const float y = 0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2];

		if (y < prev - 0.5f)
		{
			monotonic = false;
			worstDrop = (worstDrop > prev - y) ? worstDrop : (prev - y);
		}
		prev = y;
	}

	Expect(monotonic, "Hot 은 휘도가 떨어지는 구간이 없다");

	// Jet 은 그렇지 않다. 이 차이가 실제로 존재함을 확인해 둔다.
	prev = -1.0f;
	bool jetMonotonic = true;
	for (int i = 0; i <= 256; ++i)
	{
		uint8_t c[3];
		LutTable::Sample(LutPreset::Jet, i / 256.0f, c);
		const float y = 0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2];
		if (y < prev - 0.5f) jetMonotonic = false;
		prev = y;
	}

	Expect(!jetMonotonic, "Jet 은 휘도가 떨어지는 구간이 있다 (알려진 성질)");
}

void TestRangeRejectsColor()
{
	printf("\n[범위 계산 - 거부 조건]\n");

	std::vector<uint8_t> dummy(4096, 128);

	Expect(!LutTable::ComputeRange(dummy.data(), 16, 16, 64, 3, 8).IsValid(),
		"3채널은 거부 (LUT 는 Gray 전용)");
	Expect(!LutTable::ComputeRange(dummy.data(), 16, 16, 64, 4, 8).IsValid(),
		"4채널은 거부");
	Expect(!LutTable::ComputeRange(nullptr, 16, 16, 64, 1, 8).IsValid(),
		"데이터가 null");
	Expect(!LutTable::ComputeRange(dummy.data(), 0, 16, 64, 1, 8).IsValid(),
		"폭이 0");
	Expect(!LutTable::ComputeRange(dummy.data(), 16, 16, 64, 1, 12).IsValid(),
		"지원하지 않는 비트깊이");
}

void TestPercentileIgnoresOutliers()
{
	printf("\n[퍼센타일이 이상치를 무시하는가]\n");

	// 대부분 100~150 인데 핫픽셀 하나가 255, 데드픽셀 하나가 0.
	// min/max 를 쓰면 범위가 0~255 로 벌어져 스트레치가 무의미해진다.
	Gray8 img(200, 200, 0);

	for (uint32_t y = 0; y < 200; ++y)
		for (uint32_t x = 0; x < 200; ++x)
			img.Set(x, y, static_cast<uint8_t>(100 + (x % 51)));

	img.Set(0, 0, 255);      // 핫픽셀
	img.Set(1, 0, 0);        // 데드픽셀

	const LutRange r = img.Range(0.001f);

	printf("       lo=%u hi=%u\n", r.lo, r.hi);

	Expect(r.IsValid(), "범위를 구함");
	Expect(r.lo >= 100, "데드픽셀(0)에 끌려가지 않음");
	Expect(r.hi <= 150, "핫픽셀(255)에 끌려가지 않음");
}

void TestFlatImageFallsBack()
{
	printf("\n[평탄한 이미지]\n");

	Gray8 img(64, 64, 77);
	const LutRange r = img.Range();

	// 모든 값이 같으면 스트레치할 게 없다. 0으로 나누지 않고 전체 범위로 물러난다.
	printf("       lo=%u hi=%u\n", r.lo, r.hi);
	Expect(r.lo == 0 && r.hi == 255, "평탄하면 전체 범위로 물러남");

	std::vector<uint8_t> table(256 * 4);
	LutTable::Build(LutPreset::Grayscale, 256, 255, r, table.data());

	uint8_t c0[3], c255[3];
	Entry(table, 0, c0);
	Entry(table, 255, c255);
	Expect(c0[0] == 0 && c255[0] == 255, "그래도 정상적인 테이블이 나옴");
}

void TestStretchIsBakedIn()
{
	printf("\n[스트레치가 테이블에 구워지는가]\n");

	LutRange r;
	r.lo = 100;
	r.hi = 150;

	std::vector<uint8_t> table(256 * 4);
	LutTable::Build(LutPreset::Grayscale, 256, 255, r, table.data());

	uint8_t below[3], atLo[3], mid[3], atHi[3], above[3];
	Entry(table, 50, below);
	Entry(table, 100, atLo);
	Entry(table, 125, mid);
	Entry(table, 150, atHi);
	Entry(table, 200, above);

	printf("       [50]=%u [100]=%u [125]=%u [150]=%u [200]=%u\n",
		below[0], atLo[0], mid[0], atHi[0], above[0]);

	Expect(below[0] == 0, "lo 아래는 잘려서 0");
	Expect(atLo[0] == 0, "lo 는 0");
	Expect(atHi[0] == 255, "hi 는 255");
	Expect(above[0] == 255, "hi 위는 잘려서 255");
	Expect(mid[0] > 120 && mid[0] < 135, "중간은 대략 128");

	// 스트레치가 없었다면 [125] 는 125 였을 것이다.
	Expect(mid[0] != 125, "스트레치가 실제로 적용됨 (원본 값과 다름)");
}

void TestBuild16Bit()
{
	printf("\n[16bit 테이블]\n");

	// 16bit 는 이 기능의 존재 이유다. 백버퍼가 8bit 라 LUT 없이는
	// 관심 구간이 몇 계조 안에 갇힌다.
	Gray16 img(300, 300, 0);

	for (uint32_t y = 0; y < 300; ++y)
		for (uint32_t x = 0; x < 300; ++x)
			img.Set(x, y, static_cast<uint16_t>(40000 + (x % 201)));

	const LutRange r = img.Range();
	printf("       lo=%u hi=%u  (원본은 40000~40200 부근)\n", r.lo, r.hi);

	Expect(r.lo >= 39900 && r.lo <= 40100, "lo 가 관심 구간 근처");
	Expect(r.hi >= 40100 && r.hi <= 40300, "hi 가 관심 구간 근처");

	const uint32_t entryCount = LutTable::GetTextureEntryCount(16);
	const uint32_t domainMax = LutTable::GetDomainMax(16);

	std::vector<uint8_t> table(static_cast<size_t>(entryCount) * 4);
	LutTable::Build(LutPreset::Grayscale, entryCount, domainMax, r, table.data());

	// 항목 수가 값 범위보다 작으므로 원본 값을 항목 인덱스로 변환해야 한다.
	auto ToEntry = [&](uint32_t value) -> uint32_t
		{
			const uint64_t idx = static_cast<uint64_t>(value) * (entryCount - 1) / domainMax;
			return static_cast<uint32_t>(idx);
		};

	printf("       텍스처 항목 수 = %u (값 %u개를 덮음)\n", entryCount, domainMax + 1);

	uint8_t atLo[3], atHi[3], atMid[3];
	// 경계는 항목 하나만큼 안쪽으로 들어가서 본다. 항목 하나가 값 4개를
	// 덮으므로 lo/hi 가 정확히 항목 경계에 떨어지지 않을 수 있다.
	Entry(table, ToEntry(r.lo) > 0 ? ToEntry(r.lo) - 1 : 0, atLo);
	Entry(table, ToEntry(r.hi) + 1, atHi);
	Entry(table, ToEntry((r.lo + r.hi) / 2), atMid);

	Expect(atLo[0] == 0, "lo 아래에서 0");
	Expect(atHi[0] == 255, "hi 위에서 255");
	Expect(atMid[0] > 100 && atMid[0] < 155, "중간이 실제로 중간 밝기");

	// 스트레치 없이 그냥 표시하면 40000/65535 -> 156, 40200/65535 -> 157.
	// 200 계조가 1~2 계조로 뭉개진다는 뜻이다.
	const int naiveLo = static_cast<int>(40000.0 / 65535.0 * 255.0);
	const int naiveHi = static_cast<int>(40200.0 / 65535.0 * 255.0);
	printf("       LUT 없이 표시하면 %d ~ %d (계조 %d개)\n",
		naiveLo, naiveHi, naiveHi - naiveLo + 1);

	Expect(naiveHi - naiveLo < 5, "LUT 없이는 이 구간이 몇 계조로 뭉개진다");
}

void TestColorMapsAreNotGray()
{
	printf("\n[컬러맵이 실제로 색을 내는가]\n");

	LutRange r;
	r.lo = 0;
	r.hi = 255;

	std::vector<uint8_t> table(256 * 4);

	LutTable::Build(LutPreset::Grayscale, 256, 255, r, table.data());
	bool allGray = true;
	for (uint32_t i = 0; i < 256; ++i)
		if (!IsGrayEntry(table, i)) allGray = false;
	Expect(allGray, "Grayscale 은 전부 무채색");

	for (LutPreset p : { LutPreset::Hot, LutPreset::Viridis, LutPreset::Jet })
	{
		LutTable::Build(p, 256, 255, r, table.data());

		int coloredCount = 0;
		for (uint32_t i = 0; i < 256; ++i)
			if (!IsGrayEntry(table, i)) ++coloredCount;

		char msg[96];
		sprintf_s(msg, "%ls 는 유채색 항목이 있음 (%d/256)",
			LutTable::GetPresetName(p), coloredCount);
		Expect(coloredCount > 128, msg);
	}
}

void TestAlphaIsOpaque()
{
	printf("\n[알파]\n");

	LutRange r; r.lo = 0; r.hi = 255;
	std::vector<uint8_t> table(256 * 4);
	LutTable::Build(LutPreset::Viridis, 256, 255, r, table.data());

	bool opaque = true;
	for (uint32_t i = 0; i < 256; ++i)
		if (table[static_cast<size_t>(i) * 4 + 3] != 255) opaque = false;

	// 백버퍼가 D2D 타깃(PREMULTIPLIED)과 공유되므로 알파가 1 이 아니면
	// 그 위 오버레이 블렌딩이 틀어진다.
	Expect(opaque, "모든 항목의 알파가 255");
}

int main()
{
	printf("LutTable 회귀 하네스\n");
	printf("================================================\n");

	TestPresetNames();
	TestEndpoints();
	TestHotIsMonotonicLuminance();
	TestRangeRejectsColor();
	TestPercentileIgnoresOutliers();
	TestFlatImageFallsBack();
	TestStretchIsBakedIn();
	TestBuild16Bit();
	TestColorMapsAreNotGray();
	TestAlphaIsOpaque();

	printf("\n================================================\n");
	printf("%s  (%d/%d)\n",
		g_fail == 0 ? "=== ALL PASS ===" : "=== FAILURES ===",
		g_total - g_fail, g_total);

	return g_fail;
}
