all:
	g++ -o sample_cgroup sample_cgroup.cpp Cgroup.cc CgroupBackend.cc Enum.cc -lcgroup

clean:
	rm sample_cgroup