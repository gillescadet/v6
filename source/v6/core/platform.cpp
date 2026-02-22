/*V6*/

#include <v6/core/common.h>

#if V6_UE4_PLUGIN == 0

#define V6_OVR_PLATFORM_OCCULUS 0

#if V6_OVR_PLATFORM_OCCULUS == 1
#include <OVR_Platform.h>

#include <v6/core/platform.h>

#define V6_OVR_PLATFORM_NAME "OculusStore"
#endif

BEGIN_V6_NAMESPACE

static bool s_initialized = false;
static char s_appID[256] = {};

bool Platform_Init( const char* platformName, const char* appID )
{
	V6_ASSERT( s_initialized == false );
	V6_ASSERT( s_appID[0] == 0 );

#if V6_OVR_PLATFORM_OCCULUS == 1
	if ( _stricmp( platformName, V6_OVR_PLATFORM_NAME ) != 0 )
	{
		V6_ERROR( "Unable to initialize OVR plaform.\n" );
		return false;
	}
	
	if ( appID == nullptr )
	{
		s_initialized = true;
		V6_WARNING( "OVR platform is disabled.\n" );
		return true;
	}

	if ( ovr_PlatformInitializeWindows( appID ) != ovrPlatformInitialize_Success )
	{
		V6_ERROR( "Unable to initialize OVR plaform.\n" );
		return false;
	}

	ovr_Entitlement_GetIsViewerEntitled();
#endif

	s_initialized = true;
	strcpy_s( s_appID, sizeof( s_appID ), appID );

	V6_MSG( "OVR plaform initialized\n" );

	return true;
}

bool Platform_IsDevelopperMode()
{
	V6_ASSERT( s_initialized );
	
	return s_appID[0] == 0;
}

bool Platform_ProcessMessages()
{
	V6_ASSERT( s_initialized );
	
	if ( !s_appID[0] )
		return true;

#if V6_OVR_PLATFORM_OCCULUS == 1
	for (;;)
	{
		const ovrMessageHandle message = ovr_PopMessage();

		if ( message == nullptr )
			return true;

		switch ( ovr_Message_GetType(message) )
		{
		case ovrMessage_Room_CreateAndJoinPrivate:
			break;
		case ovrMessage_Room_GetCurrent:
			break;
		case ovrMessage_Room_Get:
			break;
		case ovrMessage_Room_Leave:
			break;
		case ovrMessage_Room_Join:
			break;
		case ovrMessage_Room_KickUser:
			break;
		case ovrMessage_User_GetLoggedInUser:
			break;
		case ovrMessage_User_Get:
			break;
		case ovrMessage_User_GetLoggedInUserFriends:
			break;
		case ovrMessage_Room_GetInvitableUsers:
			break;
		case ovrMessage_Room_InviteUser:
			break;
		case ovrMessage_Room_SetDescription:
			break;
		case ovrMessage_Room_UpdateDataStore:
			break;
		case ovrMessage_Notification_Room_RoomUpdate:
			break;
		case ovrMessage_User_GetUserProof:
			break;
		case ovrMessage_User_GetAccessToken:
			break;
		case ovrMessage_Achievements_GetDefinitionsByName:
			break;
		case ovrMessage_Achievements_GetProgressByName:
			break;
		case ovrMessage_Achievements_Unlock:
			break;
		case ovrMessage_Achievements_AddCount:
			break;
		case ovrMessage_Achievements_AddFields:
			break;
		case ovrMessage_Leaderboard_WriteEntry:
			break;
		case ovrMessage_Leaderboard_GetEntries:
			break;
		case ovrMessage_Entitlement_GetIsViewerEntitled:
			if ( ovr_Message_IsError( message ) )
			{
				V6_ERROR( "Could NOT get an entitlement\n" );
				return false;
			}
			break;
		case ovrMessage_CloudStorage_Load:
			break;
		case ovrMessage_CloudStorage_Save:
			break;
		case ovrMessage_CloudStorage_Delete:
			break;
		case ovrMessage_CloudStorage_LoadBucketMetadata:
			break;
		default:
			V6_WARNING( "Unknown OVR platform message %d", ovr_Message_GetType( message ) );
		}

		ovr_FreeMessage( message );
	}
#endif

	return true;
}

void Platform_Shutdown()
{
	V6_ASSERT( s_initialized );

	s_initialized = false;
	s_appID[0] = 0;
}

END_V6_NAMESPACE

#endif // #if V6_UE4_PLUGIN == 0