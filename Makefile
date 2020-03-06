all:
	g++ main.cpp Cgroup.cc CgroupBackend.cc Enum.cc -o main -pthread -lcgroup -std=c++17 -lstdc++fs

clean:
	rm main