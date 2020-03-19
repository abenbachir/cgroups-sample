#pragma once
#ifndef __CGROUP_DEF_HH__
#define __CGROUP_DEF_HH__

#include <iostream>
#include <sstream>
#include <cstring>
#include <string>
#include <sstream>

#define CGROUP_MAX_VAL 512
#define CGROUP_MEM_MB_TO_BYTES(val) val * 1024 * 1024
#define CGROUP_MEM_MB_TO_KB(val) val * 1024
#define CGROUP_MEM_KB_TO_MB(val) double(val) / 1024
#define CGROUP_MEM_KB_TO_BYTES(val) val * 1024
#define CGROUP_MEMORY_PARAM_UNLIMITED 9007199254740991LL /* = INT64_MAX >> 10 */

#ifndef LOGGER_LINE_INFO
# define LOGGER_LINE_INFO(line_str) \
	do { \
		std::ostringstream __msg; \
		__msg << line_str; \
		std::cout << __msg.str() << std::endl;\
	} while (0)
#endif

#ifndef LOGGER_LINE_ERROR
# define LOGGER_LINE_ERROR(line_str) \
	do { \
		std::ostringstream __msg; \
		__msg << line_str; \
		std::cout << __msg.str() << std::endl;\
	} while (0)
#endif

#define CGROUP_DEBUG(log) LOGGER_LINE_INFO("[CGROUP] " << log)
#define CGROUP_ERROR(error) LOGGER_LINE_ERROR("[CGROUP] " << error)
#endif // __CGROUP_DEF_HH__

