SHELL = tcsh

# program defaults
TCP_PORT  ?= 8080
transport ?= tcp
delay     ?= 4000

clean:
	cd server && make clean
	cd client && make clean
build:
	cd server && make build
	cd client && make build
# must run this step only once to generate public/private keys and certificate if testing in SSL_MODE (setenv SSL_MODE 1)
ssl:
	cd server && make ssl
run:
	cd server && make run &
	sleep 10
	cd client && make run &
run_verbose:
	cd server && make run_verbose &
	sleep 10
	cd client && make run_verbose &
run_stats:
	cd server && setenv STATS_INTV 1 && setenv DELAY ${delay} && make run_stats &
	sleep 10
	cd client && setenv STATS_INTV 1 && setenv DELAY ${delay} && make run_stats &
# Client should connect to server
run1:
	cd client && setenv CLIENT_CONNECT_TO 60 && make run &
	sleep 30
	cd server && make run &
# Client will will wait till connect timeout to connect to server before giving up
run2:
	cd client && setenv CLIENT_CONNECT_TO 30 && make run &
	sleep 60
#	cd server &&  make run &

# You can also test connect timeout by running client first, wait for a while and then run server
run_client:
	cd client && make run &
run_server:
	cd server && make run &

# Housekeeping
kill:
	fuser -i -TERM -k ${TCP_PORT}/${transport} &
tcp_stat:
	netstat -vatn | grep ${TCP_PORT} &
listen:
	netstat --listen | grep ${TCP_PORT} &
udp_stat:
	netstat -vaun | grep ${TCP_PORT} &
pid:
	lsof -ti :${TCP_PORT}

