all:
	g++ cgroup_main.cpp Enum.cc Cgroup.cc CgroupBackendFactory.cc CgroupBackendV2.cc CgroupBackend.cc -o cgroup_main -pthread -lcgroup -std=c++17 -lstdc++fs

clean:
	rm -f main cgroup_main