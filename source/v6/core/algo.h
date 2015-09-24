/*V6*/

#pragma once

#ifndef __V6_CORE_ALGO_H__
#define __V6_CORE_ALGO_H__

#include <v6/core/math.h>

BEGIN_V6_CORE_NAMESPACE

template <typename T>
void QuickSort(T * pValues, int nFirst, int nLast)
{
	if (nFirst >= nLast)
	{
		return;
	}

	T const & oPivot = pValues[nFirst];
	int nMin = nFirst;
	int nMax = nLast + 1;
	for (;;)
	{
		do
		{
			++nMin;
		} while (nMin <= nLast && pValues[nMin] <= oPivot);
		do
		{
			--nMax;
		} while (pValues[nMax] > oPivot);
		if (nMin >= nMax)
		{
			break;
		}
		Swap(pValues[nMin], pValues[nMax]);
	}
	Swap(pValues[nFirst], pValues[nMax]);

	QuickSort(pValues, nFirst, nMax - 1);
	QuickSort(pValues, nMax + 1, nLast);
}

template <typename T>
void InsertionSort(T * pValues, int nCount)
{
	if (nCount < 2)
	{
		return;
	}

	for (int n = 1; n < nCount; ++n)
	{
		T nInsert = pValues[n];
		int i = n;
		while (i > 0 && pValues[i - 1] > nInsert)
		{
			pValues[i] = pValues[i - 1];
			--i;
		}
		pValues[i] = nInsert;
	}
}

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_ALGO_H__