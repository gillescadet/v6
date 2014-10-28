/*V6*/

#include <v6/baker/common.h>
#include <v6/baker/baker.h>
#include <v6/baker/tilereader.h>

#include <v6/core/image.h>
#include <v6/core/math.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>

int main()
{
	V6_LOG("Baker 0.0");

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

	V6_LOG("Image size  : %dx%d", nWidth, nHeight);
	V6_LOG("Depth range : %g -> %g", fMinDepth, fMaxDepth);

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
	

	return 0;
}