all:
	g++ sample_cgroup.cpp Cgroup.cc CgroupBackend.cc Enum.cc -o sample_cgroup  -lcgroup -std=c++17 -lstdc++fs

clean:
	rm sample_cgroup