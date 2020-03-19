#pragma once
#ifndef __TENANTCONFIG_HH__
#define __TENANTCONFIG_HH__

#include <map>
#include <string>
#include <iostream>

#define CONFIG_WARN(error) std::cerr << "WARN: " << error << std::endl

namespace mdsd {

typedef enum {
    CONFINIT_UNIT_PERCENTAGE = 0,
    CONFINIT_UNIT_MEGABYTE,
    CONFINIT_UNIT_KILOBYTE
} TenantUnitMeasure;

class TenantConfig
{
public:
    std::string name;
    unsigned int softquota;
    unsigned int cpu;
    TenantUnitMeasure cpuUnit;
    unsigned int memory;
    TenantUnitMeasure memoryUnit;

    TenantConfig(const std::string& name, unsigned int softquota = 0);

    int ParseUnitMeasure(const std::string& val, std::string& out);

    void ApplyConfig(const std::map<std::string, std::string>& config);

    void Print() {
        std::cout << "Tenant: '" << name << "' softquota=" << softquota << " cpu=" << cpu << " memory=" << memory << std::endl;
    }
};

} // namespace mdsd

#endif // __CONFIGINI_HH__

