#include <string>
#include "my_string.h"
#include "exception.h"
#include "config.h"
#include <sys/sysinfo.h>
#include "file.h"
#include <fstream>

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
  if (line!="") {
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
  unsigned intStartPos = strLine.find("inet addr:");

  if (intStartPos == strLine.npos) {
    my_throw("Error occured!");
  }

  intStartPos+=10;

  // Everything after the colon, up to the first space is the IP address
  unsigned intEndPos = strLine.find(" ", intStartPos);

  if (intStartPos == strLine.npos) {
    my_throw("Error occured!");
  }

  // Now extract the IP address
  strIPAddress = strLine.substr(intStartPos, intEndPos-intStartPos);

  return strIPAddress;
}

long get_uptime() {
#ifndef __linux__
  undefined_func_throw;
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
  undefined_func_throw;
#endif
}

unsigned process_instances(const string & strprocess_name) {
  // Use a linux 'ps -A' call to ask linux how many times a given pocess is already running
  FILE * fp = NULL;
  char line[130];   /* line of data from unix command*/
  unsigned Instances = 0;

  string ToRun = "ps -A | grep " + strprocess_name;
  fp = popen(ToRun.c_str(), "r");   /* Issue the command. */

  /* Read a line			*/
  while (fgets(line, sizeof line, fp))
  {
    // Decode the line
    string strLine = line;
    if (strLine.substr(24) == (strprocess_name + "\n")) Instances++;
  }
  pclose(fp);
  return Instances;
}

// Execution, capture output:
int system_capture_out(const string & COMMAND, string & strout) {
  // Do a regular system() call, but return the output out and error in string variables.
  string strout_file= (string)"/tmp/." + PACKAGE + "_output_" + itostr(getpid()) + "_out.txt";
  remove (strout_file.c_str()); // In case it already exists.
  int intret = system(string((string)COMMAND + " &> " + strout_file).c_str());
  strout = ""; // Clear the output
  if (!file_exists(strout_file)) {
    // Output file not found!
    my_throw("Could not find output file " + strout_file + "!");
  }

  // Now read the file into the return string;
  ifstream infile(strout_file.c_str());
  if (!infile) {
    my_throw("Could not open " + strout_file + "!");
    return -1;
  }

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

  // Remove the output file:
  remove(strout_file.c_str());

  return intret;
}

void restart_linux() {
  // Restart the PC
  system("reboot");
}

// Call malloc, check the return results, thrown an exception if an error occurs
void * xmalloc(size_t size) {
  // Call malloc, check the return results, thrown an exception if an error occurs
  // Logic and idea for xmalloc ripped from libc.txt...
  register void *value = malloc (size);
  if (value == 0) {
    my_throw("virtual memory exhausted");
  }
  return value; // Execution reached this point, return the pointer from malloc.
}
