CFLAG= -std=c++17 -I ../../../build/release-install-cpp11/include/
LFLAG= -L../../../build/release-install-cpp11/lib  -ljsoncpp -lmuduo_net -lmuduo_base -pthread
all : registry_server rpc_server rpc_client
registry_server : registry_server.cpp
	g++  -g $(CFLAG) $^ -o $@ $(LFLAG)
rpc_server : rpc_server.cpp
	g++  -g $(CFLAG) $^ -o $@ $(LFLAG)
rpc_client : rpc_client.cpp
	g++  -g $(CFLAG) $^ -o $@ $(LFLAG)
.PHONY:clean
clean:
	rm -rf registry_server rpc_server rpc_client 