#include "Cgroup.hh"
#include "CgroupBackend.hh"
#include "CgroupBackendV2.hh"
#include "CgroupBackendV1.hh"
#include "Enum.hh"

#include <unistd.h>
#include <mntent.h>
#include <sys/mount.h>
#include <fstream>
#include <iostream>

#include <exception>

// for file open/read
#include <fcntl.h>
#include <sys/file.h>

using namespace mdsd;


Cgroup::Cgroup(const std::shared_ptr<CgroupBackend>& backend): backend(backend)
{
    backend->Init();
}

Cgroup::~Cgroup()
{
}


