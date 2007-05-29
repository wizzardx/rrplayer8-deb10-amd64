#include "string_search_script.h"
#include <vector>
#include "string_splitter.h"
#include "string.h"
#include "testing.h"

// Types & constants used by run_find_script_class:

  // A definition of the commands accepted by run_find_script
  struct cmd_template {
    string cmd;   // eg "+=" (add to)
    int num_args; // eg: 2
    string arg_types; // eg "I, I" or "S S"
    string strtemplate; // A unique description (without spaces).
  };                    // eg: "append_string", or "add_int"
                        // ie - depending on the arg types, run different
                        //      logic.

  // The last element will always have a "" cmd - that is how functions will
  // know they have reached the last element!
  const cmd_template cmd_templates[] = {
    {"sf",     1, "S",    "search_forwards"},
    {"sb",     1, "S",    "search_backwards"},
    {"+",      1, "I",    "inc_pos"},
    {"-",      1, "I",    "dec_pos"},
    {"sbs",    0, "",     "set_block_start"},
    {"sbe",    0, "",     "set_block_end"},
    {"ret",    1, "S",    "return_string"},
    {"=",      2, "VS S", "assign_string"},
    {"=",      2, "VI I", "assign_int"},
    {"+=",     2, "VI I", "increment_int"},
    {"+=",     2, "VS S", "append_string"},
    {"-=",     2, "VI I", "decrement_int"},
    {"print",  1, "S",    "print_string"},

    {"", 0, "", ""} // The final element - ie, there are no
                    // more command templates!!
  };

// A class used by run_search_script():

class run_search_script_class {
public:
  // Constructor: Takes the string to be searched;
  run_search_script_class(const string & strbuff) : strbuffer(strbuff) {
    reset_vars(); // Reset user variables, and status vars
  };

  // Main action is run here:
  void run_search_script(const string & strsearch_script, string & strresult) {
    strresult = ""; // Found string

    // Extract the script into individual commands:
    string_splitter command_split(strsearch_script, ";", '"');

    // Now process the commands, one at a time.
    int intcmd_index = 0;

    while ((!blnstring_found) && command_split) {
      string strcmd = command_split;
      parse_command(trim(strcmd));
    }

    // Now we have all the results.
    if (!blnstring_found) my_throw("Could not find the string!");

    strresult = strfound_string;
  }
private:
  // The string to be searched
  string strbuffer;

  // Status variables:
  bool blnstring_found; // Set to True when the sub-string string is found
  string strfound_string; // The string that was found

  // User variables:
  struct {
    string S[10];  // 10 strings for the user
    int    I[10];  // 10 integers for the user
  } user_vars;

  // Various other status:
  int intblock_start; // The start of a block of text - can be marked by "sbs"
  int intblock_end;   // The end of a block of text - can be marked by "sbe"

  int intpos;  // The current position within the buffer.

  // Reset user vars and also the object's current status:
  void reset_vars() {
    blnstring_found = false;
    strfound_string = "";

    // User vars
    for (int i=0; i++; i < 10) {
      user_vars.S[i] = "";
      user_vars.I[i] = -1;
    }

    // Other status vars;
    intblock_start = -1;
    intblock_end = -1;

    intpos = -1;
  }

  // Check strings like "I2", "S0", etc, and see if they represent valid var
  // strings
  bool is_int_var(const string & strvar) {
    return (strvar.length() == 2 && strvar[0] == 'I' && isdigit(strvar[1]));
  }

  bool is_string_var(const string & strvar) {
    // Return true
    return (strvar == "B") ||
           (strvar.length() == 2 && strvar[0] == 'S' && isdigit(strvar[1]));
  }

  string get_user_var(const string & strvar) {
    string strret=""; // Value of the variable.
    bool blnunknown = false; // Set to true if it is determined that the user
                             // variable is not known.
    if (strvar == "B") {
      // Attempt to fetch the current block
      if ((intblock_start != -1) && (intblock_end != -1) &&
          (intblock_start <= intblock_end)) {
        strret = substr(strbuffer, intblock_start, intblock_end - intblock_start + 1);
      }
    }
    else if (strvar == "P") {
      // Fetch the current position
      undefined_throw;
    }
    else if (strvar.length() == 2) {
      // A 2-character variable? Check if it is an Integer or a String var.
      if ((strvar[0] == 'I' || strvar[0] == 'S') &&
           (isdigit(strvar[1]))) {
        // This variable is known. Return the value currently stored there.
        switch (strvar[0]) {
          case 'I': {
            strret = itostr(user_vars.I[strtoi(substr(strvar, 1))]);
          } break;
          case 'S': {
            strret = user_vars.S[strtoi(substr(strvar, 1))];
          } break;
          default: LOGIC_ERRROR;
        }
      } else my_throw("Unknown variable: " + strvar);
    }
    else my_throw("Unknown variable: " + strvar);
    return strret;
  }

  void set_user_var(const string & strvar, const string & strvalue) {
    if (strvar.length() != 2) my_throw("Unknown variable: " + strvar);

    // A 2-character variable? Check if it is an Integer or a String var.
    if ((strvar[0] == 'I' || strvar[0] == 'S') && (isdigit(strvar[1]))) {
      // This variable is known. Set the value
      switch (strvar[0]) {
        case 'I': {
          // An integer.
          if (!isint(strvalue)) {
            my_throw("Cannot assign non-int value '" + strvalue + "' to '" + strvar + "'!");
          }
          user_vars.I[strtoi(substr(strvar, 1))] = strtoi(strvalue);
        } break;
        case 'S': {
          // A string
          user_vars.S[strtoi(substr(strvar, 1))] = strvalue;
        } break;
        default: LOGIC_ERROR; // Unknown variable type
      }
    } else my_throw("Unknown variable: " + strvar);
  }

  // This function compares the passed command 'strcmd' against cmd_templates,
  // and in the process builds up a simpler version of the command.
  string translate_command(const string strcmd) {
    string strtranslated_cmd="";
    bool blncmd_found = false; // Set to true if the matching template is found.
    strtranslated_cmd = "";    // Clear the output string.

    // Break the command into parts: First part is the command,
    // remaining parts are arguments.
    string_splitter command_parts(strcmd, " ", '"');

    // Check how many parts were extracted:
    if (command_parts.count() < 1) my_throw("An error occured finding the command in instruction " + strcmd);

    // Command parts were extracted.
    // Now check the command against the list of command templates.
    int cmd_list_index=0;
    while (!blncmd_found && cmd_templates[cmd_list_index].cmd != "")  {
      // Does the command name & # of args match?
      string strcmd_name = command_parts[0];

      if ((cmd_templates[cmd_list_index].cmd      == strcmd_name) &&
          (cmd_templates[cmd_list_index].num_args == command_parts.count() - 1)) {

        // Now compare the cmd template's arg types against the
        // cmd. Also see if typecasting is possible.
        string_splitter template_args(cmd_templates[cmd_list_index].arg_types, " ");

        // Quick check: Did we extract the specified number of args from
        // the template?
        if (template_args.count() != cmd_templates[cmd_list_index].num_args) {
          my_throw("An error in command template '"
                 + cmd_templates[cmd_list_index].strtemplate
                 + "'! An invalid # of arg types were found");
        }

        // Correct number of template arg types found.
        // Now compare each of the arg types, and skip out as soon
        // as a non-matching template arg type is found.
        bool blnmismatch = false; // Set to true as soon as an arg is found
                                  // where the type does not match the
                                  // template.
        string strtemp_args = ""; // As arguments are successfully matched,
                                  // they are appended to this string.
        int int_template_arg_index = 0;
        while (!blnmismatch &&
               int_template_arg_index <
               cmd_templates[cmd_list_index].num_args) {

          string strtemplate_arg_type = template_args[int_template_arg_index];
          string strcmd_arg = command_parts[int_template_arg_index + 1];

          if (strtemplate_arg_type == "I") {
            // This cmd template arg is an integer
            // Take the command's arg and see if we can
            // convert it into this type
            string strtemp = "";
            try {
              strtemp_args += " " + itostr(fetch_int_value(strcmd_arg));
            } catch(...) {
              // Could not convert the command's arg
              blnmismatch = true;
            }
          }
          else if (strtemplate_arg_type == "S") {
            // This cmd template arg is a string
            // Take the command's arg and see if we can
            // convert it into this type
            // - If we have problems converting to a string then throw the exception!
            string strtemp = "";
            strtemp = fetch_string_value(strcmd_arg);
            strtemp = quote_string(strtemp, '"');
            strtemp_args += " " + strtemp;
          }
          else if (strtemplate_arg_type == "VI") {
            // This cmd template arg is an Integer Var.
            // Check the command's arg and see if it is in fact an integer
            // variable.
            if (is_int_var(strcmd_arg)) {
              strtemp_args += " " + strcmd_arg;
            }
            else {
              blnmismatch = true;
            }
          }
          else if (strtemplate_arg_type == "VS") {
            // This cmd template arg is a String Var.
            // Check the command's arg and see if it is in fact a string
            // variable.
            if (is_string_var(strcmd_arg)) {
              strtemp_args += " " + strcmd_arg;
            }
            else {
              blnmismatch = true;
            }
          }
          else {
            // Unknown argument type found in the cmd template definition!
            my_throw("An error in command template '"
                     + cmd_templates[cmd_list_index].strtemplate
                     + "'! An unknown argument type was found!");
          }
          // Go to the next template argument:
          ++int_template_arg_index;
        }

        // If there were no argument type mismatches or errors, then we have found
        // a matching command template. At the same time we have generated
        // simplified info for an easier-to-parse command string.
        if (!blnmismatch) {
          // Set the "simplified" command string:
          strtranslated_cmd += cmd_templates[cmd_list_index].strtemplate;
          strtranslated_cmd += strtemp_args;
          // We have now found a matching command template:
          blncmd_found = true;
        }
      }
      ++cmd_list_index; // Go to the next command template
    }

    // Did we find a matching command template?
    if (!blncmd_found) {
      // Could not find a matching command template!
      my_throw("I do not understand this command! (no matching template): "
               + strcmd);
    }
    return strtranslated_cmd;
  }

   // Run a command that was translated by translate_command
  void run_translated_cmd(const string & strtranslated_cmd) {
    string_splitter cmd_parts(strtranslated_cmd, " ", '"');
    if (cmd_parts.count() < 1) my_throw("Error! Could not extract the parts of a translated command!");

    string strcmd = cmd_parts[0];
    if (strcmd == "search_forwards") {
      if (cmd_parts.count() != 2) LOGIC_ERROR;
      string strsearch_for = unquote_string(cmd_parts[1], '"');
      if (intpos == -1) { intpos = 0; }
      int intnew_pos = strbuffer.find(strsearch_for, intpos);
      if (intnew_pos == strbuffer.npos) {
        my_throw("Could not find the text '" + strsearch_for + "'");
      }
      intpos = intnew_pos;
    }
    else if (strcmd == "search_backwards") {
      if (cmd_parts.count() != 2) LOGIC_ERROR;
      string strsearch_for = unquote_string(cmd_parts[1], '"');
      if (intpos == -1) { intpos = strbuffer.length() - 1; }
      int intnew_pos = strbuffer.rfind(strsearch_for, intpos);
      if (intnew_pos == strbuffer.npos) {
        my_throw("Could not find the text '" + strsearch_for + "'");
      }
      intpos = intnew_pos;
    }
    else if (strcmd == "inc_pos") {
      if (cmd_parts.count() != 2) LOGIC_ERROR;
      int intval = strtoi(cmd_parts[1]);
      int intnew_pos = intpos + intval;
      if (intnew_pos < 0 || intnew_pos > strbuffer.length() - 1) {
        my_throw("Cannot increment position!");
      }
      intpos = intnew_pos;
    }
    else if (strcmd == "dec_pos") {
      if (cmd_parts.count() != 2) LOGIC_ERROR;
      int intval = strtoi(cmd_parts[1]);
      int intnew_pos = intpos - intval;
      if (intnew_pos < 0 || intnew_pos > strbuffer.length() - 1) {
        my_throw("Cannot decrement position!");
      }
      intpos = intnew_pos;
    }
    else if (strcmd == "set_block_start") {
      if (cmd_parts.count() != 1) LOGIC_ERROR;
      if (intpos == -1) my_throw("Cannot set block start! Current position not set.");
      intblock_start = intpos;
    }
    else if (strcmd == "set_block_end") {
      if (cmd_parts.count() != 1) LOGIC_ERROR;
      if (intpos == -1) my_throw("Cannot set block start! Current position not set.");
      intblock_end = intpos;
    }
    else if (strcmd == "return_string") {
      if (cmd_parts.count() != 2) LOGIC_ERROR;
      strfound_string = unquote_string(cmd_parts[1], '"');
      // End result of search was found!
      blnstring_found = true; // End the script loop
    }
    else if (strcmd == "assign_string") {
      if (cmd_parts.count() != 3) LOGIC_ERROR;
      string strvar = cmd_parts[1];
      string strvalue = unquote_string(cmd_parts[2], '"');
      set_user_var(strvar, strvalue);
    }
    else if (strcmd == "assign_int") {
      if (cmd_parts.count() != 3) LOGIC_ERROR;
      set_user_var(cmd_parts[1], cmd_parts[2]);
    }
    else if (strcmd == "increment_int") {
      if (cmd_parts.count() != 3) LOGIC_ERROR;
      string strvar = cmd_parts[1];
      int intvalue = strtoi(cmd_parts[2]);
      set_user_var(strvar, itostr(strtoi(get_user_var(strvar)) + intvalue));
    }
    else if (strcmd == "append_string") {
      if (cmd_parts.count() != 3) LOGIC_ERROR;
      string strvar = cmd_parts[1];
      string strvalue = unquote_string(cmd_parts[2], '"');
      set_user_var(strvar, get_user_var(strvar) + strvalue);
    }
    else if (strcmd == "decrement_int") {
      if (cmd_parts.count() != 3) LOGIC_ERROR;
      string strvar = cmd_parts[1];
      int intvalue = strtoi(cmd_parts[2]);
      set_user_var(strvar, itostr(strtoi(get_user_var(strvar)) - intvalue));
    }
    else if (strcmd == "print_string") {
      if (cmd_parts.count() != 2) LOGIC_ERROR;
      const string str = unquote_string(cmd_parts[1], '"');
      cout << "PRINT: '" << str << "'" << endl;
    }
    else if (strcmd == "") {
      my_throw("Empty command!");
    }
    else {
      // Unknown command
      my_throw("Unknown command! " + strcmd);
    }
  }

  // This string parses one of the user commands, eg: 'SF "hello world"'
  void parse_command(const string & strcmd) {
    // Translate the command: Ie, check the command, # and type of args,
    // amd find which "command template" is matched. Also, build up a
    // "translated" command.
    // eg: '= I1 S5' might translate to 'assign_int I1 1234', if variable
    // S5 contained the string "1234".
    string strtranslated_cmd = ""; // The simplified command.
    strtranslated_cmd = translate_command(strcmd);

    // Command was translated
    run_translated_cmd(strtranslated_cmd);
  }

  // Sub-functions for parsing the user-provided string:

  // Check args like 'I2', 'S2', '234', or '"1234"'. I2 would be an int var: Return
  // the value directly. S2 would be a string var: See if the string is a
  // valid int, if so, return. '234' is a plain int, return as is, "1234" is
  // a quoted string: Parse and see if it contains a valid int.

  int fetch_int_value(const string & strvar) {
    int intret = 0;
    bool blnquote_error;   // Set to true if a quoting problem is found.
    // Is this an integer variable? eg "I3"
    if (is_int_var(strvar)) {
      intret = strtoi(get_user_var(strvar));
    }
    // Is this a string variable? eg "S5"
    else if (is_string_var(strvar)) {
      string strvalue = get_user_var(strvar);
      if (!isint(strvalue)) {
        my_throw("Variable " + strvar + " does not contain an int!");
      }
      intret = strtoi(strvalue);
    }
    else if (is_quoted_string(strvar, '"', blnquote_error)) {
      string strtemp = unquote_string(strvar, '"');
      if (!isint(strtemp)) my_throw("String " + strvar + " is not a valid integer!");
      intret = strtoi(strtemp);
    }
    // Is this a plain integer?
    else if (isint(strvar)) {
      intret = strtoi(strvar);
    }
    else {
      // An invalid argument!
      my_throw("Could not retrieve integer value from [" + strvar + "]");
    }
    return intret;
  }

  // Same as above, but all values are allowed for strings.
  string fetch_string_value(const string & strvar) {
    string strret = "";
    bool blnquote_error; // Set to true if a quoting problem is found.
    if (is_string_var(strvar) || is_int_var(strvar)) {
      strret = get_user_var(strvar);
    }
    else if (is_quoted_string(strvar, '"', blnquote_error)) {
      strret = unquote_string(strvar, '"');
    }
    else if (isint(strvar)) {
      strret = strvar;
    }
    else {
      // An invalid argument!
      my_throw("Could not retrieve string value from [" + strvar + "]");
    }
    return strret;
  }
};

// A specialised string search function
void run_search_script(const string & strbuffer, const string & strsearch_script, string & strresult) {
  run_search_script_class RFS(strbuffer);
  RFS.run_search_script(strsearch_script, strresult);
}

