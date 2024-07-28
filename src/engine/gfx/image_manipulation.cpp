#include "image_manipulation.h"
#include <base/math.h>
#include <base/system.h>

static constexpr int DILATE_BPP = 4; // RGBA assumed
static constexpr uint8_t DILATE_ALPHA_THRESHOLD = 10;

static void Dilate(int w, int h, const uint8_t *pSrc, uint8_t *pDest)
{
	const int aDirX[] = {0, -1, 1, 0};
	const int aDirY[] = {-1, 0, 0, 1};

	int m = 0;
	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++, m += DILATE_BPP)
		{
			for(int i = 0; i < DILATE_BPP; ++i)
				pDest[m + i] = pSrc[m + i];
			if(pSrc[m + DILATE_BPP - 1] > DILATE_ALPHA_THRESHOLD)
				continue;

			int aSumOfOpaque[] = {0, 0, 0};
			int Counter = 0;
			for(int c = 0; c < 4; c++)
			{
				const int ClampedX = clamp(x + aDirX[c], 0, w - 1);
				const int ClampedY = clamp(y + aDirY[c], 0, h - 1);
				const int SrcIndex = ClampedY * w * DILATE_BPP + ClampedX * DILATE_BPP;
				if(pSrc[SrcIndex + DILATE_BPP - 1] > DILATE_ALPHA_THRESHOLD)
				{
					for(int p = 0; p < DILATE_BPP - 1; ++p)
						aSumOfOpaque[p] += pSrc[SrcIndex + p];
					++Counter;
					break;
				}
			}

			if(Counter > 0)
			{
				for(int i = 0; i < DILATE_BPP - 1; ++i)
				{
					aSumOfOpaque[i] /= Counter;
					pDest[m + i] = (uint8_t)aSumOfOpaque[i];
				}

				pDest[m + DILATE_BPP - 1] = 255;
			}
		}
	}
}

static void CopyColorValues(int w, int h, const uint8_t *pSrc, uint8_t *pDest)
{
	int m = 0;
	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++, m += DILATE_BPP)
		{
			if(pDest[m + DILATE_BPP - 1] == 0)
			{
				mem_copy(&pDest[m], &pSrc[m], DILATE_BPP - 1);
			}
		}
	}
}

void DilateImage(uint8_t *pImageBuff, int w, int h)
{
	DilateImageSub(pImageBuff, w, h, 0, 0, w, h);
}

void DilateImageSub(uint8_t *pImageBuff, int w, int h, int x, int y, int sw, int sh)
{
	uint8_t *apBuffer[2] = {nullptr, nullptr};

	const size_t ImageSize = (size_t)sw * sh * sizeof(uint8_t) * DILATE_BPP;
	apBuffer[0] = (uint8_t *)malloc(ImageSize);
	apBuffer[1] = (uint8_t *)malloc(ImageSize);
	uint8_t *pBufferOriginal = (uint8_t *)malloc(ImageSize);

	for(int Y = 0; Y < sh; ++Y)
	{
		int SrcImgOffset = ((y + Y) * w * DILATE_BPP) + (x * DILATE_BPP);
		int DstImgOffset = (Y * sw * DILATE_BPP);
		int CopySize = sw * DILATE_BPP;
		mem_copy(&pBufferOriginal[DstImgOffset], &pImageBuff[SrcImgOffset], CopySize);
	}

	Dilate(sw, sh, pBufferOriginal, apBuffer[0]);

	for(int i = 0; i < 5; i++)
	{
		Dilate(sw, sh, apBuffer[0], apBuffer[1]);
		Dilate(sw, sh, apBuffer[1], apBuffer[0]);
	}

	CopyColorValues(sw, sh, apBuffer[0], pBufferOriginal);

	free(apBuffer[0]);
	free(apBuffer[1]);

	for(int Y = 0; Y < sh; ++Y)
	{
		int SrcImgOffset = ((y + Y) * w * DILATE_BPP) + (x * DILATE_BPP);
		int DstImgOffset = (Y * sw * DILATE_BPP);
		int CopySize = sw * DILATE_BPP;
		mem_copy(&pImageBuff[SrcImgOffset], &pBufferOriginal[DstImgOffset], CopySize);
	}

	free(pBufferOriginal);
}

static float CubicHermite(float A, float B, float C, float D, float t)
{
	float a = -A / 2.0f + (3.0f * B) / 2.0f - (3.0f * C) / 2.0f + D / 2.0f;
	float b = A - (5.0f * B) / 2.0f + 2.0f * C - D / 2.0f;
	float c = -A / 2.0f + C / 2.0f;
	float d = B;

	return (a * t * t * t) + (b * t * t) + (c * t) + d;
}

static void GetPixelClamped(const uint8_t *pSourceImage, int x, int y, uint32_t W, uint32_t H, size_t BPP, uint8_t aSample[4])
{
	x = clamp<int>(x, 0, (int)W - 1);
	y = clamp<int>(y, 0, (int)H - 1);

	mem_copy(aSample, &pSourceImage[x * BPP + (W * BPP * y)], BPP);
}

static void SampleBicubic(const uint8_t *pSourceImage, float u, float v, uint32_t W, uint32_t H, size_t BPP, uint8_t aSample[4])
{
	float X = (u * W) - 0.5f;
	int xInt = (int)X;
	float xFract = X - std::floor(X);

	float Y = (v * H) - 0.5f;
	int yInt = (int)Y;
	float yFract = Y - std::floor(Y);

	uint8_t aaaSamples[4][4][4];
	for(int y = 0; y < 4; ++y)
	{
		for(int x = 0; x < 4; ++x)
		{
			GetPixelClamped(pSourceImage, xInt + x - 1, yInt + y - 1, W, H, BPP, aaaSamples[x][y]);
		}
	}

	for(size_t i = 0; i < BPP; i++)
	{
		float aRows[4];
		for(int y = 0; y < 4; ++y)
		{
			aRows[y] = CubicHermite(aaaSamples[0][y][i], aaaSamples[1][y][i], aaaSamples[2][y][i], aaaSamples[3][y][i], xFract);
		}
		aSample[i] = (uint8_t)clamp<float>(CubicHermite(aRows[0], aRows[1], aRows[2], aRows[3], yFract), 0.0f, 255.0f);
	}
}

static void ResizeImage(const uint8_t *pSourceImage, uint32_t SW, uint32_t SH, uint8_t *pDestinationImage, uint32_t W, uint32_t H, size_t BPP)
{
	for(int y = 0; y < (int)H; ++y)
	{
		float v = (float)y / (float)(H - 1);
		for(int x = 0; x < (int)W; ++x)
		{
			float u = (float)x / (float)(W - 1);
			uint8_t aSample[4];
			SampleBicubic(pSourceImage, u, v, SW, SH, BPP, aSample);
			mem_copy(&pDestinationImage[x * BPP + ((W * BPP) * y)], aSample, BPP);
		}
	}
}

uint8_t *ResizeImage(const uint8_t *pImageData, int Width, int Height, int NewWidth, int NewHeight, int BPP)
{
	uint8_t *pTmpData = (uint8_t *)malloc((size_t)NewWidth * NewHeight * BPP);
	ResizeImage(pImageData, Width, Height, pTmpData, NewWidth, NewHeight, BPP);
	return pTmpData;
}

int HighestBit(int OfVar)
{
	if(!OfVar)
		return 0;

	int RetV = 1;

	while(OfVar >>= 1)
		RetV <<= 1;

	return RetV;
}
