/*V6*/

#include <v6/baker/common.h>
#include <v6/baker/tilereader.h>

#include <v6/baker/baker.h>

#include <v6/core/color.h>
#include <v6/core/filesystem.h>
#include <v6/core/image.h>
#include <v6/core/math.h>
#include <v6/core/memory.h>

#include <math.h>

BEGIN_V6_BAKER_NAMESPACE

struct STileExtra;

struct STile
{
	int				m_x;
	int				m_y;
	int				m_w;
	int				m_h;

	float *			GetColorBuffer() { return (float *)(this + 1); }
	int				GetColorBufferSize() { return m_w * m_h * 4 * sizeof(float); }
	float *			GetDepthBuffer() { return (float *)((char *)GetColorBuffer() + GetColorBufferSize()); }
	int				GetDepthBufferSize() { return m_w * m_h * 1 * sizeof(float); }
	int				GetTileSize() { return sizeof(STile) + GetColorBufferSize() + GetDepthBufferSize(); }
	STileExtra *	GetTileExtra() { return (STileExtra *)((char *)this + GetTileSize()); }
};

struct STileExtra
{
	STile * m_next;
};

struct STileReaderAddFileContext
{
	STileReaderAddFileContext()
		: m_nFiles(0)
		, m_nErrors(0)
	{
	}

	CTileReader * m_pTileReader;
	const char * m_pDirectory;
	int m_nFiles;
	int m_nErrors;
};

void TileReaderAddFile(const char * pFilename, void * pTileReaderAddFileContext)
{
	STileReaderAddFileContext & oTileReaderAddFileContext = *((STileReaderAddFileContext *)pTileReaderAddFileContext);

	char pFullpath[256];
	sprintf_s(pFullpath, "%s/%s", oTileReaderAddFileContext.m_pDirectory, pFilename);
	oTileReaderAddFileContext.m_nFiles++;
	oTileReaderAddFileContext.m_nErrors += oTileReaderAddFileContext.m_pTileReader->AddFile(pFullpath) ? 0 : 1;
}

CTileReader::CTileReader(CBaker & oBaker)
	: m_oBaker(oBaker)
	, m_pBlockAllocator(oBaker.GetHeap().newInstance<core::CBlockAllocator>(oBaker.GetHeap()))
	, m_pFirstTile(nullptr)
	, m_fMinDepth(1e10f)
	, m_fMaxDepth(-1e10f)
	, m_nWidth(0)
	, m_nHeight(0)
{
}

CTileReader::~CTileReader()
{
	m_pFirstTile = nullptr;

	m_oBaker.GetHeap().deleteInstance(m_pBlockAllocator);
}

bool CTileReader::AddFiles(const char * pDirectory)
{
	STileReaderAddFileContext oTileReaderAddFileContext;
	oTileReaderAddFileContext.m_pTileReader = this;
	oTileReaderAddFileContext.m_pDirectory = pDirectory;

	char pFilter[256];
	sprintf_s(pFilter, sizeof(pFilter), "%s/frame*.v6t", pDirectory);
	m_oBaker.GetFileSystem().GetFileList(pFilter, TileReaderAddFile, &oTileReaderAddFileContext);

	return oTileReaderAddFileContext.m_nFiles > 0 && oTileReaderAddFileContext.m_nErrors == 0;
}

bool CTileReader::AddFile(const char * pFilename)
{
	FILE * file = NULL;

	if (fopen_s(&file, pFilename, "rb") != 0)
	{
		V6_ERROR("Unable to open file %s", pFilename);
		return false;
	}

	char pMagic[5] = { 0, 0, 0, 0, 0 };
	if (fread(pMagic, 4, 1, file) != 1 || pMagic[0] != 'V' || pMagic[1] != '6' || pMagic[2] != 'T' || pMagic[3] != '0')
	{
		V6_ERROR("Bad magic: %s", pMagic);
		fclose(file);
		return false;
	}

	int nSize = 0;
	if (fread(&nSize, 4, 1, file) != 1 || nSize < sizeof(STile))
	{
		V6_ERROR("Bad size: %d", nSize);
		fclose(file);
		return false;
	}

	STile * pTile = (STile *)m_pBlockAllocator->alloc(nSize + sizeof(STileExtra));

	if (fread(pTile, nSize, 1, file) != 1)
	{
		V6_ERROR("Bad tile");
		fclose(file);
		return false;
	}

	fclose(file);

	if (pTile->GetTileSize() != nSize)
	{
		V6_ERROR("Bad size: %d != %d", pTile->GetTileSize(), nSize);
		return false;
	}

	{
		float * pDepthfs = pTile->GetDepthBuffer();
		float * pDepthf = pDepthfs;
		for (int y = 0; y < pTile->m_h; ++y)
		{
			for (int x = 0; x < pTile->m_w; ++x, ++pDepthf)
			{
				if (*pDepthf < 0.000001f)
				{
					*pDepthf = 1e10f;
				}
				else if (*pDepthf < 1e6f)
				{
					m_fMinDepth = core::CMath::Min(m_fMinDepth, *pDepthf);
					m_fMaxDepth = core::CMath::Max(m_fMaxDepth, *pDepthf);
				}
			}
		}
	}

	STileExtra * pTileExtra = pTile->GetTileExtra();
	pTileExtra->m_next = m_pFirstTile;
	m_pFirstTile = pTile;

	m_nWidth = core::CMath::Max(m_nWidth, pTile->m_x + pTile->m_w);
	m_nHeight = core::CMath::Max(m_nHeight, pTile->m_y + pTile->m_h);

	return true;
}

void CTileReader::FillColorImage(core::CImage & oImage)
{
	if (oImage.GetWidth() != m_nWidth || oImage.GetHeight() != m_nHeight)
	{
		V6_ASSERT(!"Bad size");
		return;
	}

	for (STile * pTile = m_pFirstTile; pTile; pTile = pTile->GetTileExtra()->m_next)
	{
		float * pColorfs = pTile->GetColorBuffer();
		float * pColorf = pColorfs;
		for (int y = 0; y < pTile->m_h; ++y)
		{
			core::SColor * pColor32 = oImage.GetColors() + (pTile->m_y + y) * m_nWidth + pTile->m_x;
			for (int x = 0; x < pTile->m_w; ++x, pColorf += 4, pColor32++)
			{
				static float fInvGamma = 1.0f / 2.2f;
				pColor32->m_r = core::CMath::Clamp((int)(pow(pColorf[0], fInvGamma) * 255), 0, 255);
				pColor32->m_g = core::CMath::Clamp((int)(pow(pColorf[1], fInvGamma) * 255), 0, 255);
				pColor32->m_b = core::CMath::Clamp((int)(pow(pColorf[2], fInvGamma) * 255), 0, 255);
				pColor32->m_a = 255;
			}
		}
	}
}

void CTileReader::FillDepthImage(core::CImage & oImage, float fMinDepth, float fMaxDepth)
{
	if (oImage.GetWidth() != m_nWidth || oImage.GetHeight() != m_nHeight)
	{
		V6_ASSERT(!"Bad size");
		return;
	}

	float const fScaleDepth = 1.0f / (fMaxDepth - fMinDepth);

	for (STile * pTile = m_pFirstTile; pTile; pTile = pTile->GetTileExtra()->m_next)
	{
		float * pDepthfs = pTile->GetDepthBuffer();
		float * pDepthf = pDepthfs;
		for (int y = 0; y < pTile->m_h; ++y)
		{
			core::SColor * pColor32 = oImage.GetColors() + (pTile->m_y + y) * m_nWidth + pTile->m_x;
			for (int x = 0; x < pTile->m_w; ++x, ++pDepthf, pColor32++)
			{
				float const fDepth = core::CMath::Clamp((*pDepthf - fMinDepth) * fScaleDepth, 0.0f, 1.0f);
				core::u8 const nDepth = (int)(fDepth * 255);
				pColor32->m_r = nDepth;
				pColor32->m_g = nDepth;
				pColor32->m_b = nDepth;
				pColor32->m_a = 255;
			}
		}
	}
}

END_V6_BAKER_NAMESPACE