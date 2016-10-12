/*V6*/

#pragma once

#ifndef __V6_CORE_COMPRESSION_H__
#define __V6_CORE_COMPRESSION_H__

BEGIN_V6_NAMESPACE

struct BitStream_s;
struct Color_s;

struct EncodedBlock_s
{
	u32	cellEndColors;
	u64	cellPresence;
	u64	cellColorIndices;
};

struct EncodedBlockEx_s
{
	u32	cellEndColors;
	u64	cellPresence;
	u64	cellColorIndices[2];
};

struct ImageBlockBC1_s
{
	u16 color0;
	u16 color1;
	u32 bits;
};

void	Block_Decode( u32 cellRGBA[64], u32* cellCount, const EncodedBlockEx_s* encodedBlock );
u32		Block_Encode_BoundingBox( EncodedBlockEx_s* encodedBlock, const u32 cellRGBA[64], u32 cellCount );
u32		Block_Encode_Optimize( EncodedBlockEx_s* encodedBlock, const u32 cellRGBA[64], u32 cellCount, u32 quality );

u32		Block_ComputeBufferMaxSizeForPackingPositions( u32 blockCount );
void	Block_PackPositions( BitStream_s* bitStreamWriter, const u32* blockPos, u32 blockCount );
void	Block_UnpackPositions( BitStream_s* bitStreamReader, u32* blockPos, u32 blockCount );

void	ImageBlock_Decode_BC1( Color_s* pixels, u32 lineStride, const ImageBlockBC1_s* block );
u32		ImageBlock_Encode_BC1( ImageBlockBC1_s* block, const Color_s* pixels, u32 lineStride );

#if 0

template < typename TYPE >
struct HuffmanNode_s
{
	typedef HuffmanNode_s< TYPE > Node_t;

	Node_t*	nextRef;
	Node_t*	leftChild;
	Node_t*	rightChild;
	TYPE	value;
	u32		frequency;
	u32		id;
	u32		bitCount;
	u32		bitMask;
};

template < typename TYPE >
int HuffmanNode_CompareFrequency( const void* nodePointer0, const void* nodePointer1 )
{
	typedef HuffmanNode_s< TYPE > Node_t;

	const Node_t* node0 = (Node_t*)nodePointer0;
	const Node_t* node1 = (Node_t*)nodePointer1;
	return node0->frequency - node1->frequency;
}

template < typename TYPE >
void HuffmanTree_Encode( BitStream_s* bitStream, const TYPE* uniqueValues, const u32* uniqueFrequencies, u32 uniqueCount, const TYPE* values, const u32* uniqueValueIDs, u32 valueCount, IStack* stack )
{
	V6_ASSERT( BitStream_IsAligned( bitStream ) );
	V6_ASSERT( uniqueCount > 0 );

	typedef HuffmanNode_s< TYPE > Node_t;

	ScopedStack scopedStack( stack );

	// Fill leaves

	u32 leafCount = uniqueCount;
	Node_t* nodes = stack->newArray< Node_t >( leafCount * 2 - 1 );
	for ( u32 nodeID = 0; nodeID < leafCount; ++nodeID )
	{
		Node_t* node = &nodes[nodeID];
		node->value = uniqueValues[nodeID];
		node->frequency = uniqueFrequencies[nodeID];
		node->id = nodeID;
		node->nextRef = nullptr;
		node->leftChild = nullptr;
		node->rightChild = nullptr;
		node->bitCount = 0;
		node->bitMask = 0;
	}

	qsort( nodes, leafCount, sizeof( *nodes ), HuffmanNode_CompareFrequency< TYPE > );

	const Node_t** uniqueNodes = stack->newArray< const Node_t* >( leafCount );
	for ( u32 nodeID = 0; nodeID < leafCount; ++nodeID )
	{
		const Node_t* node = &nodes[nodeID];
		uniqueNodes[node->id] = node->frequency > 1 ? node : nullptr;
	}

	Node_t* nodeForFrequencyOfOne = nullptr;
	if ( nodes[0].frequency == 1 )
	{
		u32 frequencyOfOne = 0;
		{
			u32 nodeID;
			for ( nodeID = 0; nodeID < leafCount; ++nodeID )
			{
				if ( nodes[nodeID].frequency > 1 )
					break;
			}
			const u32 nodeOffset = nodeID-1;
			nodes += nodeOffset;
			leafCount -= nodeOffset;
			frequencyOfOne = nodeID;
		}

		const Node_t firstNodeValue = nodes[0];
		
		u32 insertBeforeNodeID;
		for ( insertBeforeNodeID = 1; insertBeforeNodeID < leafCount; ++insertBeforeNodeID )
		{
			if ( nodes[insertBeforeNodeID].frequency >= frequencyOfOne )
				break;
		
			nodes[insertBeforeNodeID-1] = nodes[insertBeforeNodeID];
		}

		nodeForFrequencyOfOne = &nodes[insertBeforeNodeID-1];
		*nodeForFrequencyOfOne = firstNodeValue;
		nodeForFrequencyOfOne->frequency = frequencyOfOne;
	}

	for ( u32 nodeID = 0; nodeID < leafCount-1; ++nodeID )
		nodes[nodeID].nextRef = nodes + nodeID + 1;

	// Build tree

	Node_t* rootNode = nodes;

	{
		Node_t* firstRef = nodes;
		Node_t* freeNodes = nodes + leafCount;

		while ( firstRef->nextRef )
		{
			rootNode = freeNodes;
			++freeNodes;

			rootNode->leftChild = firstRef;
			firstRef = firstRef->nextRef;
			rootNode->rightChild = firstRef;
			firstRef = firstRef->nextRef;

			rootNode->value = 0;
			rootNode->frequency = rootNode->leftChild->frequency + rootNode->rightChild->frequency;
			rootNode->id = 0;
			rootNode->bitCount = 0;
			rootNode->bitMask = 0;
				
			Node_t* prevRef = nullptr;
			Node_t* upperRef;
			for ( upperRef = firstRef; upperRef; prevRef = upperRef, upperRef = upperRef->nextRef )
			{
				if ( rootNode->frequency <= upperRef->frequency )
					break;
			}

			rootNode->nextRef = upperRef;
			if ( prevRef )
				prevRef->nextRef = rootNode;
			else
				firstRef = rootNode;
		}
	}

	// Update leaf bit mask

	const u32 stackMax = 32;
	struct { Node_t* rightChild; u32 bitMask; u32 bitCount; } nodeStack[stackMax];
	u32	bitMaxCount = 0;

	{
		Node_t* node = rootNode;
		u32 stackSize = 0;
		u32 bitMask = 0;
		u32 bitCount = 0;
		for (;;)
		{
			if ( node->leftChild )
			{
				bitMask <<= 1;
				++bitCount;
				V6_ASSERT( bitMask < (1u << bitCount) );
				bitMaxCount = Max( bitMaxCount, bitCount );

				V6_ASSERT( stackSize < stackMax );
				nodeStack[stackSize].rightChild = node->rightChild;
				nodeStack[stackSize].bitMask = bitMask | 1;
				nodeStack[stackSize].bitCount = bitCount;
				++stackSize;
				
				node = node->leftChild;
				continue;
			}

			node->bitMask = bitMask;
			node->bitCount = bitCount;

			if ( stackSize == 0 )
				break;

			--stackSize;
			node = nodeStack[stackSize].rightChild;
			bitMask = nodeStack[stackSize].bitMask;
			bitCount = nodeStack[stackSize].bitCount;
		}
	}

#if 0
	// Build LUT

	const u32 entryCount = 1 << bitMaxCount;
	TYPE* entries = stack->newArray< TYPE >( entryCount );
	
	u32 entryDoneCount = 0;
	for ( u32 nodeID = 0; nodeID < leafCount; ++nodeID )
	{
		const Node_t* node = &nodes[nodeID];
		const u32 bitShift = bitMaxCount - node->bitCount;
		const u32 firstEntryID = node->bitMask << bitShift;
		const u32 subEntryCount = 1 << bitShift;
		const u32 lastEntryID = firstEntryID + subEntryCount - 1;
		for ( u32 entryID = firstEntryID; entryID <= lastEntryID; ++entryID )
			entries[entryID] = node->value;
		entryDoneCount += subEntryCount;
	}
	V6_ASSERT( entryDoneCount == entryCount );
#endif

	// Output to bitstream

	BitStream_Write< 32 >( bitStream, leafCount );
	BitStream_Write< 32 >( bitStream, valueCount );

	{
		TYPE* uniqueValueStream = (TYPE*)BitStream_GetBuffer( bitStream );
		Node_t* node = rootNode;
		u32 stackSize = 0;
		for (;;)
		{
			if ( node->leftChild )
			{
				nodeStack[stackSize].rightChild = node->rightChild;
				V6_ASSERT( stackSize < stackMax );
				++stackSize;
				node = node->leftChild;
				continue;
			}

			*uniqueValueStream = node->value;
			++uniqueValueStream;

			if ( stackSize == 0 )
				break;

			--stackSize;
			node = nodeStack[stackSize].rightChild;
		}
		BitStream_SkipByteCount( bitStream, leafCount * sizeof( TYPE ) );
	}

	{
		Node_t* node = rootNode;
		u32 stackSize = 0;
		for (;;)
		{
			if ( node->leftChild )
			{
				BitStream_Write< 1 >( bitStream, 0u );
				nodeStack[stackSize].rightChild = node->rightChild;
				V6_ASSERT( stackSize < stackMax );
				++stackSize;
				node = node->leftChild;
				continue;
			}

			BitStream_Write< 1 >( bitStream, 1u );

			if ( stackSize == 0 )
				break;

			--stackSize;
			node = nodeStack[stackSize].rightChild;
		}
	}

	BitStream_Align( bitStream );

	for ( u32 valueID = 0; valueID < valueCount; ++valueID )
	{
		const u32 uniqueValueID = uniqueValueIDs[valueID];
		const Node_t* node = uniqueNodes[uniqueValueID] ? uniqueNodes[uniqueValueID] : nodeForFrequencyOfOne;
		V6_ASSERT( node->bitCount >= 0 && node->bitCount <= 32 );
		switch ( node->bitCount )
		{
		case  0: break;
		case  1: BitStream_Write<  1 >( bitStream, node->bitMask ); break;
		case  2: BitStream_Write<  2 >( bitStream, node->bitMask ); break;
		case  3: BitStream_Write<  3 >( bitStream, node->bitMask ); break;
		case  4: BitStream_Write<  4 >( bitStream, node->bitMask ); break;
		case  5: BitStream_Write<  5 >( bitStream, node->bitMask ); break;
		case  6: BitStream_Write<  6 >( bitStream, node->bitMask ); break;
		case  7: BitStream_Write<  7 >( bitStream, node->bitMask ); break;
		case  8: BitStream_Write<  8 >( bitStream, node->bitMask ); break;
		case  9: BitStream_Write<  9 >( bitStream, node->bitMask ); break;
		case 10: BitStream_Write< 10 >( bitStream, node->bitMask ); break;
		case 11: BitStream_Write< 11 >( bitStream, node->bitMask ); break;
		case 12: BitStream_Write< 12 >( bitStream, node->bitMask ); break;
		case 13: BitStream_Write< 13 >( bitStream, node->bitMask ); break;
		case 14: BitStream_Write< 14 >( bitStream, node->bitMask ); break;
		case 15: BitStream_Write< 15 >( bitStream, node->bitMask ); break;
		case 16: BitStream_Write< 16 >( bitStream, node->bitMask ); break;
		case 17: BitStream_Write< 17 >( bitStream, node->bitMask ); break;
		case 18: BitStream_Write< 18 >( bitStream, node->bitMask ); break;
		case 19: BitStream_Write< 19 >( bitStream, node->bitMask ); break;
		case 20: BitStream_Write< 20 >( bitStream, node->bitMask ); break;
		case 21: BitStream_Write< 21 >( bitStream, node->bitMask ); break;
		case 22: BitStream_Write< 22 >( bitStream, node->bitMask ); break;
		case 23: BitStream_Write< 23 >( bitStream, node->bitMask ); break;
		case 24: BitStream_Write< 24 >( bitStream, node->bitMask ); break;
		case 25: BitStream_Write< 25 >( bitStream, node->bitMask ); break;
		case 26: BitStream_Write< 26 >( bitStream, node->bitMask ); break;
		case 27: BitStream_Write< 27 >( bitStream, node->bitMask ); break;
		case 28: BitStream_Write< 28 >( bitStream, node->bitMask ); break;
		case 29: BitStream_Write< 29 >( bitStream, node->bitMask ); break;
		case 30: BitStream_Write< 30 >( bitStream, node->bitMask ); break;
		case 31: BitStream_Write< 31 >( bitStream, node->bitMask ); break;
		case 32: BitStream_Write< 32 >( bitStream, node->bitMask ); break;
		}

		if ( node == nodeForFrequencyOfOne )
			BitStream_Write< sizeof( TYPE ) * 8 >( bitStream, values[valueID] );
	}

	BitStream_Align( bitStream );
}

template < typename TYPE >
static u32 HuffmanTree_ComputeSizeMax( u32 valueCount )
{
	const u32 bitCount = 32 + 32 + (1 + sizeof( TYPE ) * 8 + 32) * (valueCount + 31);
	return 8 * ((bitCount + 63) / 64);
}

template < typename TYPE >
static void HuffmanTree_BuildBatch( BitStream_s* bitStream, const TYPE* values, u32 valueCount, IStack* stack )
{
	for ( u32 hashID = 1; hashID < uniqueMaxCount; ++hashID )
		hashSetOffsets[hashID] = hashSetOffsets[hashID-1] + hashSetCounts[hashID-1];

	V6_ASSERT( hashSetOffsets[65535] + hashSetCounts[65535] == batchCount );

	memset( hashSetUniqueCounts, 0, uniqueMaxCount * sizeof( *hashSetUniqueCounts ) );
	memset( uniqueFrequencies, 0, uniqueMaxCount * sizeof( *uniqueFrequencies ) );

	for ( u32 valueID = firstValueID; valueID <= lastValueID; ++valueID )
	{
		const TYPE value = values[lastValueID];
		const u16 hashKey = Hash_MakeU16( value );

		const u32 hashSetOffset = hashSetOffsets[hashKey];
		const u32 hashSetUniqueCount = hashSetUniqueCounts[hashKey];
		u32 hashSetRank;
		for ( hashSetRank = 0; hashSetRank < hashSetUniqueCount; ++hashSetRank )
			if ( uniqueValues[hashSetOffset + hashSetRank] == value )
				break;

		const u32 uniqueID = hashSetOffset + hashSetRank;

		if ( hashSetRank == hashSetUniqueCount )
		{
			uniqueValues[uniqueID] = value;
			uniqueFrequencies[uniqueID] = 1;
			++hashSetUniqueCounts[hashKey];
		}
		else
		{
			++uniqueFrequencies[uniqueID];
		}

		uniqueValueIDs[valueID] = uniqueID;
	}

	HuffmanTree_Encode< TYPE >( bitStream, uniqueValues, uniqueFrequencies, uniqueCount, values + firstValueID, uniqueValueIDs + firstValueID, lastValueID - firstValueID + 1, stack );
}

template < typename TYPE >
static void HuffmanTree_Build( BitStream_s* bitStream, const TYPE* values, u32 valueCount, IStack* stack )
{
	ScopedStack scopedStack( stack );

	const u32 uniqueMaxCount = 65536;
	TYPE* uniqueValues = stack->newArray< TYPE >( uniqueMaxCount );
	u32* uniqueFrequencies = stack->newArray< u32 >( uniqueMaxCount );
	u32* hashSetCounts = stack->newArray< u32 >( uniqueMaxCount );
	u32* hashSetUniqueCounts = stack->newArray< u32 >( uniqueMaxCount );
	u32* hashSetOffsets = stack->newArray< u32 >( uniqueMaxCount );
	u32* uniqueValueIDs = stack->newArray< u32 >( valueCount );
	
	u32 uniqueCount = 0;
	u32 firstValueID = 0;
	u32 lastValueID = 0;
	while ( firstValueID < valueCount )
	{
		memset( hashSetCounts, 0, uniqueMaxCount * sizeof( *hashSetCounts ) );

		for ( lastValueID = firstValueID; lastValueID < valueCount; ++lastValueID )
		{
			const TYPE value = values[lastValueID];
			const u16 hashKey = Hash_MakeU16( value );

			++hashSetCounts[hashKey];
			++uniqueCount;

			if ( uniqueCount == uniqueMaxCount )
				break;
		}

		const u32 batchCount = lastValueID - firstValueID + 1;
		HuffmanTree_BuildBatch( bitStream, values, batchCount, IStack* stack );
		
		uniqueCount = 0;
		firstValueID = lastValueID + 1;
	}

	if ( uniqueCount > 0 )
	{
		const u32 batchCount = lastValueID - firstValueID + 1;
		HuffmanTree_BuildBatch( bitStream, values, batchCount, IStack* stack );
	}
}

#endif

END_V6_NAMESPACE

#endif // __V6_CORE_COMPRESSION_H__
