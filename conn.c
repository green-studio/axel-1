
#include "axel.h"

char string[MAX_STRING];

/* Convert an URL to a conn_t structure					*/
int conn_set( conn_t *conn, char *set_url )
{
	char url[MAX_STRING];
	char *i, *j;
	
	/* protocol://							*/
	//处理协议
	if( ( i = strstr( set_url, "://" ) ) == NULL )
	{
		conn->proto = PROTO_DEFAULT;//前边没有http ftp 直接主机名
		strncpy( url, set_url, MAX_STRING );
	}
	else
	{
		if( set_url[0] == 'f' )
			conn->proto = PROTO_FTP;
		else if( set_url[0] == 'h' )
			conn->proto = PROTO_HTTP;
		else
		{
			return( 0 );//其它情况
		}
		strncpy( url, i + 3, MAX_STRING );
	}
	
	/* Split							*/
	//分割
	if( ( i = strchr( url, '/' ) ) == NULL )
	{
		strcpy( conn->dir, "/" );
	}
	else
	{
		*i = 0;
		snprintf( conn->dir, MAX_STRING, "/%s", i + 1 );
	}
	strncpy( conn->host, url, MAX_STRING );
	j = strchr( conn->dir, '?' );	//?后面是参数   xxx.html?a=1
	if( j != NULL )
		*j = 0;
	i = strrchr( conn->dir, '/' );	//最右边的/
	*i = 0;
	if( j != NULL )	//没有?
		*j = '?';
	if( i == NULL )	//url末尾作为文件名
	{
		strcpy( conn->file, conn->dir );
		strcpy( conn->dir, "/" );
	}
	else
	{
		strcpy( conn->file, i + 1 );
		strcat( conn->dir, "/" );
	}
	
	/* Check for username in host field				*/
	//处理用户
	if( strrchr( conn->host, '@' ) != NULL )
	{
		strncpy( conn->user, conn->host, MAX_STRING );
		i = strrchr( conn->user, '@' );
		*i = 0;
		strcpy( conn->host, i + 1 );
		*conn->pass = 0;
	}
	/* If not: Fill in defaults					*/
	else	//没有用户,默认填充
	{
		if( conn->proto == PROTO_FTP )
		{
			/* Dash the password: Save traffic by trying
			   to avoid multi-line responses		*/
			strcpy( conn->user, "anonymous" );
			strcpy( conn->pass, "-lara_hack@gmx.co.uk" );
		}
		else
		{
			*conn->user = *conn->pass = 0;
		}
	}
	
	/* Password?							*/
	if( ( i = strchr( conn->user, ':' ) ) != NULL )
	{
		*i = 0;
		strcpy( conn->pass, i + 1 );
	}
	/* Port number?							*/
	//处理端口
	if( ( i = strchr( conn->host, ':' ) ) != NULL )
	{
		*i = 0;
		sscanf( i + 1, "%i", &conn->port );
	}
	/* Take default port numbers from /etc/services			*/
	else
	{
#ifndef DARWIN
		struct servent *serv;
		
		if( conn->proto == PROTO_FTP )
			serv = getservbyname( "ftp", "tcp" );
		else
			serv = getservbyname( "www", "tcp" );
		
		if( serv )
			conn->port = ntohs( serv->s_port );
		else
#endif
		if( conn->proto == PROTO_HTTP )
			conn->port = 80;
		else
			conn->port = 21;
	}
	
	return( conn->port > 0 );
}

/* Generate a nice URL string.						*/
char *conn_url( conn_t *conn )
{
	if( conn->proto == PROTO_FTP )
		strcpy( string, "ftp://" );
	else
		strcpy( string, "http://" );
	
	if( *conn->user != 0 && strcmp( conn->user, "anonymous" ) != 0 )
		sprintf( string + strlen( string ), "%s:%s@",
			conn->user, conn->pass );

	sprintf( string + strlen( string ), "%s:%i%s%s",
		conn->host, conn->port, conn->dir, conn->file );
	
	return( string );
}

/* Simple...								*/
void conn_disconnect( conn_t *conn )
{
	if( conn->proto == PROTO_FTP && !conn->proxy )
		ftp_disconnect( conn->ftp );
	else
		http_disconnect( conn->http );
	conn->fd = -1;
}

int conn_init( conn_t *conn )
{
	char *proxy = conn->conf->http_proxy, *host = conn->conf->no_proxy;
	int i;
	
	if( *conn->conf->http_proxy == 0 )
	{
		proxy = NULL;
	}
	else if( *conn->conf->no_proxy != 0 )
	{
		for( i = 0; ; i ++ )
			if( conn->conf->no_proxy[i] == 0 )
			{
				if( strstr( conn->host, host ) != NULL )
					proxy = NULL;
				host = &conn->conf->no_proxy[i+1];
				if( conn->conf->no_proxy[i+1] == 0 )
					break;
			}
	}
	
	conn->proxy = proxy != NULL;
	
	if( conn->proto == PROTO_FTP && !conn->proxy )
	{
		conn->ftp->ftp_mode = FTP_PASSIVE;
		if( !ftp_connect( conn->ftp, conn->host, conn->port, conn->user, conn->pass ) )
		{
			conn->message = conn->ftp->message;
			conn_disconnect( conn );
			return( 0 );
		}
		conn->message = conn->ftp->message;
		if( !ftp_cwd( conn->ftp, conn->dir ) )
		{
			conn_disconnect( conn );
			return( 0 );
		}
	}
	else
	{
		if( !http_connect( conn->http, conn->proto, proxy, conn->host, conn->port, conn->user, conn->pass ) )
		{
			conn->message = conn->http->headers;
			conn_disconnect( conn );
			return( 0 );
		}
		conn->message = conn->http->headers;
		conn->fd = conn->http->fd;	//tcp连接的fd
	}
	return( 1 );
}

int conn_setup( conn_t *conn )
{
	if( conn->ftp->fd <= 0 && conn->http->fd <= 0 )
		if( !conn_init( conn ) )
			return( 0 );
	
	if( conn->proto == PROTO_FTP && !conn->proxy )
	{
		if( !ftp_data( conn->ftp ) )	/* Set up data connnection	*/
			return( 0 );
		conn->fd = conn->ftp->data_fd;
		
		if( conn->currentbyte )
		{
			ftp_command( conn->ftp, "REST %i", conn->currentbyte );
			if( ftp_wait( conn->ftp ) / 100 != 3 &&
			    conn->ftp->status / 100 != 2 )
				return( 0 );
		}
	}
	else
	{
		char s[MAX_STRING];
		
		snprintf( s, MAX_STRING, "%s%s", conn->dir, conn->file );
		conn->http->firstbyte = conn->currentbyte;	//当前字节
		conn->http->lastbyte = conn->lastbyte;		//最近字节
		http_get( conn->http, s );
	}
	return( 1 );
}

int conn_exec( conn_t *conn )
{
	if( conn->proto == PROTO_FTP && !conn->proxy )
	{
		if( !ftp_command( conn->ftp, "RETR %s", conn->file ) )
			return( 0 );
		return( ftp_wait( conn->ftp ) / 100 == 1 );
	}
	else
	{
		if( !http_exec( conn->http ) )
			return( 0 );
		return( conn->http->status / 100 == 2 );
	}
}

/* Get file size and other information					*/
int conn_info( conn_t *conn )
{
	/* It's all a bit messed up.. But it works.			*/
	if( conn->proto == PROTO_FTP && !conn->proxy )
	{
		ftp_command( conn->ftp, "REST %i", 1 );
		if( ftp_wait( conn->ftp ) / 100 == 3 ||
		    conn->ftp->status / 100 == 2 )
		{
			conn->supported = 1;
			ftp_command( conn->ftp, "REST %i", 0 );
			ftp_wait( conn->ftp );
		}
		else
		{
			conn->supported = 0;
		}
		
		if( !ftp_cwd( conn->ftp, conn->dir ) )
			return( 0 );
		conn->size = ftp_size( conn->ftp, conn->file, MAX_REDIR );
		if( conn->size < 0 )
			conn->supported = 0;
		if( conn->size == -1 )
			return( 0 );
	}
	else
	{
		char s[MAX_STRING], *t;
		int i = 0;
		
		do
		{
			conn->currentbyte = 1;
			conn_setup( conn );
			conn_exec( conn );
			conn_disconnect( conn );
			/* Code 3xx == redirect				*/
			if( conn->http->status / 100 != 3 )
				break;
			if( ( t = http_header( conn->http, "location:" ) ) == NULL )
				return( 0 );
			sscanf( t, "%s", s );
			if( strstr( s, "://" ) == NULL)
			{
				sprintf( conn->http->headers, "%s%s",
					conn_url( conn ), s );
				strncpy( s, conn->http->headers, MAX_STRING );
			}
			conn_set( conn, s );
			i ++;
		}
		while( conn->http->status / 100 == 3 && i < MAX_REDIR );
		
		if( i == MAX_REDIR )
		{
			sprintf( conn->message, _("Too many redirects.\n") );
			return( 0 );
		}
		
		conn->size = http_size( conn->http );
		if( conn->http->status == 206 && conn->size >= 0 )
		{
			conn->supported = 1;
			conn->size ++;
		}
		else if( conn->http->status == 200 || conn->http->status == 206 )
		{
			conn->supported = 0;
		}
		else
		{
			t = strchr( conn->message, '\n' );
			if( t == NULL )
				sprintf( conn->message, _("Unknown HTTP error.\n") );
			else
				*t = 0;
			return( 0 );
		}
	}
	
	return( 1 );
}

void print_conn(conn_t * conn){
	printf("***********start conn****************\n");
	if(conn)
	{
		printf("conn->proto : %s\n" , conn->proto == 2 ? "HTTP" : "FTP");
		printf("conn->proxy : %d\n" , conn->proxy);
		printf("conn->host : %s\n" , conn->host);
		printf("conn->dir : %s\n" , conn->dir);
		printf("conn->file : %s\n" , conn->file);
		printf("conn->user : %s\n" , conn->user);
		printf("conn->pass : %s\n" , conn->pass);
		/*
			
		*/
		printf("conn->http->host : %s\n" , conn->http->host);
		printf("conn->http->auth : %s\n" , conn->http->auth);
		printf("conn->http->request : %s\n" , conn->http->request);
		printf("conn->http->headers: %s\n" , conn->http->headers);
		printf("conn->http->firstbyte : %d\n" , conn->http->firstbyte);
		printf("conn->http->lastbyte : %d\n" , conn->http->lastbyte);
		printf("conn->http->status : %d\n" , conn->http->status);
		printf("conn->http->fd : %d\n" , conn->http->fd);
	}
	else
	{
		printf("ERROR conn null\n");
	}
	printf("***********end conn****************\n");
}