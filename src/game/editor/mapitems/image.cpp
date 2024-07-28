#include "image.h"

#include <game/mapitems.h>

CEditorImage::CEditorImage(CEditor *pEditor) :
	m_AutoMapper(pEditor)
{
	OnInit(pEditor);
	m_Texture.Invalidate();
}

CEditorImage::~CEditorImage()
{
	Graphics()->UnloadTexture(&m_Texture);
	free(m_pData);
	m_pData = nullptr;
}

void CEditorImage::OnInit(CEditor *pEditor)
{
	CEditorComponent::OnInit(pEditor);
	RegisterSubComponent(m_AutoMapper);
	InitSubComponents();
}

void CEditorImage::AnalyseTileFlags()
{
	mem_zero(m_aTileFlags, sizeof(m_aTileFlags));

	size_t tw = m_Width / 16; // tilesizes
	size_t th = m_Height / 16;
	if(tw == th && m_Format == CImageInfo::FORMAT_RGBA)
	{
		int TileId = 0;
		for(size_t ty = 0; ty < 16; ty++)
			for(size_t tx = 0; tx < 16; tx++, TileId++)
			{
				bool Opaque = true;
				for(size_t x = 0; x < tw; x++)
					for(size_t y = 0; y < th; y++)
					{
						size_t p = (ty * tw + y) * m_Width + tx * tw + x;
						if(m_pData[p * 4 + 3] < 250)
						{
							Opaque = false;
							break;
						}
					}

				if(Opaque)
					m_aTileFlags[TileId] |= TILEFLAG_OPAQUE;
			}
	}
}

bool CEditorImage::DataEquals(const CEditorImage &Other) const
{
	// If height, width or pixel size don't match, then data cannot be equal
	const size_t ImgPixelSize = PixelSize();

	if(Other.m_Height != m_Height || Other.m_Width != m_Width || Other.PixelSize() != ImgPixelSize)
		return false;

	const auto &&GetPixel = [&](uint8_t *pData, int x, int y, size_t p) -> uint8_t {
		return pData[x * ImgPixelSize + (m_Width * ImgPixelSize * y) + p];
	};

	// Look through every pixel and check if there are any difference
	for(size_t y = 0; y < m_Height; y += ImgPixelSize)
	{
		for(size_t x = 0; x < m_Width; x += ImgPixelSize)
		{
			for(size_t p = 0; p < ImgPixelSize; p++)
				if(GetPixel(m_pData, x, y, p) != GetPixel(Other.m_pData, x, y, p))
					return false;
		}
	}

	return true;
}
