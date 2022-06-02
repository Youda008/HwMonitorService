#include "Socket.hpp"
using namespace own;

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

int main( int argc, char * argv [] )
{
	UdpSocket logSocket;

	uint16_t logPort = 28524;
	if (argc > 1)
	{
		if (sscanf( argv[1], "%hu", &logPort ) < 1)
		{
			printf( "invalid port \"%s\", number expected\n", argv[1] ); fflush( stdout );
			return 1;
		}
	}

	printf( "Listening to log messages at UDP port %hu\n", logPort ); fflush( stdout );
	SocketError res = logSocket.open( logPort );
	if (res != SocketError::Success)
	{
		printf( "Failed to open logging socket: %s (error code: %d)\n", enumString( res ), int( logSocket.getLastSystemError() ) ); fflush( stdout );
		return 2;
	}
	
	std::string appTag;
	if (argc > 2)
	{
		appTag = '[' + std::string( argv[2] ) + ']';
		printf( "Showing only messages from %s\n", argv[2] );
	}

	while (true)
	{
		Endpoint from;
		char buffer [1024];
		size_t received;
		res = logSocket.recvFrom( from, make_span( buffer, sizeof(buffer) - 1 ).as_bytes(), received );  // reserve one char for '\0'
		if (res != SocketError::Success)
		{
			printf( "Failed receive log message: %s (error code: %d)\n", enumString( res ), int( logSocket.getLastSystemError() ) ); fflush( stdout );
			continue;
		}
		buffer[ received ] = '\0';

		// display only app entered on command line
		if (!appTag.empty() && strncmp( buffer, appTag.c_str(), appTag.size() ) != 0)
			continue;

		printf( "%s\n", buffer );
	}

	return 0;
}
