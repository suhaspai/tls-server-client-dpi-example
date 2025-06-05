// Factor common client and server c code in this include file

#ifndef PACKET_H
#define PACKET_H
#endif

#include <sys/time.h>
#include <time.h>

// For C and SV data types (/usr/share/verilator/include/vltstd/svdpi.h)
#include "svdpi.h"

typedef struct {
   svBitVecVal control;
   svLogicVecVal data[BUFLEN*32/32];   
} packet_t;


#define DEFAULT_TCP_PORT   8080
#define DEFAULT_STATS_INTV 60   // seconds
#define CLIENT_CONNECT_TO  1800  // seconds
#define WAIT_SECONDS       10
#ifndef BUFLEN
#define BUFLEN 4
#endif

// Throughput stats, etc.
static time_t        start_time;
static time_t        end_time;

static unsigned long total_tx_data;
static unsigned long total_rx_data;
static unsigned      stats_interval;

static int      ssl_mode;

// Used in server and client
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

// Used in server and client
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

// Used in server and client
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

// Used in server and client
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

// Client specific
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

// Client specific
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
