/// @file
/// Basic support for windows-style INI file config loading and saving.

#ifndef CONFIG_FILE_H
#define CONFIG_FILE_J

#include <vector>
#include <map>
#include <string>

using namespace std;

// Basic support for windows-style INI file config loading and saving.

typedef map <const string, string> config_settings;

void list_config_file_sections(const string & strconfig_file,
                               vector <string> & sections);

void load_config_file_section(const string & strconfig_file,
                              const string & strsection,
                              config_settings & settings);

void save_config_file_section(const string & strconfig_file,
                              const string & strsection,
                              config_settings & settings);

/// Check a set of settings for a given setting. If the setting is found, then
/// return the value, otherwise, return a default string:
string fetch_config_setting(config_settings & settings,
                            const string & strsetting,
                            const string & strdefault);

#endif
