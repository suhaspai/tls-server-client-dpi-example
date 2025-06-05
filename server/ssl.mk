ssl: version private public csr crt 

# Check version
version:
	openssl version -a
# Generate private key (./server.key)
private:
	openssl genrsa -out server.key 2048 
# Extract public key (./public.key)
public:
	openssl rsa -in server.key -pubout -out public.key
# Create certificate signing request  (https://docs.openssl.org/3.3/man1/openssl-req/)
csr:
	openssl req -new -subj "/C=US/ST=CA/L=SD/O=OpenSource/CN=localhost/emailAddress=your@email.com" -key server.key -out server.csr
csr_interactive:
	openssl req -new -key server.key -out server.csr
# Verify csr (do not output private key)
verify:
	openssl req -text -in server.csr -noout -verify
# Generate self-signed certificated
crt:
	openssl x509 -in server.csr -out server.crt -req -signkey server.key -days 365
run: run_server run_client
run_server:
	server &
run_client:
	client &
ssl_clean:
	rm -rf server.key public.key server.csr server.crt server client
# install netstat
netstat:
	sudo apt install net-tools -y
# list all listening tcp ports
listen:
	netstat -lt
pid:
	lsof -ti :${port}
kill:
	kill -9 ${pid}
# Install openssl and libssl 
openssl:
	sudo apt-get update -qq
	sudo apt-get install -y openssl
libssl:
	sudo apt-get install libssl-dev -y

