
#include "axel.h"

/* Some nifty macro's..							*/
#define get_config_string( name )				\
	if( strcmp( key, #name ) == 0 )				\
	{							\
		st = 1;						\
		strcpy( conf->name, value );			\
	}
#define get_config_number( name )				\
	if( strcmp( key, #name ) == 0 )				\
	{							\
		st = 1;						\
		sscanf( value, "%i", &conf->name );		\
	}

int conf_loadfile( conf_t *conf, char *file )
{
	int i, line = 0;
	FILE *fp;
	char s[MAX_STRING], key[MAX_STRING], value[MAX_STRING];
	
	fp = fopen( file, "r" );
	if( fp == NULL )
		return( 1 );			/* Not a real failure	*/
	
	while( !feof( fp ) )
	{
		int st;
		
		line ++;
		
		fscanf( fp, "%100[^\n#]s", s );
		fscanf( fp, "%*[^\n]s" );
		fgetc( fp );			/* Skip newline		*/
		if( strchr( s, '=' ) == NULL )
			continue;		/* Probably empty?	*/
		sscanf( s, "%[^= \t]s", key );
		for( i = 0; s[i]; i ++ )
			if( s[i] == '=' )
			{
				for( i ++; isspace( (int) s[i] ) && s[i]; i ++ );
				break;
			}
		strcpy( value, &s[i] );
		for( i = strlen( value ) - 1; isspace( (int) value[i] ); i -- )
			value[i] = 0;
		
		st = 0;
		
		/* Long live macros!!					*/
		get_config_string( default_filename );
		get_config_string( http_proxy );
		get_config_string( no_proxy );
		get_config_number( strip_cgi_parameters );
		get_config_number( save_state_interval );
		get_config_number( connection_timeout );
		get_config_number( reconnect_delay );
		get_config_number( num_connections );
		get_config_number( buffer_size );
		get_config_number( max_speed );
		get_config_number( verbose );
		
		get_config_number( search_timeout );
		get_config_number( search_threads );
		get_config_number( search_amount );
		get_config_number( search_top );
		
		/* Option defunct but shouldn't be an error		*/
		if( strcmp( key, "speed_type" ) == 0 )
			st = 1;
		
		if( !st )
		{
			fprintf( stderr, _("Error in %s line %i.\n"), file, line );
			return( 0 );
		}
	}
	
	fclose( fp );
	return( 1 );
}

int conf_init( conf_t *conf )
{
	char s[MAX_STRING], *s2;
	int i;
	
	/* Set defaults							*/
	memset( conf, 0, sizeof( conf_t ) );
	strcpy( conf->default_filename, "default" );
	*conf->http_proxy		= 0;	//默认没有代理
	*conf->no_proxy			= 0;
	conf->strip_cgi_parameters	= 1;	
	conf->save_state_interval	= 10;
	conf->connection_timeout	= 60;
	conf->reconnect_delay		= 20;
	conf->num_connections		= 4;
	conf->buffer_size		= 5120;
	conf->max_speed			= 0;
	conf->verbose			= 1;	//默认显示详细信息
	
	conf->search_timeout		= 10;
	conf->search_threads		= 3;	//默认开3个线程
	conf->search_amount		= 15;	
	conf->search_top		= 3;	//search线程默认最多3个
	
	if( ( s2 = getenv( "HTTP_PROXY" ) ) != NULL )
		strncpy( conf->http_proxy, s2, MAX_STRING );
	
	if( !conf_loadfile( conf, ETCDIR "/axelrc" ) )
		return( 0 );
	
	if( ( s2 = getenv( "HOME" ) ) != NULL )
	{
		sprintf( s, "%s/%s", s2, ".axelrc" );
		if( !conf_loadfile( conf, s ) )
			return( 0 );
	}
	
	/* Convert no_proxy to a 0-separated-and-00-terminated list..	*/
	for( i = 0; conf->no_proxy[i]; i ++ )
		if( conf->no_proxy[i] == ',' )
			conf->no_proxy[i] = 0;
	conf->no_proxy[i+1] = 0;
	
	return( 1 );
}
