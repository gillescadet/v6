/*V6*/

#include <v6/baker/common.h>
#include <v6/baker/reader.h>

#include <v6/baker/baker.h>
#include <v6/core/filesystem.h>

BEGIN_V6_BAKER_NAMESPACE

namespace
{

struct SReaderAddFileContext
{
	CReader * m_pReader;
	const char * m_pDirectory;
};

void ReaderAddFile(const char * pFilename, void * pReaderAddFileContext)
{
	SReaderAddFileContext & oReaderAddFileContext = *((SReaderAddFileContext *)pReaderAddFileContext);

	char pFullpath[256];
	sprintf_s(pFullpath, "%s/%s", oReaderAddFileContext.m_pDirectory, pFilename);
	oReaderAddFileContext.m_pReader->addFile(pFullpath);
}

} // namespace anonymous

CReader::CReader(CBaker & oBaker)
: m_oBaker(oBaker)
{
}

CReader::~CReader()
{
}

void CReader::addFiles(const char * pDirectory)
{
	SReaderAddFileContext oReaderAddFileContext;
	oReaderAddFileContext.m_pReader = this;
	oReaderAddFileContext.m_pDirectory = pDirectory;

	char pFilter[256];
	sprintf_s(pFilter, sizeof(pFilter), "%s/frame*.v6t", pDirectory);
	m_oBaker.GetFileSystem().GetFileList(pFilter, ReaderAddFile, &oReaderAddFileContext);
}

void CReader::addFile(const char * pFilename)
{
	printf("Add file: %s\n", pFilename);
}

END_V6_BAKER_NAMESPACE