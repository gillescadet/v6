/*V6*/

#include <v6/baker/common.h>
#include <v6/baker/baker.h>
#include <v6/baker/reader.h>

int main()
{
	v6::baker::CBaker oBaker;
	v6::baker::CReader oReader(oBaker);
	oReader.addFiles("d:/data/v6");
	return 0;
}