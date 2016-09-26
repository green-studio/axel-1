
#define PROTO_FTP	1
#define PROTO_HTTP	2
#define PROTO_DEFAULT	PROTO_FTP

typedef struct
{
	conf_t *conf;
	
	int proto;
	int port;
	int proxy;
	char host[MAX_STRING];
	char dir[MAX_STRING];
	char file[MAX_STRING];
	char user[MAX_STRING];
	char pass[MAX_STRING];
	
	ftp_t ftp[1];
	http_t http[1];
	int size;		/* File size, not 'connection size'..	*/
	int currentbyte;
	int lastbyte;
	int fd;
	int enabled;
	int supported;
	int last_transfer;
	char *message;

	int state;
	pthread_t setup_thread[1];
} conn_t;

int conn_set( conn_t *conn, char *set_url );
char *conn_url( conn_t *conn );
void conn_disconnect( conn_t *conn );
int conn_init( conn_t *conn );
int conn_setup( conn_t *conn );
int conn_exec( conn_t *conn );
int conn_info( conn_t *conn );
