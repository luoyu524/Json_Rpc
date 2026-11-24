CXX ?= g++
CXXFLAGS = -std=c++17 -O2 -g -I ../../../build/release-install-cpp11/include/
LDLIBS = -L../../../build/release-install-cpp11/lib -ljsoncpp -lmuduo_net -lmuduo_base -pthread

TARGETS = bench_server bench_client bench_registry bench_topic_server bench_topic_client

all: $(TARGETS)

bench_server: bench_server.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)

bench_client: bench_client.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)

bench_registry: bench_registry.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)

bench_topic_server: bench_topic_server.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)

bench_topic_client: bench_topic_client.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
