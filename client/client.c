#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <netdb.h>

#include <sys/time.h>
#include <time.h>

// For SSL api
#include <openssl/ssl.h>
#include <openssl/err.h>

// For DPI api
#include "svdpi.h"

static SSL_CTX *ctx;
static SSL     *ssl;
static int      ssl_mode;
static int      socket_fd;

// Defaults
#define DEFAULT_TCP_PORT   8080
#define CLIENT_CONNECT_TO  1800  // seconds
#define DEFAULT_STATS_INTV 60    // seconds
#define WAIT_SECONDS       10

time_t start_time;
time_t end_time;

int long total_tx_data;
int long total_rx_data;
int  stats_interval;

#ifndef BUFLEN
#define BUFLEN 4
#endif

typedef struct {
   svBitVecVal control;
   svLogicVecVal data[BUFLEN*32/32];   
} c_data_t;

extern "C" void print_throughput_stats(const char *port)
{
   time_t run_time;

   end_time = time(NULL);
   run_time = end_time - start_time;

   if (run_time >= stats_interval)
   {
      printf("%s Sim-Time= %10ld sec, Throughput: TX= %8.3lf kB/s \t RX= %8.3lf kB/s\n", port, run_time,
             ((double) total_tx_data/run_time/1000), ((double) total_rx_data/run_time/1000));
      start_time    = end_time;
      total_tx_data = 0;
      total_rx_data = 0;
   }
}

extern "C" int c_get_ssl_mode(const char *env_var) {
   char *value;
   value = getenv(env_var);
   if (value == NULL) {
      printf("Default SSL mode= %d\n", ssl_mode);
      return (ssl_mode);
   }
   else {
      sscanf(value, "%d", &ssl_mode);
      printf("User defined SSL mode= %d\n", ssl_mode);      
      return (ssl_mode);
   }
}

extern "C" const char * c_get_server_ip(const char *env_var) {
   char *value;

   value = getenv(env_var);
   if (value == NULL) {
      printf("Default server IP address set to %s\n","localhost");
      return ("localhost");
   }
   else {
      printf("User programmed server IP address to %s\n", value);      
      return (value);
   }
}

extern "C" int c_get_tcp_port(const char *env_var) {
   char *value;
   int   tcp_port;
   
   value = getenv(env_var);
   if (value == NULL) {
      printf("Default TCP port= %d\n", DEFAULT_TCP_PORT);
      return (DEFAULT_TCP_PORT);
   }
   else {
      sscanf(value, "%d", &tcp_port);
      printf("User defined TCP/IP port= %d\n", tcp_port);      
      return (tcp_port);
   }
}

extern "C" int c_get_stats_intv(const char *env_var) {
   char * value;
   int  intv;

   value = getenv(env_var);
   if (value == NULL) {
      printf("Default stats interval set to %d seconds\n",DEFAULT_STATS_INTV);
      return (DEFAULT_STATS_INTV);
   }
   else {
      sscanf(value, "%d", &intv);      
      printf("User programmed throughput stats interval to %d seconds\n", intv);  
      return (intv);
   }
}

extern "C" int c_get_max_retry_time(const char *env_var) {
   char *value;
   int   max_time;
   
   value = getenv(env_var);
   if (value == NULL) {
      printf("Default max client connect time in seconds= %d\n", CLIENT_CONNECT_TO);
      return (CLIENT_CONNECT_TO);
   }
   else {
      sscanf(value, "%d", &max_time);
      printf("User defined max client connect time in seconds= %d\n", max_time);      
      return (max_time);
   }
}

extern "C" void close_socket()
{
   char    buf[128];
   ssize_t n;

   if (ssl_mode)
      SSL_free(ssl);

   /* first, shut down the socket for writing */
   shutdown(socket_fd, SHUT_WR);

   /* then, read data from the socket until it returns 0,
    * indicating all data from the other side has been
    * received.
    */
   while (1) {
      n = recv(socket_fd, buf, 128, 0);
      if (n  == 0)
         break;
      if ((n == -1) && (errno != EINTR))
         break;
   }

   /* finally, close the socket */
   close(socket_fd);

   if (ssl_mode)
      SSL_CTX_free(ctx);
      
   print_throughput_stats("USP");
}

extern "C" int net_open_client_socket(const char* transport)
{

   struct addrinfo  hints;
   struct addrinfo *result, *rp;
   int              s;
   char             service[12];
   const char*      addr = c_get_server_ip("SERVER_IP");
   int              port = c_get_tcp_port("TCP_PORT");       
   int              max_retry_time = c_get_max_retry_time("CLIENT_CONNECT_TO");
   int              retry_time = 0;

   // Use user defined parameters if set, otherwise defaults
   stats_interval = c_get_stats_intv("STATS_INTV");
   ssl_mode = c_get_ssl_mode("SSL_MODE");

   if (ssl_mode)
   {
      // Initialize OpenSSL
      SSL_library_init();
      OpenSSL_add_all_algorithms();
      SSL_load_error_strings();

      // Create SSL context
      ctx = SSL_CTX_new(TLS_client_method());
      if (!ctx) {
         perror("SSL_CTX_new");
         exit(1);
      }
   }

   /* Obtain address(es) matching host/port */
   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_family                    = AF_INET;
   if (strcmp(transport, "udp")      == 0)
      hints.ai_socktype               = SOCK_DGRAM; /* UDP socket */ 
   else if (strcmp(transport, "tcp") == 0)
      hints.ai_socktype               = SOCK_STREAM; /* TCP/IP socket */
   else {
      printf("must be either udp or tcp, not %s! \n", transport);
      exit(1);
   }
      

   hints.ai_flags    = 0;
   hints.ai_protocol = 0;       /* Any protocol */

   sprintf(service, "%d", port);
   s = getaddrinfo(addr, service, &hints, &result);
   if (s != 0) {
      fprintf(stderr, "Error: getaddrinfo() failed: %s\n", gai_strerror(s));
      return -1;
   }

   /* getaddrinfo() returns a list of address structures.
      Try each address until we successfully connect(2).
      If socket(2) (or connect(2)) fails, we (close the socket
      and) try the next address. */

   for (rp = result; rp != NULL; rp = rp->ai_next) {
      socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (socket_fd == -1)
         continue;
      else
         break;
   }

   if (rp == NULL) {            /* No address succeeded */
      fprintf(stderr, "Error: connect() failed\n");
      return -1;
   }

   while (1)
   {
      if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1)
         break;                 /* Success */
      else
      {
         sleep(WAIT_SECONDS);
         retry_time += WAIT_SECONDS;
         printf("Retried to conect to server at %5d seconds\n", retry_time);
         if (retry_time >= max_retry_time)
         {
            close(socket_fd);
            return (-1);
         }
         else
            continue;
      }
   }

   freeaddrinfo(result);     /* No longer needed */

   // Set NODELAY socket option to disable Nagle algorithm
   int one = 1;
   s       = setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

   fprintf(stdout, "%s:%s:%d: socket_fd= %d\n", __FILE__, __FUNCTION__, __LINE__, socket_fd);

   // Create SSL connection
   if (ssl_mode)
   {
      ssl = SSL_new(ctx);
      SSL_set_fd(ssl, socket_fd);
 
      // Perform SSL handshake
      if (SSL_connect(ssl) <= 0) {
         ERR_print_errors_fp(stderr);
         exit(1);
      }
   }

   // Note down sim start time
   start_time = time(NULL);
   
   return (0);
}

extern "C" int  client_send(const c_data_t * device_tx_data)
{
   int ret;

   if (ssl_mode)
      ret = SSL_write(ssl, device_tx_data, sizeof(c_data_t) );
   else
      ret = send(socket_fd, device_tx_data, sizeof(*device_tx_data),0);

   if(ret>0)
   {
      total_tx_data += sizeof(*device_tx_data);
      print_throughput_stats("client");
   }

   return(ret);
} 

extern "C" int client_recv(c_data_t * device_rx_data)
{
   int ret;
   if (ssl_mode)
      ret = SSL_read(ssl, device_rx_data, sizeof(c_data_t) );
   else
      ret = recv(socket_fd, device_rx_data, sizeof(*device_rx_data),0);

   if(ret>0)
   {
      total_rx_data += sizeof(*device_rx_data);
      print_throughput_stats("client");
   }

   return(ret);
} 
