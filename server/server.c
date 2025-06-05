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
#include <arpa/inet.h>

#include <sys/time.h>
#include <time.h>

// For SSL api
#include <openssl/ssl.h>
#include <openssl/err.h>

// For DPI api
#include "svdpi.h"

#define CERTFILE "server.crt"
#define KEYFILE  "server.key"

// hide locally
static SSL     *ssl;            
static SSL_CTX *ctx;

static int      clientfd;
static int      ssl_mode;

#define DEFAULT_TCP_PORT   8080
#define DEFAULT_STATS_INTV 60   // seconds

// For throughput stats
time_t        start_time;
time_t        end_time;

unsigned long total_tx_data;
unsigned long total_rx_data;
unsigned      stats_interval;

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

extern "C" int c_get_tcp_port(const char *env_var) {
   char *value;
   int   tcp_port;
   
   value = getenv(env_var);
   if (value == NULL) {
      printf("Server returning TCP port= %d\n", DEFAULT_TCP_PORT);
      return (DEFAULT_TCP_PORT);
   }
   else {
      sscanf(value, "%d", &tcp_port);
      printf("User programmed TCP/IP port to %d\n", tcp_port);      
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
      printf("User programmed TCP/IP throughput stats interval to %d seconds\n", intv);      
      return (intv);
   }
}

extern "C" void close_socket()
{
   char buf[128];
   ssize_t n;

   if (ssl_mode)   
      SSL_free(ssl);
   
   /* first, shut down the socket for writing */
   shutdown(clientfd, SHUT_WR);

   /* then, read data from the socket until it returns 0,
    * indicating all data from the other side has been
    * received.
    */
   while (1) {
      n = recv(clientfd, buf, 128, 0);
      if (n  == 0)
         break;
      if ((n == -1) && (errno != EINTR))
         break;
   }

   /* finally, close the socket */
   close(clientfd);

   if (ssl_mode)   
      SSL_CTX_free(ctx);
}

extern "C" void print_hostname() {
   char            hostname[1024];
   struct hostent* h;
   struct addrinfo hints, *res, *p;
   int             status;
   char            ipstr[INET6_ADDRSTRLEN];

   // find the hostname
   gethostname(hostname, 1023);
   printf("Hostname= %s\n", hostname); /* returns only the hostname */
   h = gethostbyname(hostname);
   printf("Use this $hostname: %s when connecting from a client\n", h->h_name); /* prints domain name  */

   // find the IP address 
   memset(&hints, 0, sizeof hints);
   hints.ai_family   = AF_UNSPEC; // AF_INET or AF_INET6 to force version
   hints.ai_socktype = SOCK_STREAM;
   if ((status       = getaddrinfo(h->h_name, NULL, &hints, &res)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
      exit(2);
   }

   printf("IP addresses for %s:\n", h->h_name);

   for(p = res;p != NULL; p = p->ai_next) {
      void       *addr;
      const char *ipver;

      // get the pointer to the address itself,
      // different fields in IPv4 and IPv6:
      if (p->ai_family == AF_INET) {     // IPv4
         struct sockaddr_in  *ipv4 = (struct sockaddr_in *)p->ai_addr;
         addr                      = &(ipv4->sin_addr);
         ipver                     = "IPv4";
      } else {                  // IPv6
         struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
         addr                      = &(ipv6->sin6_addr);
         ipver                     = "IPv6";
      }

      // convert the IP to a string and print it:
      inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
      printf("  %s: %s\n", ipver, ipstr);
   }

   freeaddrinfo(res);                    // free the linked list

   return;
}   

extern "C" int net_open_server_socket(const char *transport)
{
   struct addrinfo hints, *result, *rp;
   int             s, sfd;

   struct sockaddr client_addr;
   socklen_t client_addr_len;

   char port_str[12];
   int  port;

   // Use user defined parameters if set, otherwise defaults
   port = c_get_tcp_port("TCP_PORT");
   ssl_mode = c_get_ssl_mode("SSL_MODE");
   stats_interval = c_get_stats_intv("STATS_INTV");
   
   if (ssl_mode)
   {
      // Initialize OpenSSL
      SSL_library_init();
      OpenSSL_add_all_algorithms();
      SSL_load_error_strings();
 

      // Create SSL context
      ctx = SSL_CTX_new(TLS_server_method());
      if (!ctx) {
         perror("SSL_CTX_new");
         exit(1);
      }
 
      // Load certificate and private key
      if (SSL_CTX_use_certificate_file(ctx, CERTFILE, SSL_FILETYPE_PEM) <= 0) {
         ERR_print_errors_fp(stderr);
         exit(1);
      }
      if (SSL_CTX_use_PrivateKey_file(ctx, KEYFILE, SSL_FILETYPE_PEM) <= 0) {
         ERR_print_errors_fp(stderr);
         exit(1);
      }
   }
   
   // client needs this info about hostname or ip address to connect to server
   print_hostname();

   sfd = -1;

   // Reference: https://www.man7.org/linux/man-pages/man3/getaddrinfo.3.html
   memset(&hints, 0, sizeof(struct addrinfo));
   hints.ai_family                    = AF_INET;
   if (strcmp(transport, "udp")      == 0)
      hints.ai_socktype               = SOCK_DGRAM; /* UDP socket */ 
   else if (strcmp(transport, "tcp") == 0)
      hints.ai_socktype               = SOCK_STREAM; /* TCP/IP socket */
   else {
      fprintf(stderr, "must be either udp or tcp, not %s! \n", transport);
      exit(1);
   }

   hints.ai_flags     = AI_PASSIVE; /* For wildcard IP address */
   hints.ai_protocol  = 0;
   hints.ai_canonname = NULL;
   hints.ai_addr      = NULL;
   hints.ai_next      = NULL;

   sprintf(port_str, "%d", port);
   s = getaddrinfo(NULL, port_str, &hints, &result);
   if (s != 0) {
      fprintf(stderr, "Error: getaddrinfo(): %s\n", gai_strerror(s));
      return -1;
   }

   /* getaddrinfo() returns a list of address structures.
    * Try each address until we successfully bind(2).
    * If socket(2) (or bind(2)) fails, we (close the socket
    * and) try the next address.
    */

   for (rp = result; rp != NULL; rp = rp->ai_next) {
      sfd  = socket(rp->ai_family, rp->ai_socktype,
                    rp->ai_protocol);
      if (sfd == -1)
         continue;

      int one = 1;
      if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1)
         perror("setsockopt");

      if (setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) == -1)
         perror("setsockopt");

      if (bind(sfd, rp->ai_addr, rp->ai_addrlen)                       == 0)
         break;                 /* Success */
      perror("bind");

      close(sfd);
   }

   if (rp == NULL) {            /* No address succeeded */
      fprintf(stderr, "Error: bind() failed at port %d\n", port);
      return -1;
   }

   fprintf(stdout,"%s:%s:%d: server sfd = %d\n", __FILE__, __FUNCTION__, __LINE__, sfd);
   freeaddrinfo(result);        /* No longer needed */

   /* Start listening for the client's connection
    * The second arg is 1 because we're only expecting a single client
    */
   s = listen(sfd, 1);
   if (s < 0) {
      fprintf(stderr, "Error: listen() failed on socket %0d at port %d\n",
              sfd, port);
      return -1;
   }

   /* Accept a connection from the client */
   client_addr_len = sizeof(client_addr);
   clientfd = accept(sfd, &client_addr, &client_addr_len);
   if (clientfd < 0) {
      fprintf(stderr, "Error: accept() failed on socket %0d at port %d\n",
              sfd, port);
      return -1;
   }
   fprintf(stdout, "%s:%s:%d: socket_fd= %d\n", __FILE__, __FUNCTION__, __LINE__, clientfd);
   /* Typical general-purpose servers do a fork() at this point to have the
    * child process handle this connection while the parent listens for
    * more connections, but here we are not expecting any more connections
    * so we don't fork a new process
    */

   // If you're only getting one single connection ever, you can close() the listening sockfd in
   //   order to prevent more incoming connections on the same port
   close(sfd);                  /* not needed any more */

   if (ssl_mode)
   {
      // Create SSL connection
      ssl = SSL_new(ctx);
      SSL_set_fd(ssl, clientfd);
 

      // Perform SSL handshake
      if (SSL_accept(ssl) <= 0) {
         ERR_print_errors_fp(stderr);
         exit(1);
      }
   }
   
   // Note down sim start time
   start_time = time(NULL);
   
   return (0);
}

extern "C" int server_send(const c_data_t * host_tx_data)
{
   int ret;
   
   if (ssl_mode)
      ret = SSL_write(ssl, host_tx_data, sizeof(*host_tx_data));
   else
      ret = send(clientfd, host_tx_data, sizeof(*host_tx_data), 0);

   if(ret > 0)
   {
      total_tx_data += sizeof(*host_tx_data);
      print_throughput_stats("server");
   }

   return (ret);
} 

extern "C" int server_recv(c_data_t * host_rx_data)
{
   int ret;

   if (ssl_mode)
      ret = SSL_read(ssl, host_rx_data, sizeof(*host_rx_data));
   else
      ret = recv(clientfd, host_rx_data, sizeof(*host_rx_data), 0);

   if(ret > 0)
   {
      total_rx_data += sizeof(*host_rx_data);
      print_throughput_stats("server");
   }

   return(ret);
} 
