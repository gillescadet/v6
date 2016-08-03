struct Ouput
{
	uint	bits;
	uint	data[4];
};

Buffer< uint > inputs;
RWStructuredBuffer< Ouput > ouputs;

[ numthreads( 1, 1, 1 ) ]
void main()
{
	// Read inputs
	
	const uint offset = inputs[0] & 7;
	const uint width = inputs[1] & 7;
	
	// Build a bit mask

#if 1
	// method 1: bad ASM generation
	
	const uint mask1 = (1 << offset) - 1;
	const uint mask2 = (1 << (offset + width)) - 1;
	const uint range = mask2 & ~mask1;
	
#else
	// method 2: good ASM generation
	
	const uint range = ((1 << width) - 1) << offset;
#endif
	
	// Test	
	
	const bool cond = countbits( range ) == width;

	if ( !cond )
	{
		uint prev;
		InterlockedOr( ouputs[0].bits, 1, prev );
		if ( prev == 0 )
		{
			ouputs[0].data[0] = cond;
			ouputs[0].data[1] = range;
			ouputs[0].data[2] = width;
			ouputs[0].data[3] = countbits( range );
		}
	}
}
