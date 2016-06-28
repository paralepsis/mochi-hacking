CC=gcc
CFLAGS=-Wno-address -Wall -g
INCLUDEDIRS=-I/opt/local/include -I/Users/rross/local/include
LIBDIRS=-L/opt/local/lib -L/Users/rross/local/lib
LIBS=-labt-snoozer -lev -lm -labt -lpthread -lmercury_hl_debug -lmercury_debug \
     -lmchecksum_debug -lna_debug -lmercury_util_debug -lbmi -lmargo

src/margo-example/client: src/margo-example/client.c
	$(CC) $(CFLAGS) $(INCLUDEDIRS) -o $@ $< $(LIBDIRS) $(LIBS)

src/margo-example/server: src/margo-example/server.c src/margo-example/my-rpc.o
	$(CC) $(CFLAGS) $(INCLUDEDIRS) -o $@ $< $(LIBDIRS) $(LIBS) src/margo-example/my-rpc.o

src/margo-example/my-rpc.o: src/margo-example/my-rpc.c
	$(CC) $(CFLAGS) $(INCLUDEDIRS) -c $< -o $@

src/me/startup: src/me/startup.c
	$(CC) $(CFLAGS) $(INCLUDEDIRS) -o $@ $< $(LIBDIRS) $(LIBS)

src/me/rdma: src/me/rdma.c
	$(CC) $(CFLAGS) $(INCLUDEDIRS) -o $@ $< $(LIBDIRS) $(LIBS)

src/discovery/hg-sv-disc.o: src/discovery/hg-sv-disc.c
	$(CC) $(CFLAGS) $(INCLUDEDIRS) -Isrc/discovery -o $@ -c $< 

src/discovery/init.o: src/discovery/init.c
	$(CC) $(CFLAGS) $(INCLUDEDIRS) -Isrc/discovery -o $@ -c $< 

src/discovery/hg-disc-svr: src/discovery/hg-disc-svr.c src/discovery/init.o src/discovery/hg-sv-disc.o
	$(CC) $(CFLAGS) $(INCLUDEDIRS) -Isrc/discovery -o $@ $< $(LIBDIRS) \
	      $(LIBS) src/discovery/init.o src/discovery/hg-sv-disc.o

