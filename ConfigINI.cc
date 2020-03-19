#include "ConfigINI.hh"
#include <fstream>
#include <iostream>
#include <exception>
#include <confini.h>
#include <map>

using namespace mdsd;
using namespace std;


ConfigINI::ConfigINI()
{
}

ConfigINI::~ConfigINI()
{
}

void ConfigINI::Parse(const std::string& path, IniParsedDataMap& data)
{
    /*  The format of the INI file  */
    IniFormat iniformat = INI_DEFAULT_FORMAT;
    /*  Load the INI file  */
    // intialize with root section
    data[ROOT_SECTION_NAME] = map<string,string>();
    auto loaded_ini = load_ini_path(path.c_str(), iniformat, NULL, &ConfigINI::ListenerCallback, &data);
    if (loaded_ini) {

        fprintf(stderr, "Sorry, something went wrong :-(\n");
        return;

    }
}


int ConfigINI::ListenerCallback(IniDispatch * dispatch, void * user_data)
{
    IniParsedDataMap* data = (IniParsedDataMap *) user_data;

    // printf(
    //     "\ndispatch_id: %zu\n"
    //     "format: {IniFormat}\n"
    //     "type: %u\n"
    //     "data: |%s|\n"
    //     "value: |%s|\n"
    //     "append_to: |%s|\n"
    //     "d_len: %zu\n"
    //     "v_len: %zu\n"
    //     "at_len: %zu\n",

    //     dispatch->dispatch_id,
    //     dispatch->type,
    //     dispatch->data,
    //     dispatch->value,
    //     *dispatch->append_to ? dispatch->append_to : ROOT_SECTION_NAME,
    //     dispatch->d_len,
    //     dispatch->v_len,
    //     dispatch->at_len
    // );
    
    switch (dispatch->type)
    {
        case INI_SECTION:
        {
            if(data->find(dispatch->data) == data->end())
            {
                (*data)[dispatch->data] = map<string,string>();
            }
            break;
        }
        case INI_KEY:
        {
            auto appent_to = *dispatch->append_to ? dispatch->append_to : ROOT_SECTION_NAME;
            auto section = data->find(appent_to);
            if(section == data->end())
                break;
            
            section->second[dispatch->data] = dispatch->value;
            break;
        }
        default:
            std::cout << "ConfigINI: Unkown INI type: " << dispatch->type << std::endl;
            break;
    }
  return 0;
}