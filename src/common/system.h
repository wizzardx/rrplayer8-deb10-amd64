/// @file
/// General system-related functions.

// System info
string get_hostname();
string get_ip();
long get_uptime();
bool pid_exists(const long lngpid);
unsigned process_instances(const string & strprocess_name);

// Execution, capture output:
int system_capture_out(const string & COMMAND, string & strout);

// Same as above, but throw a descriptive exception if the result is non-zero
void system_capture_out_throw(const string & COMMAND, string & strout);

void restart_linux();
void * xmalloc(size_t size); ///< Call malloc, check the return results, thrown an exception if an error occurs

/// A conveniance macro for sleeping for a number of milliseconds:
#define sleep_ms(MS) usleep(MS*1000)

#define debug_print_mem_usage system(string("cat /proc/" + itostr(getpid()) + "/status | grep VmSize").c_str())
