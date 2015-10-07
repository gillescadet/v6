/*V6*/

#pragma once

#ifndef __V6_CORE_GRID_H__
#define __V6_CORE_GRID_H__

BEGIN_V6_CORE_NAMESPACE

class IAllocator;
struct SBox;
struct Vec3;

class CGrid
{
public:
	CGrid(IAllocator & oHeap);
	~CGrid();

	void Clear();
	void Build(SBox const * pBoxes, int nBoxes, float fAverageObjectPerCell);
	void Build(Vec3 const * pPoints, int nPoints, float fAverageObjectPerCell);
	void PrintBuildStatistics() const;

public:
	class CBuildState;

private:
	IAllocator & m_oHeap;
	CBuildState * m_pBuildState;
};

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_GRID_H__