void Onion_BlockCoordsToPos( out float3 blockPosMinRS, out float3 blockPosMaxRS, uint sign, uint axis, uint3 blockCoords, float gridMinScale, float invMacroPeriodWidth, float invMacroGridWidth )
{
	float3 blockPosMinGS, blockPosMaxGS;

	blockPosMinGS.z = gridMinScale * exp2( (blockCoords.z + 0.0f) * invMacroPeriodWidth );
	blockPosMaxGS.z = gridMinScale * exp2( (blockCoords.z + 1.0f) * invMacroPeriodWidth );
	blockPosMinGS.xy = mad( blockCoords.xy + 0.0f, invMacroGridWidth * 2.0f, -1.0f ) * blockPosMaxGS.z;
	blockPosMaxGS.xy = mad( blockCoords.xy + 1.0f, invMacroGridWidth * 2.0f, -1.0f ) * blockPosMaxGS.z;

	const float signedMinZ = sign ? -blockPosMaxGS.z : blockPosMinGS.z;
	const float signedMaxZ = sign ? -blockPosMinGS.z : blockPosMaxGS.z;

	if ( axis == 0 )
	{
		blockPosMinRS.x = signedMinZ;
		blockPosMinRS.y = blockPosMinGS.x;
		blockPosMinRS.z = blockPosMinGS.y;

		blockPosMaxRS.x = signedMaxZ;
		blockPosMaxRS.y = blockPosMaxGS.x;
		blockPosMaxRS.z = blockPosMaxGS.y;
	}
	else if ( axis == 1 )
	{
		blockPosMinRS.y = signedMinZ;
		blockPosMinRS.z = blockPosMinGS.x;
		blockPosMinRS.x = blockPosMinGS.y;

		blockPosMaxRS.y = signedMaxZ;
		blockPosMaxRS.z = blockPosMaxGS.x;
		blockPosMaxRS.x = blockPosMaxGS.y;
	}
	else
	{
		blockPosMinRS.z = signedMinZ;
		blockPosMinRS.x = blockPosMinGS.x;
		blockPosMinRS.y = blockPosMinGS.y;

		blockPosMaxRS.z = signedMaxZ;
		blockPosMaxRS.x = blockPosMaxGS.x;
		blockPosMaxRS.y = blockPosMaxGS.y;
	}
}