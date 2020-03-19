#pragma once
#ifndef __CONFIGINI_HH__
#define __CONFIGINI_HH__

#include <map>
#include <string>
#include <confini.h>

namespace mdsd {

#define ROOT_SECTION_NAME "{root}"
typedef std::map<std::string, std::map<std::string, std::string>> IniParsedDataMap;

class ConfigINI
{
public:
    ConfigINI();
    ~ConfigINI();

    void Parse(const std::string& path, IniParsedDataMap& data);

private:
    static int ListenerCallback(IniDispatch * dispatch, void * user_data);
};

} // namespace mdsd

#endif // __CONFIGINI_HH__

