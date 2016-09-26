#include "axel.h"

/* Axel */
static void save_state( axel_t *axel );
static void *setup_thread( void * );
static void axel_message( axel_t *axel, char *format, ... );
static void axel_divide( axel_t *axel );
extern void print_conn(conn_t * conn);
void print_axel(axel_t *axel);
static char *buffer = NULL;

extern void print_conn(conn_t * conn);
/* Create a new axel_t structure					*/
axel_t *axel_new( conf_t *conf, int count, void *url )
{
	search_t *res;
	axel_t *axel;
	url_t *u;
	char *s;
	int i;
	
	axel = malloc( sizeof( axel_t ) );
	memset( axel, 0, sizeof( axel_t ) );
	*axel->conf = *conf;
	axel->conn = malloc( sizeof( conn_t ) * axel->conf->num_connections );
	memset( axel->conn, 0, sizeof( conn_t ) * axel->conf->num_connections );
	/*
	axel OK!
	axel->filename :
	print_axel(axel);
	*/
	if( axel->conf->max_speed > 0 )
	{
		if( (float) axel->conf->max_speed / axel->conf->buffer_size < 0.5 )
		{
			if( axel->conf->verbose >= 2 )
				axel_message( axel, _("Buffer resized for this speed.") );
			axel->conf->buffer_size = axel->conf->max_speed;
		}
		axel->delay_time = (int) ( (float) 1000000 / axel->conf->max_speed * axel->conf->buffer_size * axel->conf->num_connections );
	}
	printf("malloc size : %d\n" , sizeof(axel_t) + sizeof(conn_t));
	if( buffer == NULL )
	{
		buffer = malloc( max( MAX_STRING, axel->conf->buffer_size ) );
		printf("buffer OK...\n");;
	}
	printf("count = %d\n" , count);
	if( count == 0 )
	{
		axel->url = malloc( sizeof( url_t ) );
		axel->url->next = axel->url;
		strcpy( axel->url->text, (char *) url );
	}
	else
	{
		res = (search_t *) url;
		u = axel->url = malloc( sizeof( url_t ) );
		printf("----start print url list\n");
		for( i = 0; i < count; i ++ )
		{
			strcpy( u->text, res[i].url );
			if( i < count - 1 )
			{
				u->next = malloc( sizeof( url_t ) );
				u = u->next;
			}
			else
			{
				u->next = axel->url;
			}
			printf("url[%d] : %s\n" , i , u->text);
		}
		printf("----end print url list\n");
	}
//	print_axel(axel);
	axel->conn[0].conf = axel->conf;
	/*
	 * 根据端口信息判断是否设置成功
	 * */
	if( !conn_set( &axel->conn[0], axel->url->text ) )
	{
		axel_message( axel, _("Could not parse URL.\n") );
		axel->ready = -1;
		return( axel );
	}
	//指向真正的url
	axel->url = axel->url->next;
	
	strcpy( axel->filename, axel->conn[0].file );
	if( *axel->filename == 0 )	/* Index page == no fn		*/
		strcpy( axel->filename, axel->conf->default_filename );
	/*
	 * 文件名有?
	 * */
	if( ( s = strchr( axel->filename, '?' ) ) != NULL && axel->conf->strip_cgi_parameters )
		*s = 0;		/* Get rid of CGI parameters		*/
	
	if( !conn_init( &axel->conn[0] ) )
	{
		axel_message( axel, axel->conn[0].message );
		axel->ready = -1;
		return( axel );
	}
	
	/* This does more than just checking the file size, it all depends
	   on the protocol used.					*/
	/*
	 * 获取文件大小
	 * */
	if( !conn_info( &axel->conn[0] ) )
	{
		axel_message( axel, axel->conn[0].message );
		axel->ready = -1;
		return( axel );
	}
	if( ( axel->size = axel->conn[0].size ) != INT_MAX )
	{
		if( axel->conf->verbose > 0 )
			axel_message( axel, _("File size: %i bytes"), axel->size );
	}
	
	/* Wildcards in URL --> Get complete filename			*/
	if( strchr( axel->filename, '*' ) || strchr( axel->filename, '?' ) )
		strcpy( axel->filename, axel->conn[0].file );
	
	return( axel );
}

/* Open a local file to store the downloaded data			*/
int axel_open( axel_t *axel )
{
	int i, fd;
	
	if( axel->conf->verbose > 0 )
		axel_message( axel, _("Opening output file %s"), axel->filename );
	snprintf( buffer, MAX_STRING, "%s.st", axel->filename );
	
	axel->outfd = -1;
	
	/* Check whether server knows about RESTart and switch back to
	   single connection download if necessary			*/
	if( !axel->conn[0].supported )
	{
		axel_message( axel, _("Server unsupported, "
			"starting from scratch with one connection.") );
		axel->conf->num_connections = 1;
		axel->conn = realloc( axel->conn, sizeof( conn_t ) );
		axel_divide( axel );
	}
	/*
		XXX.st  此处完成断点续传
	*/
	else if( ( fd = open( buffer, O_RDONLY ) ) != -1 )
	{
		read( fd, &axel->conf->num_connections, sizeof( axel->conf->num_connections ) );
		/*
		 * 根据原来的connection数,重新分配,还原到原来设置的
		 * */
		axel->conn = realloc( axel->conn, sizeof( conn_t ) * axel->conf->num_connections );
		memset( axel->conn + 1, 0, sizeof( conn_t ) * ( axel->conf->num_connections - 1 ) );

		axel_divide( axel );
		//已下载的字节数
		read( fd, &axel->bytes_done, sizeof( axel->bytes_done ) );
		for( i = 0; i < axel->conf->num_connections; i ++ )
			read( fd, &axel->conn[i].currentbyte, sizeof( axel->conn[i].currentbyte ) );

		axel_message( axel, _("State file found: %i bytes downloaded, %i to go."),
			axel->bytes_done, axel->size - axel->bytes_done );
		
		close( fd );
		//打开已经下载的部分文件
		if( ( axel->outfd = open( axel->filename, O_WRONLY, 0666 ) ) == -1 )
		{
			axel_message( axel, _("Error opening local file") );
			return( 0 );
		}
	}
	//没有未下载完的部分文件,
	/* If outfd == -1 we have to start from scrath now		*/
	if( axel->outfd == -1 )
	{
		//默认初始化conn
		axel_divide( axel );
		
		if( ( axel->outfd = open( axel->filename, O_CREAT | O_WRONLY, 0666 ) ) == -1 )
		{
			axel_message( axel, _("Error opening local file") );
			return( 0 );
		}
		
		/* And check whether the filesystem can handle seeks to
		   past-EOF areas.. Speeds things up. :) AFAIK this
		   should just not happen:				*/
		if( lseek( axel->outfd, axel->size, SEEK_SET ) == -1 && axel->conf->num_connections > 1 )
		{
			/* But if the OS/fs does not allow to seek behind
			   EOF, we have to fill the file with zeroes before
			   starting. Slow..				*/
			axel_message( axel, _("Crappy filesystem/OS.. Working around. :-(") );
			lseek( axel->outfd, 0, SEEK_SET );
			memset( buffer, 0, axel->conf->buffer_size );
			i = axel->size;
			while( i > 0 )
			{
				write( axel->outfd, buffer, min( i, axel->conf->buffer_size ) );
				i -= axel->conf->buffer_size;
			}
		}
	}
	
	return( 1 );
}

/* Start downloading							*/
//开启多线程
void axel_start( axel_t *axel )
{
	int i;
	
	/* HTTP might've redirected and FTP handles wildcards, so
	   re-scan the URL for every conn				*/
	for( i = 1; i < axel->conf->num_connections; i ++ )
	{
		conn_set( &axel->conn[i], axel->url->text );
		axel->url = axel->url->next;
		axel->conn[i].conf = axel->conf;
		axel->conn[i].supported = 1;
	}
	
	if( axel->conf->verbose > 0 )
		axel_message( axel, _("Starting download") );
	
	for( i = 0; i < axel->conf->num_connections; i ++ )
	if( axel->conn[i].currentbyte <= axel->conn[i].lastbyte )
	{
		if( axel->conf->verbose >= 2 )
			axel_message( axel, _("Connection %i downloading from %s:%i"),
			              i, axel->conn[i].host, axel->conn[i].port );
		if( pthread_create( axel->conn[i].setup_thread, NULL, setup_thread, &axel->conn[i] ) != 0 )
		{
			axel_message( axel, _("pthread error!!!") );
			axel->ready = -1;
		}
		else
		{
			axel->conn[i].last_transfer = gettime();
			axel->conn[i].state = 1;
		}
	}
	
	/* The real downloading will start now, so let's start counting	*/
	axel->start_time = gettime();
	axel->ready = 0;
}

/* Main 'loop'								*/
//最核心的函数
void axel_do( axel_t *axel )
{
	fd_set fds[1];
	int hifd, i, j, size;
	struct timeval timeval[1];
	
	/* Create statefile if necessary				*/
	if( gettime() > axel->next_state )
	{
		save_state( axel );
		axel->next_state = gettime() + axel->conf->save_state_interval;
	}
	
	/* Wait for data on (one of) the connections			*/
	FD_ZERO( fds );
	hifd = 0;
	//设置fd set,找出最大的fd  conn->fd载创建线程时建立，由HTTP->fd确定.conn_init里面
	//若是ftp则直接使用tcp->fd,但是两者都是tcp->fd
	for( i = 0; i < axel->conf->num_connections; i ++ )
	{
		if( axel->conn[i].enabled )
			FD_SET( axel->conn[i].fd, fds );
		hifd = max( hifd, axel->conn[i].fd );
//		printf("axel->conn[%d].fd : %d  enable : %d\n" , i , axel->conn[i].fd , axel->conn[i].enabled);
	}
	if( hifd == 0 )
	{
		/* No connections yet. Wait...				*/
		usleep( 100000 );
		goto conn_check;
	}
	else
	{
		timeval->tv_sec = 0;
		timeval->tv_usec = 100000;
		/* A select() error probably means it was interrupted
		   by a signal, or that something else's very wrong...	*/
		//select处理各个fd,看其是否活跃
		if( select( hifd + 1, fds, NULL, NULL, timeval ) == -1 )
		{
			axel->ready = -1;
			return;
		}
	}
	
	/* Handle connections which need attention			*/
	for( i = 0; i < axel->conf->num_connections; i ++ )
	if( axel->conn[i].enabled ) {
	if( FD_ISSET( axel->conn[i].fd, fds ) )
	{
		axel->conn[i].last_transfer = gettime();
		size = read( axel->conn[i].fd, buffer, axel->conf->buffer_size );
		if( size == -1 )  //出错
		{
			if( axel->conf->verbose )
			{
				axel_message( axel, _("Error on connection %i! "
					"Connection closed"), i );
			}
			axel->conn[i].enabled = 0;
			conn_disconnect( &axel->conn[i] );
			continue;
		}
		else if( size == 0 )  //完成
		{
			if( axel->conf->verbose )
			{
				/* Only abnormal behaviour if:		*/
				if( axel->conn[i].currentbyte < axel->conn[i].lastbyte && axel->size != INT_MAX )
				{
					axel_message( axel, _("Connection %i unexpectedly closed"), i );
				}
				else
				{
					axel_message( axel, _("Connection %i finished"), i );
				}
			}
			if( !axel->conn[0].supported )
			{
				axel->ready = 1;
			}
			axel->conn[i].enabled = 0;
			conn_disconnect( &axel->conn[i] );
			continue;
		}
		/* j == Bytes to go					*/
		j = axel->conn[i].lastbyte - axel->conn[i].currentbyte + 1;
		if( j < size )
		{
			if( axel->conf->verbose )
			{
				axel_message( axel, _("Connection %i finished"), i );
			}
			axel->conn[i].enabled = 0;
			conn_disconnect( &axel->conn[i] );
			size = j;
			/* Don't terminate, still stuff to write!	*/
		}
		/* This should always succeed..				*/
		lseek( axel->outfd, axel->conn[i].currentbyte, SEEK_SET );
		//写入输出的文件,更改下载信息。
		if( write( axel->outfd, buffer, size ) != size )
		{
			
			axel_message( axel, _("Write error!") );
			axel->ready = -1;
			return;
		}
		axel->conn[i].currentbyte += size;
		axel->bytes_done += size;
	}
	else  //超时
	{
		if( gettime() > axel->conn[i].last_transfer + axel->conf->connection_timeout )
		{
			if( axel->conf->verbose )
				axel_message( axel, _("Connection %i timed out"), i );
			conn_disconnect( &axel->conn[i] );
			axel->conn[i].enabled = 0;
		}
	} }
	
	if( axel->ready )
		return;
	
conn_check:
	/* Look for aborted connections and attempt to restart them.	*/
	for( i = 0; i < axel->conf->num_connections; i ++ )
	{
		if( !axel->conn[i].enabled && axel->conn[i].currentbyte < axel->conn[i].lastbyte )
		{
			if( axel->conn[i].state == 0 )
			{
				conn_set( &axel->conn[i], axel->url->text );
				axel->url = axel->url->next;
				if( axel->conf->verbose >= 2 )
					axel_message( axel, _("Connection %i downloading from %s:%i"),
				        	      i, axel->conn[i].host, axel->conn[i].port );
				if( pthread_create( axel->conn[i].setup_thread, NULL, setup_thread, &axel->conn[i] ) == 0 )
				{
					axel->conn[i].state = 1;
					axel->conn[i].last_transfer = gettime();
				}
				else
				{
					axel_message( axel, _("pthread error!!!") );
					axel->ready = -1;
				}
			}
			else
			{
				if( gettime() > axel->conn[i].last_transfer + axel->conf->reconnect_delay )
				{
					pthread_cancel( *axel->conn[i].setup_thread );
					axel->conn[i].state = 0;
				}
			}
		}
	}

	/* Calculate current average speed and finish_time		*/
	axel->bytes_per_second = (int) ( (double) ( axel->bytes_done - axel->start_byte ) / ( gettime() - axel->start_time ) );
	axel->finish_time = (int) ( axel->start_time + (double) ( axel->size - axel->start_byte ) / axel->bytes_per_second );

	/* Check speed. If too high, delay for some time to slow things
	   down a bit. I think a 5% deviation should be acceptable.	*/
	if( axel->conf->max_speed > 0 )
	{
		if( (float) axel->bytes_per_second / axel->conf->max_speed > 1.05 )
			axel->delay_time += 10000;
		else if( ( (float) axel->bytes_per_second / axel->conf->max_speed < 0.95 ) && ( axel->delay_time >= 10000 ) )
			axel->delay_time -= 10000;
		else if( ( (float) axel->bytes_per_second / axel->conf->max_speed < 0.95 ) )
			axel->delay_time = 0;
		usleep( axel->delay_time );
	}
	
	/* Ready?							*/
	if( axel->bytes_done == axel->size )
		axel->ready = 1;
}

/* Close an axel connection						*/
void axel_close( axel_t *axel )
{
	int i;
	message_t *m;
	
	/* Delete state file if necessary				*/
	if( axel->ready == 1 )
	{
		snprintf( buffer, MAX_STRING, "%s.st", axel->filename );
		unlink( buffer );
	}
	/* Else: Create it.. 						*/
	else if( axel->bytes_done > 0 )
	{
		save_state( axel );
	}

	/* Delete any message not processed yet				*/
	while( axel->message )
	{
		m = axel->message;
		axel->message = axel->message->next;
		free( m );
	}
	
	/* Close all connections and local file				*/
	close( axel->outfd );
	for( i = 0; i < axel->conf->num_connections; i ++ )
		conn_disconnect( &axel->conn[i] );

	free( axel->conn );
	free( axel );
}

/* time() with more precision						*/
double gettime()
{
	struct timeval time[1];
	
	gettimeofday( time, 0 );
	return( (double) time->tv_sec + (double) time->tv_usec / 1000000 );
}

/* Save the state of the current download				*/
void save_state( axel_t *axel )
{
	int fd, i;
	char fn[MAX_STRING+4];

	/* No use for such a file if the server doesn't support
	   resuming anyway..						*/
	if( !axel->conn[0].supported )
		return;
	
	snprintf( fn, MAX_STRING, "%s.st", axel->filename );
	if( ( fd = open( fn, O_CREAT | O_TRUNC | O_WRONLY, 0666 ) ) == -1 )
	{
		return;		/* Not 100% fatal..			*/
	}
	write( fd, &axel->conf->num_connections, sizeof( axel->conf->num_connections ) );
	write( fd, &axel->bytes_done, sizeof( axel->bytes_done ) );
	for( i = 0; i < axel->conf->num_connections; i ++ )
	{
		write( fd, &axel->conn[i].currentbyte, sizeof( axel->conn[i].currentbyte ) );
	}
	close( fd );
}

/* Thread used to set up a connection					*/
void *setup_thread( void *c )
{
	conn_t *conn = c;
	int oldstate;
	
	/* Allow this thread to be killed at any time.			*/
	pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, &oldstate );
	pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, &oldstate );
	
	if( conn_setup( conn ) )
	{
		conn->last_transfer = gettime();
		/*
		 * 以HTTP为例,在开始时并未建立连接,每个conn的fd为0或者-1,但是建立连接之后就变成了相应的http->fd
		 * 并不是载一建立就会enabled
		 * */
		if( conn_exec( conn ) )
		{
			conn->last_transfer = gettime();
			conn->enabled = 1;
			conn->state = 0;
			return( NULL );
		}
	}
	
	conn_disconnect( conn );
	conn->state = 0;
	return( NULL );
}

/* Add a message to the axel->message structure				*/
static void axel_message( axel_t *axel, char *format, ... )
{
	message_t *m = malloc( sizeof( message_t ) ), *n = axel->message;
	va_list params;
	
	memset( m, 0, sizeof( message_t ) );
	va_start( params, format );
	vsnprintf( m->text, MAX_STRING, format, params );
	va_end( params );
	
	if( axel->message == NULL )
	{
		axel->message = m;
	}
	else
	{
		while( n->next != NULL )
			n = n->next;
		n->next = m;
	}
}

/* Divide the file and set the locations for each connection		*/
/*
 * 分配每个连接的字节数
 * */
static void axel_divide( axel_t *axel )
{
	int i;
	
	axel->conn[0].currentbyte = 0;
	axel->conn[0].lastbyte = axel->size / axel->conf->num_connections - 1;
	for( i = 1; i < axel->conf->num_connections; i ++ )
	{
#ifdef DEBUG
		printf( "Downloading %i-%i using conn. %i\n", axel->conn[i-1].currentbyte, axel->conn[i-1].lastbyte, i - 1 );
#endif
		axel->conn[i].currentbyte = axel->conn[i-1].lastbyte + 1;
		axel->conn[i].lastbyte = axel->conn[i].currentbyte + axel->size / axel->conf->num_connections;
	}
	axel->conn[axel->conf->num_connections-1].lastbyte = axel->size - 1;
#ifdef DEBUG
	printf( "Downloading %i-%i using conn. %i\n", axel->conn[i-1].currentbyte, axel->conn[i-1].lastbyte, i - 1 );
#endif
}

void print_axel(axel_t *axel)
{
	printf("***********start axel****************\n");
	if(axel)
	{
		url_t *url = axel->url;
		int i , num_con = axel->conf[0].num_connections;
		printf("axel OK!\n");
		printf("axel->filename : %s\n" , axel->filename);
		while(url)
		{
			printf("axel->url : %s\n" , url->text);
			if(url ==  url->next)
			{
				break;
			}
			url = url->next;
		}
		printf("axel->start_byte : %d\n" , axel->start_byte);
		printf("axel->bytes_done : %d\n" , axel->bytes_done);
		printf("axel->size : %d\n" , axel->size);
		printf("axel->outfd : %d\n" , axel->outfd);
		for(i = 0; i <  num_con; ++i)
		{
			print_conn(&axel->conn[i]);
		}
	}
	else
	{
		printf("ERROR axel null\n");
	}
	printf("***********end axel****************\n");
}
