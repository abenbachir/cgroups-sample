

HEADERS=-I/usr/local/include/confini.h
LIBS=-I/usr/local/lib/libconfini.so

all:
	g++ cgroup_main.cpp TenantConfig.cc ConfigINI.cc EnumToString.cc Cgroup.cc CgroupBackendFactory.cc CgroupBackendV1.cc CgroupBackendV2.cc CgroupBackend.cc -o cgroup_main -lconfini -pthread -lcgroup -std=c++17 -lstdc++fs

clean:
	rm -f main cgroup_main