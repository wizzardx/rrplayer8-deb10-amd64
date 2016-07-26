#include <string>
#include "my_string.h"
#include "exception.h"
#include "file.h"
#include <fstream>
#include "system.h"
#include "string_splitter.h"

#ifdef __linux__
  #include <sys/sysinfo.h>
  #include "config.h"
#else
  #include <io.h>
  #include "testing.h"
#endif

#include <unistd.h>

using namespace std;

// System info
string get_hostname() {
  // Use a linux 'ifconfig ' call to ask linux for the machine's IP address
  FILE *fp = NULL;
  char line[130] = "";   /* line of data from unix command* */

  string strHostname = ""; // Intermediate variable for return result.

  // Set a default IP address return value in case we exit with an error
  strHostname = "[Can't find hostname]";

  // Run hostname
  fp = popen("/bin/hostname", "r"); // Run the command

  // Read the 1st (and only) line (if popen doesn't work,
  // fgets doesn't return anything
  fgets(line, sizeof line, fp); // Line 1

  // Close the pipe now that we have the wanted line
  pclose(fp);

  // Now return the line
  if (strlen(line) != 0) {
    strHostname=line;
    // Strip the last character from the line (usually a newline)
    strHostname=substr(strHostname, 0, strHostname.length()-1);
  }
  // Now return the value
  return strHostname;
}

string get_ip() {
  // Use a linux 'ifconfig ' call to ask linux for the machine's IP address

  FILE *fp = NULL;
  char line[130] = "";   /* line of data from unix command* */

  // Set a default IP address return value in case we exit with an error
  string strIPAddress = "[Can't find IP]";

  // Run ifconfig
  fp = popen("/sbin/ifconfig", "r"); // Run the command

  // Read the 2 lines (if popen doesn't work,
  // fgets doesn't return anything
  fgets(line, sizeof line, fp); // Line 1
  fgets(line, sizeof line, fp); // Line 2

  // Close the pipe now that we have the wanted line
  pclose(fp);

  // Now decode the line read in
  // - format of the line read...
  //    "inet addr:172.30.166.59  Bcast:172.30.166.255  Mask:255.255.255.0"

  string strLine = line;
  // Is there a string segment "inet addr:" ?
  size_t intStartPos = strLine.find("inet addr:");

  if (intStartPos == strLine.npos) {
    my_throw("Error occured!");
  }

  intStartPos+=10;

  // Everything after the colon, up to the first space is the IP address
  size_t intEndPos = strLine.find(" ", intStartPos);

  if (intStartPos == strLine.npos) {
    my_throw("Error occured!");
  }

  // Now extract the IP address
  strIPAddress = strLine.substr(intStartPos, intEndPos-intStartPos);

  return strIPAddress;
}

long get_uptime() {
#ifndef __linux__
  undefined_throw;
#else
  // Return how many seconds the system has been running for
  struct sysinfo si;
  int result = sysinfo(&si);
  if(result != 0) {
    my_throw("Abnormal info retrieval... result = " + itostr(result));
  }
  return si.uptime;
#endif
}

bool pid_exists(const long lngpid) {
#ifdef __linux__
  return dir_exists("/proc/" + itostr(lngpid)+ "/");
#else
  undefined_throw;
#endif
}

unsigned process_instances(const string & strprocess_name) {
  // Use the linux tool "pidof" to fetch a list of PIDs for the specified
  // process. Count them and return the count
  int intret = 0;
  {
    string stroutput;
    try {
      system_capture_out_throw("/bin/pidof " + strprocess_name, stroutput);
    } catch (...) {
      // pidof returns an error code but no output if the process
      // is not running.
      if (stroutput != "") throw;
    }
    string_splitter pids(stroutput);
    intret = pids.size();
  }
  return intret;
}

// Execution, capture output:
int system_capture_out(const string & COMMAND, string & strout) {
#ifndef __linux__
  cout << COMMAND << endl;
  undefined_throw;
#else
  // Clear the output:
  strout = "";
  // Do a regular system() call, but return the output out and error in string variables.
  string strout_file = (string)"/tmp/." + PACKAGE + "_output_" + itostr(getpid()) + "_out.txt";
  string strresult_file = (string)"/tmp/." + PACKAGE + "_result_" + itostr(getpid()) + "_result.txt";
  string strcmd = COMMAND + " > " + strout_file + " 2>&1; echo $? > " + strresult_file;

  // When there is an error, system() is returning 256 instead of the real error code!
  // So piping the real result into a text file and reading it instead of using
  // system()'s return code.
  system(strcmd.c_str());

  if (!file_exists(strout_file))    my_throw("Could not find output file " + strout_file + "!\n - Command was: " + strcmd);
  if (!file_exists(strresult_file)) my_throw("Could not find result file " + strresult_file + "!\n - Command was: " + strcmd);

  // Read the output file into the return string;
  {
    ifstream infile(strout_file.c_str());
    if (!infile) my_throw("Could not open " + strout_file + "!");

    // Surely there is a better way to read the entire file into the buffer...:
    int intlinenum = 0;
    string strline = "";
    while (getline(infile, strline)) {
      ++intlinenum;
      if (intlinenum > 1) {
        strout += "\n";
      }
      strout += strline;
    }
    infile.close();
  }

  // Remove the output file:
  CHECK_LIBC(remove(strout_file.c_str()), "rm: " + strout_file);

  // Read the result file into the return code:
  int intret;
  {
    ifstream infile(strresult_file.c_str());
    if (!infile) my_throw("Could not open " + strout_file + "!");

    // Read the first line, then close the file
    string strline;
    getline(infile, strline);
    infile.close();

    // Attempt to use this as the return code:
    intret = strtoi(strline);
  }

  // Remove the result file:
  CHECK_LIBC(remove(strresult_file.c_str()), "rm: " + strresult_file);

  // Return the return code:
  return intret;
#endif
}

// Same as above, but throw a descriptive exception if the result is non-zero
void system_capture_out_throw(const string & COMMAND, string & strout) {
  int intresult = system_capture_out(COMMAND, strout);
  if (intresult != 0) my_throw("Failed: \"" + COMMAND + "\". Code: " + itostr(intresult) + ". Returned: \"" + strout + "\"");
}

void restart_linux() {
  // Restart the PC
  system("reboot");
}

// Call malloc, check the return results, throw an exception if an error occurs
void * xmalloc(size_t size) {
  // Logic and idea for xmalloc ripped from libc.txt...
  register void *value = malloc (size);
  if (value == 0) {
    my_throw("virtual memory exhausted");
  }
  return value; // Execution reached this point, return the pointer from malloc.
}
