/*V6*/

#include <v6/core/common.h>
#include <v6/core/grid.h>

#include <v6/core/box.h>
#include <v6/core/math.h>
#include <v6/core/memory.h>
#include <v6/core/vec3.h>

BEGIN_V6_NAMESPACE

class CGrid::CBuildState
{
public:
	CGrid::CBuildState(IAllocator & oHeap)
		: m_oAllocator(oHeap)
	{
	}

	CBlockAllocator m_oAllocator;
	SBox			m_oBox;
	Vec3			m_vPoint2CoordsOffset;
	Vec3			m_vPoint2CoordsScale;
	int				m_vCellCount[3];
	Vec3			m_vCellSize;
	int *			m_pGridCounters;
	int *			m_pGridOffsets;
	int *			m_pGridReferences;
	float			m_fEpsilon;
	int				m_nZOffset;
	int				m_nYOffset;
};

V6_INLINE void ConvertPoint2Coords(int * vCoords, Vec3 const & vPoint, CGrid::CBuildState const & oBuildState)
{
	Vec3 const vDiscretPoint = (vPoint + oBuildState.m_vPoint2CoordsOffset) * oBuildState.m_vPoint2CoordsScale;
	vCoords[0] = Clamp((int)vDiscretPoint.x, 0, oBuildState.m_vCellCount[0] - 1);
	vCoords[1] = Clamp((int)vDiscretPoint.y, 0, oBuildState.m_vCellCount[1] - 1);
	vCoords[2] = Clamp((int)vDiscretPoint.z, 0, oBuildState.m_vCellCount[2] - 1);
}

CGrid::CGrid(IAllocator & oHeap)
: m_oHeap(oHeap)
, m_pBuildState(nullptr)
{
}

CGrid::~CGrid()
{
	Clear();
}

void CGrid::Clear()
{
	if (m_pBuildState != nullptr)
	{
		m_oHeap.deleteInstance(m_pBuildState);
		m_pBuildState = nullptr;
	}
}

void CGrid::Build(SBox const * pBoxes, int nBoxes, float fAverageObjectPerCell)
{
	Clear();
	if (!nBoxes)
	{
		return;
	}

	m_pBuildState = m_oHeap.newInstance<CBuildState>(m_oHeap);

	/// Compute global bounding box
	for (int nObjectId = 0; nObjectId < nBoxes; ++nObjectId)
	{
		m_pBuildState->m_oBox.Extend(pBoxes[nObjectId]);
	}

	/// Compute grid discretisation	
	Vec3 const vExtent = m_pBuildState->m_oBox.Size();
	m_pBuildState->m_fEpsilon = vExtent.Length() * 0.00001f;
	m_pBuildState->m_vPoint2CoordsOffset = -m_pBuildState->m_oBox.m_vMin;
	Vec3 vNormalizedExtent = vExtent.Normalized();
	const float fK = Pow(nBoxes / (fAverageObjectPerCell * vNormalizedExtent.x * vNormalizedExtent.y * vNormalizedExtent.z), 0.333333f);
	const Vec3 vCellCount = vNormalizedExtent * fK + 0.5f;
	for (int i = 0; i < 3; ++i)
	{
		m_pBuildState->m_vCellCount[i] = vCellCount[i] < 1.0f ? 1 : (int)(vCellCount[i]);
		m_pBuildState->m_vCellSize[i] = vExtent[i] / m_pBuildState->m_vCellCount[i];
		m_pBuildState->m_vPoint2CoordsScale[i] = m_pBuildState->m_vCellCount[i] / vExtent[i];
	}
	
	/// Reset counters
	int const nCellCount = m_pBuildState->m_vCellCount[0] * m_pBuildState->m_vCellCount[1] * m_pBuildState->m_vCellCount[2];
	int const nCounterBufferSize = nCellCount * sizeof(int);
	m_pBuildState->m_pGridCounters = (int *)m_pBuildState->m_oAllocator.alloc(nCounterBufferSize);
	memset(m_pBuildState->m_pGridCounters, 0, nCounterBufferSize);
	m_pBuildState->m_nYOffset = m_pBuildState->m_vCellCount[0];
	m_pBuildState->m_nZOffset = m_pBuildState->m_vCellCount[0] * m_pBuildState->m_vCellCount[1];

	/// Count objects
	int nReferenceCount = 0;
	for (int nObjectId = 0; nObjectId < nBoxes; ++nObjectId)
	{
		SBox const & oBox = pBoxes[nObjectId];
		int vMinRange[3];
		int vMaxRange[3];
		ConvertPoint2Coords(vMinRange, oBox.m_vMin, *m_pBuildState);
		ConvertPoint2Coords(vMaxRange, oBox.m_vMax, *m_pBuildState);

		for (int z = vMinRange[2]; z <= vMaxRange[2]; ++z)
		{
			int* counterZY =
				m_pBuildState->m_pGridCounters +
				z * m_pBuildState->m_nZOffset +
				vMinRange[1] * m_pBuildState->m_nYOffset;
			for (int y = vMinRange[1]; y <= vMaxRange[1]; ++y, counterZY += m_pBuildState->m_nYOffset)
			{
				int* counter = counterZY + vMinRange[0];
				for (int x = vMinRange[0]; x <= vMaxRange[0]; ++x, ++counter)
				{
					++*counter;
					++nReferenceCount;
				}
			}
		}
	}

	/// Compute offsets
	m_pBuildState->m_pGridOffsets = (int *)m_pBuildState->m_oAllocator.alloc(nCounterBufferSize);
	m_pBuildState->m_pGridOffsets[0] = 0;
	for (int i = 1; i < nCellCount; ++i)
	{
		m_pBuildState->m_pGridOffsets[i] = m_pBuildState->m_pGridOffsets[i - 1] + m_pBuildState->m_pGridCounters[i - 1];
	}

	/// Add object references
	m_pBuildState->m_pGridReferences = (int *)m_pBuildState->m_oAllocator.alloc(nReferenceCount * sizeof(int));
	memset(m_pBuildState->m_pGridCounters, 0, nCounterBufferSize);
	for (int nObjectId = 0; nObjectId < nBoxes; ++nObjectId)
	{
		SBox const & oBox = pBoxes[nObjectId];
		int vMinRange[3];
		int vMaxRange[3];
		ConvertPoint2Coords(vMinRange, oBox.m_vMin, *m_pBuildState);
		ConvertPoint2Coords(vMaxRange, oBox.m_vMax, *m_pBuildState);

		for (int z = vMinRange[2]; z <= vMaxRange[2]; ++z)
		{
			const int zyOffset = z * m_pBuildState->m_nZOffset + vMinRange[1] * m_pBuildState->m_nYOffset;
			int* counterZY = m_pBuildState->m_pGridCounters + zyOffset;
			const int* offsetZY = m_pBuildState->m_pGridOffsets + zyOffset;
			for (int y = vMinRange[1]; y <= vMaxRange[1]; ++y, counterZY += m_pBuildState->m_nYOffset, offsetZY += m_pBuildState->m_nYOffset)
			{
				int* counter = counterZY + vMinRange[0];
				const int* offset = offsetZY + vMaxRange[0];
				for (int x = vMinRange[0]; x <= vMaxRange[0]; ++x, ++counter, ++offset)
				{
					m_pBuildState->m_pGridReferences[*offset + (*counter)++] = nObjectId;
				}
			}
		}
	}
}

void CGrid::Build(Vec3 const * pPoints, int nPoints, float fAverageObjectPerCell)
{
	Clear();
	if (!nPoints)
	{
		return;
	}

	m_pBuildState = m_oHeap.newInstance<CBuildState>(m_oHeap);

	/// Compute global bounding box
	for (int nObjectId = 0; nObjectId < nPoints; ++nObjectId)
	{
		m_pBuildState->m_oBox.Extend(pPoints[nObjectId]);
	}

	/// Compute grid discretisation	
	Vec3 const vExtent = m_pBuildState->m_oBox.Size();
	m_pBuildState->m_fEpsilon = vExtent.Length() * 0.00001f;
	m_pBuildState->m_vPoint2CoordsOffset = -m_pBuildState->m_oBox.m_vMin;
	Vec3 vNormalizedExtent = vExtent.Normalized();
	const float fK = Pow(nPoints / (fAverageObjectPerCell * vNormalizedExtent.x * vNormalizedExtent.y * vNormalizedExtent.z), 0.333333f);
	const Vec3 vCellCount = vNormalizedExtent * fK + 0.5f;
	for (int i = 0; i < 3; ++i)
	{
		m_pBuildState->m_vCellCount[i] = vCellCount[i] < 1.0f ? 1 : (int)(vCellCount[i]);
		m_pBuildState->m_vCellSize[i] = vExtent[i] / m_pBuildState->m_vCellCount[i];
		m_pBuildState->m_vPoint2CoordsScale[i] = m_pBuildState->m_vCellCount[i] / vExtent[i];
	}

	/// Reset counters
	int const nCellCount = m_pBuildState->m_vCellCount[0] * m_pBuildState->m_vCellCount[1] * m_pBuildState->m_vCellCount[2];
	int const nCounterBufferSize = nCellCount * sizeof(int);
	m_pBuildState->m_pGridCounters = (int *)m_pBuildState->m_oAllocator.alloc(nCounterBufferSize);
	memset(m_pBuildState->m_pGridCounters, 0, nCounterBufferSize);
	m_pBuildState->m_nYOffset = m_pBuildState->m_vCellCount[0];
	m_pBuildState->m_nZOffset = m_pBuildState->m_vCellCount[0] * m_pBuildState->m_vCellCount[1];

	/// Count objects
	for (int nObjectId = 0; nObjectId < nPoints; ++nObjectId)
	{
		Vec3 const & vPoint = pPoints[nObjectId];
		int vCoords[3];
		ConvertPoint2Coords(vCoords, vPoint, *m_pBuildState);

		int const nOffset =
			vCoords[2] * m_pBuildState->m_nZOffset +
			vCoords[1] * m_pBuildState->m_nYOffset +
			vCoords[0];

		++m_pBuildState->m_pGridCounters[nOffset];
	}

	/// Compute offsets
	m_pBuildState->m_pGridOffsets = (int *)m_pBuildState->m_oAllocator.alloc(nCounterBufferSize);
	m_pBuildState->m_pGridOffsets[0] = 0;
	for (int i = 1; i < nCellCount; ++i)
	{
		m_pBuildState->m_pGridOffsets[i] = m_pBuildState->m_pGridOffsets[i - 1] + m_pBuildState->m_pGridCounters[i - 1];
	}

	/// Add object references
	m_pBuildState->m_pGridReferences = (int *)m_pBuildState->m_oAllocator.alloc(nPoints * sizeof(int));
	for (int nObjectId = 0; nObjectId < nPoints; ++nObjectId)
	{
		Vec3 const & vPoint = pPoints[nObjectId];
		int vCoords[3];
		ConvertPoint2Coords(vCoords, vPoint, *m_pBuildState);

		int const nOffset = 
			vCoords[2] * m_pBuildState->m_nZOffset +
			vCoords[1] * m_pBuildState->m_nYOffset +
			vCoords[0];

		m_pBuildState->m_pGridReferences[m_pBuildState->m_pGridOffsets[nOffset]] = nObjectId;
	}
}

void CGrid::PrintBuildStatistics() const
{
	V6_MSG( "Grid Statistics:\n" );
	if ( m_pBuildState )
	{
		int const nCellCount = m_pBuildState->m_vCellCount[0] * m_pBuildState->m_vCellCount[1] * m_pBuildState->m_vCellCount[2];
		int nReferenceCount = 0;
		int nEmptyCellCount = 0;
		int nMaxCellCount = 0;
		
		for (int i = 0; i < nCellCount; ++i)
		{
			nReferenceCount += m_pBuildState->m_pGridCounters[i];
			nEmptyCellCount += m_pBuildState->m_pGridCounters[i] == 0;
			nMaxCellCount = Max(nMaxCellCount, m_pBuildState->m_pGridCounters[i]);
		}

		V6_MSG( "- cell count: %d (%dx%dx%d)\n", nCellCount, m_pBuildState->m_vCellCount[0], m_pBuildState->m_vCellCount[1], m_pBuildState->m_vCellCount[2]);
		V6_MSG( "- reference count: %d\n", nReferenceCount);
		V6_MSG( "- empty cell count: %d\n", nEmptyCellCount);
		V6_MSG( "- max cell count: %d\n", nMaxCellCount);
	}
	else
	{
		V6_MSG( "- no data\n" );
	}
}

END_V6_NAMESPACE
