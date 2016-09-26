
#include "axel.h"

static void stop( int signal );
static char *size_human( int value );
static char *time_human( int value );
static void print_commas( int bytes_done );
static void print_help();
static void print_version();
static void print_messages( axel_t *axel );
extern void print_conn(conn_t * conn);
extern void print_axel(axel_t *axel);
int run = 1;

#ifdef NOGETOPTLONG
#define getopt_long( a, b, c, d, e ) getopt( a, b, c )
#else
static struct option axel_options[] =
{
	/* name			has_arg	flag	val */
	{ "max-speed",		1,	NULL,	's' },
	{ "num-connections",	1,	NULL,	'n' },
	{ "output",		1,	NULL,	'o' },
	{ "search",		2,	NULL,	'S' },
	{ "no-proxy",		0,	NULL,	'N' },
	{ "quiet",		0,	NULL,	'q' },
	{ "verbose",		0,	NULL,	'v' },
	{ "help",		0,	NULL,	'h' },
	{ "version",		0,	NULL,	'V' },
	{ NULL,			0,	NULL,	0 }
};
#endif

/* For returning string values from functions				*/
static char string[MAX_STRING];

int main( int argc, char *argv[] )
{
	char fn[MAX_STRING] = "";
	int do_search = 0;
	search_t *search;
	conf_t conf[1];
	axel_t *axel;
	int i, j;
	char *s;
	
#ifdef I18N
	setlocale( LC_ALL, "" );
	bindtextdomain( PACKAGE, LOCALE );
	textdomain( PACKAGE );
#endif
	
	if( !conf_init( conf ) )
	{
		return( 1 );
	}
	
	opterr = 0;
	
	j = -1;
	while( 1 )
	{
		int option;
		
		option = getopt_long( argc, argv, "s:n:o:S::NqvhHV", axel_options, NULL );
		if( option == -1 )
			break;
		
		switch( option )
		{
		case 's':
			if( !sscanf( optarg, "%i", &conf->max_speed ) )
			{
				print_help();
				return( 1 );
			}
			break;
		case 'n':
			if( !sscanf( optarg, "%i", &conf->num_connections ) )
			{
				print_help();
				return( 1 );
			}
			break;
		case 'o':
			strncpy( fn, optarg, MAX_STRING );
			break;
		case 'S':
			do_search = 1;
			if( optarg != NULL )
			if( !sscanf( optarg, "%i", &conf->search_top ) )
			{
				print_help();
				return( 1 );
			}
			break;
		case 'N':
			*conf->http_proxy = 0;
			break;
		case 'h':
			print_help();
			return( 0 );
		case 'v':
			if( j == -1 )
				j = 1;
			else
				j ++;
			break;
		case 'V':
			print_version();
			return( 0 );
		case 'q':
			close( 1 );
			conf->verbose = -1;
			if( open( "/dev/null", O_WRONLY ) != 1 )
			{
				fprintf( stderr, _("Can't redirect stdout to /dev/null.\n") );
				return( 1 );
			}
			break;
		default:
			print_help();
			return( 1 );
		}
	}
	
	if( j > -1 )
		conf->verbose = j;
	
	if( argc - optind == 0 )
	{
		print_help();
		return( 1 );
	}
	else if( strcmp( argv[optind], "-" ) == 0 )
	{
		s = malloc( MAX_STRING );
		scanf( "%127[^\n]s", s );
	}
	else
	{
		s = argv[optind];
	}
	
	printf( _("Initializing download: %s\n"), s );
	printf("--------axel new --------\n");
	//axel -n 40 'xxxx' -S 3
	if( do_search )
	{
		search = malloc( sizeof( search_t ) * ( conf->search_amount + 1 ) );
		memset( search, 0, sizeof( search_t ) * ( conf->search_amount + 1 ) );
		search[0].conf = conf;
		if( conf->verbose )
			printf( _("Doing search...\n") );
		i = search_makelist( search, s );
		if( i < 0 )
		{
			fprintf( stderr, _("File not found\n" ) );
			return( 1 );
		}
		if( conf->verbose )
			printf( _("Testing speeds, this can take a while...\n") );
		j = search_getspeeds( search, i );
		search_sortlist( search, i );
		if( conf->verbose )
		{
			printf( _("%i usable servers found, will use these URLs:\n"), j );
			j = min( j, conf->search_top );  //search_top为-S后的参数
			printf( "%-60s %15s\n", "URL", "Speed" );
			for( i = 0; i < j; i ++ )
				printf( "%-70.70s %5i\n", search[i].url, search[i].speed );
			printf( "\n" );
		}
		axel = axel_new( conf, j, search );
		free( search );
		if( axel->ready == -1 )
		{
			print_messages( axel );
			axel_close( axel );
			return( 1 );
		}
	}
	else if( argc - optind == 1 )
	{
		print_conf(conf);
		axel = axel_new( conf, 0, s );
		if( axel->ready == -1 )
		{
			print_messages( axel );
			axel_close( axel );
			return( 1 );
		}
	}
	else
	{
		search = malloc( sizeof( search_t ) * ( argc - optind ) );
		memset( search, 0, sizeof( search_t ) * ( argc - optind ) );
		for( i = 0; i < ( argc - optind ); i ++ )
			strncpy( search[i].url, argv[optind+i], MAX_STRING );
		axel = axel_new( conf, argc - optind, search );
		free( search );
		if( axel->ready == -1 )
		{
			print_messages( axel );
			axel_close( axel );
			return( 1 );
		}
	}
	print_messages( axel );
	if( s != argv[optind] )
	{
		free( s );
	}
	printf("fn : %s\n" , *fn);
	if( *fn )
	{
		struct stat buf;
		
		if( stat( fn, &buf ) == 0 )
		{
			if( S_ISDIR( buf.st_mode ) )
			{
				strncat( fn, "/", MAX_STRING );
				strncat( fn, axel->filename, MAX_STRING );
			}
		}
		sprintf( string, "%s.st", fn );
		if( access( fn, F_OK ) == 0 ) if( access( string, F_OK ) != 0 )
		{
			fprintf( stderr, _("No state file, cannot resume!\n") );
			return( 1 );
		}
		if( access( string, F_OK ) == 0 ) if( access( fn, F_OK ) != 0 )
		{
			printf( _("State file found, but no downloaded data. Starting from scratch.\n" ) );
			unlink( string );
		}
		strcpy( axel->filename, fn );
	}
	else
	{
		/* Local file existence check					*/
		i = 0;
		s = axel->filename + strlen( axel->filename );
		while( 1 )
		{
			sprintf( string, "%s.st", axel->filename );
			if( access( axel->filename, F_OK ) == 0 )
			{
				if( axel->conn[0].supported )
				{
					if( access( string, F_OK ) == 0 )
						break;
				}
			}
			else
			{
				if( access( string, F_OK ) )
					break;
			}
			sprintf( s, ".%i", i );
			i ++;
		}
	}
	//
	print_conn(axel->conn);
//	print_axel(axel);
	printf("--------axel open --------\n");
	if( !axel_open( axel ) )
	{
		print_messages( axel );
		return( 1 );
	}
	print_messages( axel );
//	print_axel(axel);
	printf("--------axel start --------\n");
	axel_start( axel );
	print_messages( axel );
	print_axel(axel);
	if( axel->bytes_done > 0 )	/* Print first dots if resuming	*/
	{
		putchar( '\n' );
		print_commas( axel->bytes_done );
	}
	
	axel->start_byte = axel->bytes_done;
	
	/* Install save_state signal handler for resuming support	*/
	signal( SIGINT, stop );
	signal( SIGTERM, stop );
	
	while( !axel->ready && run )
	{
		int prev, done;
		//已经下载的字节数
		prev = axel->bytes_done;
//		printf("--------axel do --------\n");
		axel_do( axel );
//		print_axel(axel);
		
		/* The infamous wget-like 'interface'.. ;)		*/
		done = ( axel->bytes_done / 1024 ) - ( prev / 1024 );
		if( done && conf->verbose > -1 )
		{
			for( i = 0; i < done; i ++ )
			{
				i += ( prev / 1024 );
				if( ( i % 50 ) == 0 )
				{
					if( prev >= 1024 )
						printf( "  [%6.1fKB/s]", (double) axel->bytes_per_second / 1024 );
					if( axel->size < 10240000 )
						printf( "\n[%3i%%]  ", min( 100, 102400 * i / axel->size ) );
					else
						printf( "\n[%3i%%]  ", min( 100, i / ( axel->size / 102400 ) ) );
				}
				else if( ( i % 10 ) == 0 )
				{
					putchar( ' ' );
				}
				putchar( '.' );
				i -= ( prev / 1024 );
			}
			fflush( stdout );
		}
		
		if( axel->message )
		{
			putchar( '\n' );
			print_messages( axel );
			if( !axel->ready )
				print_commas( axel->bytes_done );
		}
		else if( axel->ready )
		{
			putchar( '\n' );
		}
	}
	
	strcpy( string + MAX_STRING / 2,
		size_human( axel->bytes_done - axel->start_byte ) );
	
	printf( _("\nDownloaded %s in %s. (%.2f KB/s)\n"),
		string + MAX_STRING / 2,
		time_human( gettime() - axel->start_time ),
		(double) axel->bytes_per_second / 1024 );
	
	axel_close( axel );
	
	return( 0 );
}

/* SIGINT/SIGTERM handler						*/
void stop( int signal )
{
	run = 0;
}

/* Convert a number of bytes to a human-readable form			*/
char *size_human( int value )
{
	if( value == 1 )
		sprintf( string, _("%i byte"), value );
	else if( value < 1024 )
		sprintf( string, _("%i bytes"), value );
	else if( value < 10485760 )
		sprintf( string, _("%.1f kilobytes"), (float) value / 1024 );
	else
		sprintf( string, _("%.1f megabytes"), (float) value / 1048576 );
	
	return( string );
}

/* Convert a number of seconds to a human-readable form			*/
char *time_human( int value )
{
	if( value == 1 )
		sprintf( string, _("%i second"), value );
	else if( value < 60 )
		sprintf( string, _("%i seconds"), value );
	else if( value < 3600 )
		sprintf( string, _("%i:%02i seconds"), value / 60, value % 60 );
	else
		sprintf( string, _("%i:%02i:%02i seconds"), value / 3600, ( value / 60 ) % 60, value % 60 );
	
	return( string );
}

/* Part of the infamous wget-like interface. Just put it in a function
   because I need it quite often..					*/
void print_commas( int bytes_done )
{
	int i, j;
	
	printf( "       " );
	j = ( bytes_done / 1024 ) % 50;
	if( j == 0 ) j = 50;
	for( i = 0; i < j; i ++ )
	{
		if( ( i % 10 ) == 0 )
			putchar( ' ' );
		putchar( ',' );
	}
	fflush( stdout );
}

void print_help()
{
#ifdef NOGETOPTLONG
	printf(	_("Usage: axel [options] url1 [url2] [url...]\n"
		"\n"
		"-s x\tSpecify maximum speed (bytes per second)\n"
		"-n x\tSpecify maximum number of connections\n"
		"-o f\tSpecify local output file\n"
		"-S [x]\tSearch for mirrors and download from x servers\n"
		"-N\tJust don't use any proxy server\n"
		"-q\tLeave stdout alone\n"
		"-v\tMore status information\n"
		"-h\tThis information\n"
		"-V\tVersion information\n"
		"\n"
		"Report bugs to lintux@lintux.cx\n") );
#else
	printf(	_("Usage: axel [options] url1 [url2] [url...]\n"
		"\n"
		"--max-speed=x\t\t-s x\tSpecify maximum speed (bytes per second)\n"
		"--num-connections=x\t-n x\tSpecify maximum number of connections\n"
		"--output=f\t\t-o f\tSpecify local output file\n"
		"--search[=x]\t\t-S [x]\tSearch for mirrors and download from x servers\n"
		"--no-proxy\t\t-N\tJust don't use any proxy server\n"
		"--quiet\t\t\t-q\tLeave stdout alone\n"
		"--verbose\t\t-v\tMore status information\n"
		"--help\t\t\t-h\tThis information\n"
		"--version\t\t-V\tVersion information\n"
		"\n"
		"Report bugs to lintux@lintux.cx\n") );
#endif
}

void print_version()
{
	printf( _("Axel version %s (%s)\n"), AXEL_VERSION_STRING, ARCH );
	printf( "\nCopyright 2001 Wilmer van der Gaast.\n" );
}

/* Print any message in the axel structure				*/
void print_messages( axel_t *axel )
{
	message_t *m;
	
	while( axel->message )
	{
		printf( "%s\n", axel->message->text );
		m = axel->message;
		axel->message = axel->message->next;
		free( m );
	}
}

void print_conf(conf_t *conf)
{
	printf("\n------conf---start-----\n");
	printf("filename : %s\n" , conf->default_filename);
	printf("http_proxy : %s\n" , conf->http_proxy);
	printf("no_proxy : %s\n" , conf->no_proxy);
	printf("strip_cgi_parameters : %d\n" , conf->strip_cgi_parameters);
	printf("save_state_interval : %d\n" , conf->save_state_interval);
	printf("connection_timeout : %d\n" , conf->connection_timeout);
	printf("reconnection_delay : %d\n" , conf->reconnect_delay);
	printf("num_connections : %d\n" , conf->num_connections);
	printf("buffer_size : %d\n" , conf->buffer_size);
	printf("max_speed : %d\n" , conf->max_speed);
	printf("verbose : %d\n" , conf->verbose);
	printf("search_timeout : %d\n" , conf->search_timeout);
	printf("search_threads : %d\n" , conf->search_threads);
	printf("search_amount : %d\n" , conf->search_amount);
	printf("search_top : %d\n" , conf->search_top);
	printf("------conf---end-----\n\n");
}
