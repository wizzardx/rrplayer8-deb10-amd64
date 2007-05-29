#include <string>
#include "my_string.h"
#include "exception.h"
#include "system.h"
#include "file.h"

using namespace std;

void net_send(const string & strDest, const string & strMessage) {
  // Send a "NET SEND" popup message to a Windows user.

  // Valid arguments?
  if (trim(strDest) == "") {
    my_throw("User not specified!");
  }

  if (trim(strMessage) == "") {
    my_throw("Message not specified!");
  }

  // Does the smbclient binary exist?
  if (!file_exists("/usr/bin/smbclient")) {
    my_throw("smbclient not installed!");
  }

  // Create and run the NET SEND command:
  string strcmd = "echo \"" + strMessage + "\" | /usr/bin/smbclient -M " + strDest;
  string strcmd_out; // Output of the executed command.

  if (system_capture_out(strcmd, strcmd_out) != 0) {
    my_throw("smbclient: " + strcmd_out);
  }
}
