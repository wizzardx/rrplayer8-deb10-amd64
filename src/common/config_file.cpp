// Basic support for windows-style INI file config loading and saving.

#include "config_file.h"
#include <string>
#include <fstream> // ifstream
#include "exception.h"
#include "my_string.h"
#include "file.h"

void list_config_file_sections(const string & strconfig_file, 
                               vector <string> & sections) {
  // Open a config file, search for the sections, and list them into a
  // string vector. Return true if this operation is successful.
  sections.clear();
  ifstream config_file(strconfig_file.c_str());
  if (!config_file) my_throw("Unable to open config file " + strconfig_file);

  string strline;
  while (getline(config_file, strline)) {
    // Trim the line
    strline = trim(strline);
      
    // Ignore the line if it's empty or starts with a #
    if ((strline != "") && (left(strline, 1) != "#")) {
      // Is this a section line? (eg: "[Setting]"
      if ((left(strline, 1) == "[") && (right(strline, 1) == "]")) {
        // Is this the section we're looking for?
        string strname = trim(substr(strline, 1, strline.length() - 2));
        if (strname != "") {
          sections.push_back(strname);
        }
      }
    }
  }
}  

void load_config_file_section(const string & strconfig_file, 
                              const string & strsection, 
                              config_settings & settings) {
  // Returns true if the requested section is found.
  ifstream config_file(strconfig_file.c_str());
  if (!config_file) my_throw("Unable to open config file " + strconfig_file);
  
  bool blndone = false; // Set to true if we can exit the file-reading
                        // loop early
  bool blnin_section = false; // Set to true when we're in the section
                              // for settings to be extracted from.
  string strline;
  int intlinenum=0;
  while ((!blndone) && (getline(config_file, strline))) {
    // Trim the line
    strline = trim(strline);
    ++intlinenum;
      
    // Ignore the line if it's empty or starts with a #
    if ((strline != "") && (left(strline, 1) != "#")) {
      // Is this a section line? (eg: "[Setting]"
      if ((left(strline, 1) == "[") && (right(strline, 1) == "]")) {
        // Are we in the needed section already?
        if (blnin_section) {
          // We left the section
          blnin_section = false;
          blndone = true;
        }
        else {
          // Is this the section we're looking for?
          string strname = trim(substr(strline, 1, strline.length() - 2));
          if (lcase(strname) == lcase(strsection)) {
            blnin_section = true;
          }
        }
      }
      else {
        // Line is not a section declaration.
        // Are we in the correct section?
        if (blnin_section) {
          // - Look for an assignment operator: =
          unsigned equals_pos = strline.find("=", 0);
          if (equals_pos == strline.npos) {
            my_throw("Invalid line " + itostr(intlinenum) + " in " + strconfig_file 
                     + " - no '=' character");
          }
          
          string strsetting=lcase(trim(substr(strline, 0, equals_pos)));
          string strvalue=trim(substr(strline, equals_pos+1));
          settings[strsetting] = strvalue;
        }
      }
    }
  }
  
  // Check: Did we find the section?
  if (!(blnin_section || blndone)) my_throw("Could not find the requested config file section: " + strsection);
}

void write_config_file_section(ofstream & config_file, 
                              const string & strsection, 
                              config_settings & settings) {
  // This function is called by save_config_file_section, to write
  // a configuration file section to an open file stream.
  
  // Write the section
  config_file << "[" << strsection << "]" << endl;
  
  // Now run through the config_settings collection and write the members
  config_settings::const_iterator config_it = settings.begin();
  while (config_it != settings.end()) {
    config_file << (*config_it).first << "=" << (*config_it).second << endl;
    ++config_it;
  }
  
  // Also write an empty line after the section
  config_file << endl;
}


void save_config_file_section(const string & strconfig_file, 
                              const string & strsection, 
                              config_settings & settings) {

  // Make a copy of the configuration:
  config_settings settings_tmp(settings);

  // Open the original config file:
  ifstream config_file_old(strconfig_file.c_str());
  if (!config_file_old) my_throw("Unable to open config file " + strconfig_file);
  
  // Open a new, temporary config file:
  string strconfig_file_tmp = strconfig_file + ".tmp";
  ofstream config_file_new(strconfig_file_tmp.c_str());
  if (!config_file_new) my_throw("Unable to create temporary config file " + strconfig_file_tmp);

  // Process the input file
  bool blnin_section = false; // Set to true when we are in the specified
                              // configuration file section.
  bool blnsection_written = false; // Set to true when the section is saved;
  string strline;
  while (getline(config_file_old, strline)) {
    // Trim the line
    strline = trim(strline);

    // Check if it's a section declaration:
    if ((left(strline, 1) == "[") && (right(strline, 1) == "]")) {
      // Are we in the section already?
      if (blnin_section) {
        // We left the section
        blnin_section = false;
      }
      else {
        // Is this the section we're looking for?
        string strname = trim(substr(strline, 1, strline.length() - 2));
        if (lcase(strname) == lcase(strsection)) {
          blnin_section = true;
              
          // We're in the section now in the input file. Write the section
          write_config_file_section(config_file_new, strsection, settings);
          blnsection_written = true;              
        }            
      }         
    }
        
    // If we're not in the section, copy the line to the new tmp file.
    // (All of the section lines are rewritten from scratch by another
    // function)
    if (!blnin_section) {
      config_file_new << strline << endl;          
    }
  }
      
  // If at the end of the config file we haven't written the settings yet,
  // output an empty line and then the section.
  if (!blnsection_written) {
    write_config_file_section(config_file_new, strsection, settings);
    blnsection_written = true;
  }
      
  // Close the file i/o streams
  config_file_old.close();
  config_file_new.close();

  // Replace the old config file with the new config file.      
  rm(strconfig_file.c_str());
  mv(strconfig_file_tmp, strconfig_file);
}

// Check a set of settings for a given setting. If the setting is found, then
// return the value, otherwise, return a default string:
string fetch_config_setting(config_settings & settings, 
                            const string & strsetting,
                            const string & strdefault) {
  string strret = "";
  config_settings::const_iterator it_config;
  it_config = settings.find(strsetting);

  if (it_config == settings.end()) {
    // Setting not found: Return the default string
    strret == strdefault;
  }
  else {
    // Setting found: Return the value.
    strret =  (*it_config).second;
  }
  return strret;
}

