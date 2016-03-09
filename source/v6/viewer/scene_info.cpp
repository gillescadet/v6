/*V6*/

#include <v6/viewer/common.h>
#include <v6/viewer/scene_info.h>

#include <v6/core/filesystem.h>
#include <v6/core/stream.h>

BEGIN_V6_VIEWER_NAMESPACE

static bool IsSpaceCar( char c )
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool IsNewLineCar( char c )
{
	return c == '\r' || c == '\n';
}

static char* Trim( char* token )
{
	while ( IsSpaceCar( *token ) ) ++token;
	char *str = token;
	while ( *token ) ++token;
	while ( token != str && IsSpaceCar( *token ) ) --token;
	if ( *token )
	{
		++token;
		*token = 0;
	}
	return str;
}

void SceneInfo_Clear( SceneInfo_s* sceneInfo )
{
	memset( sceneInfo, 0, sizeof( *sceneInfo ) );
	sceneInfo->worldUnitToCM = 1.0f;
}

bool SceneInfo_ReadFromFile( SceneInfo_s* sceneInfo, const char* filename )
{
	SceneInfo_Clear( sceneInfo );

	core::CFileReader fileReader;
	if ( !fileReader.Open( filename ) )
	{
		V6_ERROR( "Unable to open file %s.\n", filename );
		return false;
	}

	char info[4096] = {};
	V6_ASSERT( fileReader.GetSize() <= sizeof( info ) );
	fileReader.Read( fileReader.GetSize(), info );

	int line = 0;
	char *strBegin = info;
	while ( *strBegin )
	{
		char *strEnd = strBegin;
		char *strKey = strBegin;
		char *strIndex = nullptr;
		char *strValue = nullptr;

		while ( *strEnd && !IsNewLineCar( *strEnd ) )
		{
			if ( *strEnd == '#' )
			{
				if ( strIndex != nullptr || strValue != nullptr )
					goto bad_format;
				strIndex = strEnd + 1;
				*strEnd = '\0';
			}
			else if ( *strEnd == ':' )
			{
				if ( strValue != nullptr )
					goto bad_format;
				strValue = strEnd + 1;
				*strEnd = '\0';
			}
			++strEnd;
		}

		char *strNext = strEnd;
		while ( *strNext && IsNewLineCar( *strNext ) )
			++strNext;

		*strEnd = '\0';

		if ( strValue == nullptr )
			goto bad_format;

		strKey = Trim( strKey );
		int index = 0;
		if ( strIndex != nullptr )
			index = atoi( strIndex );
		strValue = Trim( strValue );

		if ( _stricmp( strBegin, "cameraPosition" ) == 0 )
		{
			if ( index < 0 || index >= sceneInfo->MAX_CAMERA_POSITION_COUNT )
				goto bad_format;
			core::Vec3 pos;
			if ( sscanf_s( strValue, "%g %g %g", &pos.x, &pos.y, &pos.z ) != 3 )
				goto bad_format;
			sceneInfo->cameraPositions[index] = pos;
			sceneInfo->cameraPositionCount = core::Max( sceneInfo->cameraPositionCount, (core::u32)index + 1 );
		}
		else if ( _stricmp( strBegin, "cameraYaw" ) == 0 )
		{
			sceneInfo->cameraYaw = (float)atof( strValue );
		}
		else if ( _stricmp( strBegin, "worldUnitToCM" ) == 0 )
		{
			sceneInfo->worldUnitToCM = (float)atof( strValue );
		}
		else
		{
			goto bad_format;
		}

		++line;

		strBegin = strNext;
	}

	return true;

bad_format:
	V6_ERROR( "Error: misformatted info file %s:%d\n", filename, line );
	return false;
}

bool SceneInfo_WriteToFile( const SceneInfo_s* sceneInfo, const char* filename )
{
	if ( !sceneInfo->dirty )
		return true;

	core::CFileWriter fileWriter;
	if ( !fileWriter.Open( filename ) )
	{
		V6_ERROR( "Unable to open file %s.\n", filename );
		return false;
	}

	char info[4096] = {};
	char* str = info;
	core::u32 remainingSize = sizeof( info );

	for ( core::u32 positionID = 0; positionID < sceneInfo->cameraPositionCount; ++positionID )
	{
		const core::Vec3* pos = &sceneInfo->cameraPositions[positionID];
		str += sprintf_s( str, sizeof( info ) - (str-info), "cameraPosition#%d: %g %g %g\n", positionID, pos->x, pos->y, pos->z );
	}
	str += sprintf_s( str, sizeof( info ) - (str-info), "cameraYaw: %g\n", sceneInfo->cameraYaw );
	str += sprintf_s( str, sizeof( info ) - (str-info), "worldUnitToCM: %g\n", sceneInfo->worldUnitToCM );

	fileWriter.Write( info, str-info );

	return false;
}

END_V6_VIEWER_NAMESPACE
