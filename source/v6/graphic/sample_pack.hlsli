/*V6*/

void Sample_Pack( out Sample s, uint3 coords, uint mip, uint3 color )
{
	s.row0 = (coords.x << 20) | (coords.y << 8) | color.r;
	s.row1 = (coords.z << 20) | (color.g << 12) | (color.b << 4) | mip;
}

void SampleOnion_Pack( out Sample s, uint axis, uint sign, uint3 blockCoords, uint3 cellCoords, uint3 color )
{
	s.row0 = (sign << 31) | (axis << 29) | (blockCoords.x << 20) | (blockCoords.y << 11) | (blockCoords.z << 0);
	s.row1 = (color.r << 24) | (color.g << 16) | (color.b << 8) | (cellCoords.x << 4) | (cellCoords.y << 2) | (cellCoords.z << 0);
}

void Sample_Unpack( Sample s, out uint3 coords, out uint mip, out uint3 color )
{
	coords.x = s.row0 >> 20;
	coords.y = (s.row0 >> 8) & 0xFFF;
	coords.z = s.row1 >> 20;
	
	color.r = s.row0 & 0xFF;
	color.g = (s.row1 >> 12) & 0xFF;
	color.b = (s.row1 >> 4) & 0xFF;
		
	mip = s.row1 & 0xF;
}

void SampleOnion_Unpack( Sample s, out uint axis, out uint sign, out uint3 blockCoords, out uint3 cellCoords, out uint3 color )
{
	sign = (s.row0 >> 31) & 1;
	axis = (s.row0 >> 29) & 3;
	
	blockCoords.x = (s.row0 >> 20) & 0x1FF;
	blockCoords.y = (s.row0 >> 11) & 0x1FF;
	blockCoords.z = (s.row0 >>  0) & 0x7FF;

	color.r = (s.row1 >> 24) & 0xFF;
	color.g = (s.row1 >> 16) & 0xFF;
	color.b = (s.row1 >>  8) & 0xFF;

	cellCoords.x = (s.row1 >> 4) & 3;
	cellCoords.y = (s.row1 >> 2) & 3;
	cellCoords.z = (s.row1 >> 0) & 3;
}

uint Sample_UnpackMip( Sample s )
{
	uint3 coords;
	uint mip;
	uint3 color;
	Sample_Unpack( s, coords, mip, color );
	return mip;
}

uint3 Sample_UnpackCoords( Sample s )
{
	uint3 coords;
	uint mip;
	uint3 color;
	Sample_Unpack( s, coords, mip, color );
	return coords;
}

void Sample_UnpackCoordsAndMip( Sample s, out uint3 coords, out uint mip )
{
	uint3 color;
	Sample_Unpack( s, coords, mip, color );
}

void Sample_UnpackColor( Sample s, out uint3 color )
{
	uint3 coords;
	uint mip;
	Sample_Unpack( s, coords, mip, color );
}

void SampleOnion_UnpackCoordsAndFace( Sample s, out uint3 coords, out uint face )
{
	uint axis;
	uint sign;
	uint3 blockCoords;
	uint3 cellCoords;
	uint3 color;

	SampleOnion_Unpack( s, axis, sign, blockCoords, cellCoords, color );

	coords = (blockCoords << 2) | cellCoords;
	face = (sign << 2) | axis;
}

void SampleOnion_UnpackColor( Sample s, out uint3 color )
{
	uint axis;
	uint sign;
	uint3 blockCoords;
	uint3 cellCoords;

	SampleOnion_Unpack( s, axis, sign, blockCoords, cellCoords, color );
}
