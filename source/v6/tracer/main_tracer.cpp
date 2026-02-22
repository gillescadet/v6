/*V6*/

#include <v6/core/common.h>

#include <v6/core/plot.h>
#include <v6/core/random.h>
#include <v6/core/vec3.h>
#include <v6/core/vec3i.h>

BEGIN_V6_NAMESPACE

static Plot_s s_plot;

struct uint64
{
	u32 low;
	u32 high;
};

//----------------------------------------------------------------------------------------------------

void OutputMessage( u32 msgType, const char * format, ... )
{
	char buffer[4096];
	va_list args;
	va_start( args, format );
	vsprintf_s( buffer, sizeof( buffer ), format, args);
	va_end( args );

	fputs( buffer, stdout );
}

//----------------------------------------------------------------------------------------------------

template < typename T1, typename T2, typename T3 >
T1 mad( const T1& v0, const T2& v1, const T3& v2 )
{
	return v0 * v1 + v2;
}

template < typename T >
T saturate( const T& v0 )
{
	return Clamp( v0, Vec3_Zero(), Vec3_One() );
}

bool BoxTest( Vec3 rayDir, Vec3 boxMinRS, Vec3 boxMaxRS )
{
	const Vec3 rayInvDir = rayDir.Rcp();
	const Vec3 t0 = boxMinRS * rayInvDir;
	const Vec3 t1 = boxMaxRS * rayInvDir;
	const Vec3 tMin = Min( t0, t1 );
	const Vec3 tMax = Max( t0, t1 );
	const float tIn = Max( Max( tMin.x, tMin.y ), tMin.z );
	const float tOut = Min( Min( tMax.x, tMax.y ), tMax.z );
	
	return tIn > 0.0f && tIn < tOut;
}

u32 Trace( Vec3 rayDir, Vec3 boxMinWS, Vec3 boxMinRS, Vec3 boxMaxRS, uint64 blockPresence, float cellSize, float invCellSize, bool usePlot )
{
	const Vec3 rayInvDir = rayDir.Rcp();
	const Vec3 t0 = boxMinRS * rayInvDir;
	const Vec3 t1 = boxMaxRS * rayInvDir;
	const Vec3 tMin = Min( t0, t1 );
	const Vec3 tMax = Max( t0, t1 );
	const float tIn = Max( Max( tMin.x, tMin.y ), tMin.z );
	const float tOut = Min( Min( tMax.x, tMax.y ), tMax.z );
	
	if ( tIn > 0.0f && tIn < tOut )
	{
		u32 cellIDs[16];
		u32 cellCount = 0;

		const Vec3 coordIn = mad( rayDir, tIn, -boxMinRS ) * invCellSize;
		const Vec3 coordClamped = Clamp( Vec3_Make( floorf( coordIn.x ), floorf( coordIn.y ), floorf( coordIn.z ) ), Vec3_Zero(), Vec3_Make( 3.0f, 3.0f, 3.0f ) );
		Vec3u coords = Vec3u_Make( (u32)coordClamped.x, (u32)coordClamped.y, (u32)coordClamped.z );

		const Vec3 tDelta = cellSize * Abs( rayInvDir );

		const Vec3 tOffsetPos = coordClamped * tDelta;
		const Vec3 tOffsetNeg = mad( tDelta, 3.0f, -tOffsetPos );

		Vec3 tCur = tMin;
		tCur.x += rayDir.x < 0.0f ? tOffsetNeg.x : tOffsetPos.x;
		tCur.y += rayDir.y < 0.0f ? tOffsetNeg.y : tOffsetPos.y;
		tCur.z += rayDir.z < 0.0f ? tOffsetNeg.z : tOffsetPos.z;

		Vec3i coordsStep;
		coordsStep.x = rayDir.x < 0.0f ? -1 : 1;
		coordsStep.y = rayDir.y < 0.0f ? -1 : 1;
		coordsStep.z = rayDir.z < 0.0f ? -1 : 1;
		
		do
		{
			const u32 cellID = mad( coords.z, 16, mad( coords.y, 4, coords.x ) );
			V6_ASSERT( cellID < 64 );

			cellIDs[cellCount++] = cellID;

			const u32 isCellHigh = cellID >> 5;
			const u32 isPresenceLow = (blockPresence.low >> cellID) & 1;
			const u32 isPresenceHigh = (blockPresence.high >> (cellID & 0x1F)) & 1;
			if ( (isPresenceLow & ~isCellHigh) | (isPresenceHigh & isCellHigh) )
			{
				if ( usePlot)
					Plot_NewObject( &s_plot, Color_Blue() );

				for ( u32 cellRank = 0; cellRank < cellCount; ++cellRank )
				{
					const u32 x = (cellIDs[cellRank] >> 0) & 3;
					const u32 y = (cellIDs[cellRank] >> 2) & 3;
					const u32 z = (cellIDs[cellRank] >> 4) & 3;
					
					Vec3 cellMinWS = boxMinWS;
					cellMinWS.x += x * cellSize;
					cellMinWS.y += y * cellSize;
					cellMinWS.z += z * cellSize;

					const Vec3 cellMaxWS = cellMinWS + cellSize;

					if ( usePlot)
						Plot_AddBox( &s_plot, &cellMinWS, &cellMaxWS, false );
				}
				
				return cellID;
			}
			
			const Vec3 tNext = tCur + tDelta;
			
			const bool nextAxisIsX = (tNext.x < tNext.y) & (tNext.x < tNext.z);
			const bool nextAxisIsYIfNotX = tNext.y < tNext.z;
			const bool nextAxisIsY = !nextAxisIsX & nextAxisIsYIfNotX;
			const bool nextAxisIsZ = !nextAxisIsX & !nextAxisIsYIfNotX;
			
			tCur.x = nextAxisIsX ? tNext.x : tCur.x;
			tCur.y = nextAxisIsY ? tNext.y : tCur.y;
			tCur.z = nextAxisIsZ ? tNext.z : tCur.z;

			coords.x += nextAxisIsX ? coordsStep.x : 0;
			coords.y += nextAxisIsY ? coordsStep.y : 0;
			coords.z += nextAxisIsZ ? coordsStep.z : 0;

		} while ( (coords.x < 4) & (coords.y < 4) & (coords.z < 4) );
	}

	return (u32)-1;
}

void Test( u32 testID, bool usePlot )
{
	const float cellSize = 0.5f + 0.5f * RandFloat();
	const float blockSize = cellSize * 4.0f;
	const float invCellSize = 1.0f / cellSize;

	const Vec3 boxMinWS = RandSphere();// + Vec3_Make( testID * 10.0f, 0.0f, 0.0f );
	const Vec3 boxMaxWS = boxMinWS + blockSize;

	u32 cellCount = (u32)Max( 4.0f, RandFloat() * 2.0f );
	u64 cellPresence = 0;
	while ( cellCount )
	{
		const u32 cellID = rand() & 0x3F;
		if ( cellPresence & (1ull << cellID) )
			continue;

		cellPresence |= (1ull << cellID);
		--cellCount;
	}

	if ( usePlot )
	{
		Plot_NewObject( &s_plot, Color_Black() );
		Plot_AddBox( &s_plot, &boxMinWS, &boxMaxWS, true );
	}

	for ( u32 z = 0; z < 4; ++z )
	{
		for ( u32 y = 0; y < 4; ++y )
		{
			for ( u32 x = 0; x < 4; ++x )
			{
				const u32 cellID = z * 16 + y * 4 + x;
				if ( cellPresence & (1ull << cellID) )
				{
					Vec3 cellMinWS = boxMinWS;
					cellMinWS.x += x * cellSize;
					cellMinWS.y += y * cellSize;
					cellMinWS.z += z * cellSize;

					const Vec3 cellMaxWS = cellMinWS + cellSize;

					if ( usePlot)
						Plot_AddBox( &s_plot, &cellMinWS, &cellMaxWS, true );
				}
			}
		}
	}

#if 1

	const Vec3 boxCenterWS = (boxMinWS + boxMaxWS) * 0.5f;
	
	for ( u32 rayID = 0; rayID < 8; )
	{
		const Vec3 rayOrg = boxCenterWS + RandSphere() * blockSize * 2.0f;
		const Vec3 rayTarget = boxCenterWS + RandSphere() * blockSize * 2.0f;
		Vec3 rayDir = rayTarget - rayOrg;

		if ( rayDir.Normalize() < cellSize )
			continue;

		if ( usePlot)
		{
			Plot_NewObject( &s_plot, Color_White() );
			Plot_AddLine( &s_plot, &rayOrg, &rayTarget );
		}

		u32 checkCellID = (u32)-1;
		float tCheckCellID = FLT_MAX;
		for ( u32 z = 0; z < 4; ++z )
		{
			for ( u32 y = 0; y < 4; ++y )
			{
				for ( u32 x = 0; x < 4; ++x )
				{
					const u32 cellID = z * 16 + y * 4 + x;
					if ( cellPresence & (1ull << cellID) )
					{
						Vec3 cellMinWS = boxMinWS;
						cellMinWS.x += x * cellSize;
						cellMinWS.y += y * cellSize;
						cellMinWS.z += z * cellSize;

						const Vec3 cellMaxWS = cellMinWS + cellSize;

						const Vec3 cellMinRS = cellMinWS - rayOrg;
						const Vec3 cellMaxRS = cellMaxWS - rayOrg;

						if ( BoxTest( rayDir, cellMinRS, cellMaxRS ) )
						{
							const float t = Dot( rayDir, cellMinRS );
							if ( t < tCheckCellID )
							{
								checkCellID = cellID;
								tCheckCellID = t;
							}
						}
					}
				}
			}
		}
		
		const Vec3 boxMinRS = boxMinWS - rayOrg;
		const Vec3 boxMaxRS = boxMaxWS - rayOrg;

		const u32 cellID = Trace( rayDir, boxMinWS, boxMinRS, boxMaxRS, *((uint64*)&cellPresence), cellSize, invCellSize, usePlot );

		if ( cellID != checkCellID )
		{
			if ( checkCellID == (u32)-1 )
				V6_ERROR( "[%02d] Ray %02d incorrectly hits cellID %02d\n", testID, rayID, cellID );
			else
				V6_ERROR( "[%02d] Ray %02d misses cellID %02d\n", testID, rayID, checkCellID );
		}

		++rayID;
	}
#endif
}

void TestAll()
{
	Plot_Create( &s_plot, "d:/tmp/plot/testTrace" );

	const u32 testCount = 10000;
	for ( u32 testID = 0; testID < testCount; ++testID )
		Test( testID, testID == 1 );

	V6_MSG( "Done %d tests\n", testCount );


	Plot_Release( &s_plot );
}

//----------------------------------------------------------------------------------------------------

END_V6_NAMESPACE

int main()
{
	V6_MSG( "Tracer Test 0.0\n" );

	v6::TestAll();

	return 0;
}
