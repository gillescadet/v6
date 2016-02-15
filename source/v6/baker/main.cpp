/*V6*/

#include <v6/baker/common.h>
#include <v6/baker/baker.h>
#include <v6/baker/tilereader.h>

#include <v6/core/frame_manager.h>
#include <v6/core/image.h>
#include <v6/core/math.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>

int main()
{
	V6_MSG( "Baker 0.0\n" );

	v6::core::CHeap oHeap;
	v6::baker::CBaker oBaker(oHeap);
	v6::baker::CTileReader oTileReader(oBaker);
	if (!oTileReader.AddFiles("d:/data/v6"))
	{
		return 1;
	}

	int nWidth, nHeight;
	oTileReader.GetImageSize(nWidth, nHeight);

	float fMinDepth, fMaxDepth;
	oTileReader.GetDepthRange(fMinDepth, fMaxDepth);

	V6_MSG( "Image size  : %dx%d\n", nWidth, nHeight);
	V6_MSG( "Depth range : %g -> %g\n", fMinDepth, fMaxDepth);

#if 1
	{
		v6::core::CImage oImage(oHeap, nWidth, nHeight);
		{
			oTileReader.FillColorImage(oImage);
			v6::core::CFileWriter oFileWriter;
			oFileWriter.Open("d:/data/v6/frameColor0.bmp");
			oImage.WriteBitmap(oFileWriter);
			oFileWriter.Close();
		}
		{
			oTileReader.FillDepthImage(oImage, fMinDepth, fMaxDepth);
			v6::core::CFileWriter oFileWriter;
			oFileWriter.Open("d:/data/v6/frameDepth0.bmp");
			oImage.WriteBitmap(oFileWriter);
			oFileWriter.Close();
		}
	}
#endif

	{
		v6::core::FrameManager frameManager( &oHeap );
		v6::core::FrameDesc desc = { (v6::core::uint)nWidth, (v6::core::uint)nHeight };
		v6::core::FrameBuffer* frameBuffer = frameManager.CreateFrameBuffer( &desc );
		
		oTileReader.FillFrameBuffer( frameBuffer );

		v6::core::CFileWriter oFileWriter;
		oFileWriter.Open("d:/data/v6/frameBuffer0.frm");
		oFileWriter.Write( "V6F0", 4 );
		oFileWriter.Write( &desc, sizeof(desc) );
		oFileWriter.Write( frameBuffer->colors, v6::core::FrameManager::GetFrameBufferColorSize( &desc ) );
		oFileWriter.Write( frameBuffer->depths, v6::core::FrameManager::GetFrameBufferDepthSize( &desc ) );
		oFileWriter.Close();
	}

	

	return 0;
}