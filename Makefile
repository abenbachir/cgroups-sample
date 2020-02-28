all:
	g++ -o sample_cgroup sample_cgroup.cpp -lcgroup

clean:
	rm sample_cgroup