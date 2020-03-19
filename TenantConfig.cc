#include "TenantConfig.hh"
#include <iostream>
#include <exception>
#include <boost/algorithm/string.hpp>

using namespace mdsd;
using namespace std;
using namespace boost::algorithm;

TenantConfig::TenantConfig(const std::string& name, unsigned int softquota)
: name(name), softquota(softquota)
{}

int TenantConfig::ParseUnitMeasure(const std::string& val, std::string& out)
{
    std::string value = to_lower_copy(val);
    out = value;
    if (ends_with(value, "%")){
        replace_all(out, "%", "");
        return CONFINIT_UNIT_PERCENTAGE;
    } else if(ends_with(value, "mb")) {
        replace_all(out, "mb", "");
        return CONFINIT_UNIT_MEGABYTE;
    } else if(ends_with(value, "kb")) {
        replace_all(out, "kb", "");
        return CONFINIT_UNIT_KILOBYTE;
    }

    return -1;
}
void TenantConfig::ApplyConfig(const std::map<std::string, std::string>& config)
{
    for (map<string, string>::const_iterator it = config.begin(); it != config.end(); ++it)
    {
        std::string value_without_unit;
        auto unit = ParseUnitMeasure(it->second, value_without_unit);

        if (it->first == "SoftQuotaCushion") {
            softquota = stoul(value_without_unit);
        }
        else if (it->first == "CPU")
        {
            cpuUnit = CONFINIT_UNIT_PERCENTAGE;
            cpu = stoul(value_without_unit);
        }
        else if (it->first == "Memory")
        {
            if (unit < 0)
                CONFIG_WARN("For tenant '" << this->name << "', failing to parse unit of '"
                    << it->first << "' field, value='" << it->second <<"', falling back to default");
            memoryUnit = static_cast<TenantUnitMeasure>(unit >= 0 ? unit : CONFINIT_UNIT_MEGABYTE);
            memory = stoul(value_without_unit);
        }
    }
}
