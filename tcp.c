

#include "axel.h"

/* Get a TCP connection */
int tcp_connect( char *hostname, int port )
{
	struct hostent *host = NULL;
	struct sockaddr_in addr;
	int fd;

#ifdef DEBUG
	struct sockaddr_in local;
	int i = sizeof( local );
	
	fprintf( stderr, "tcp_connect( %s, %i ) = ", hostname, port );
#endif
	
	/* Why this loop? Because the call might return an empty record.
	   At least it very rarely does, on my system...		*/
	for( fd = 0; fd < 3; fd ++ )
	{
		if( ( host = gethostbyname( hostname ) ) == NULL )
			return( -1 );
		if( *host->h_name ) break;
	}
	
	if( ( fd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 )
		return( -1 );
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons( port );
		addr.sin_addr = *( (struct in_addr *) host->h_addr );
	
	if( connect( fd, (struct sockaddr *) &addr, sizeof( struct sockaddr_in ) ) == -1 )
	{
		close( fd );
		return( -1 );
	}
	
#ifdef DEBUG
	getsockname( fd, &local, &i );
	fprintf( stderr, "%i\n", ntohs( local.sin_port ) );
#endif
	
	return( fd );
}
