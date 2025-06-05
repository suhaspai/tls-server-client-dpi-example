# default value which can be overridden by env variable
BUFLEN ?= 4
DELAY  ?= 2000

build: client.sv client.c
	verilator --cc --exe --build --timing --main \
	-CFLAGS "-DBUFLEN=${BUFLEN} -I${PWD}/.." +incdir+.. \
	client.sv client.c --top-module client \
	-LDFLAGS "-lssl -lcrypto"
# "make link" only if --build is not used
link:
	make -j -C obj_dir -f Vclient.mk Vclient
run:
	obj_dir/Vclient
run_verbose:
	obj_dir/Vclient +verbose +delay=40
run_stats:
	obj_dir/Vclient +delay=${DELAY}
clean:
	rm -rf obj_dir
