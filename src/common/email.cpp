#include "file.h"
#include "exception.h"
#include "temp_dir.h"
#include "system.h"
#include <fstream>

void send_email(const string & strFrom, const string & strTo, const string & strSubject, const string & strBody) {
  // This function sends an e-mail...

  // Check: Is exim installed?
  if(!file_exists("/usr/sbin/exim")) {
    my_throw("exim is not installed!");
  }

  // Create a temporary directory to work from:
  temp_dir email_dir("Temporary email directory");

  // Create a file to send the email out of:
  string stremail_file = (string)email_dir + "email.txt";

  ofstream BufferFile;
  BufferFile.open(stremail_file.c_str(), ios::out);

  // Were we able to open the file?
  if (!BufferFile) {
    my_throw("Unable to open temporary e-mail buffer file: " + stremail_file);
  }

  // Write the header fields to instruct exim:
  BufferFile << "From: " << strFrom << endl;
  BufferFile << "Subject: " << strSubject << endl;
  // Also an extra newline to mark the beginning of the body
  BufferFile << endl;

  // Append the body of the email:
  BufferFile << strBody << endl;

  // Close the e-mail buffer file:
  BufferFile.close();

  // Build a command to call exim:
  string strcmd = "/usr/sbin/exim -bm " + strTo + " < " + stremail_file;
  string strcmd_out = ""; // The output from exim.

  // Run the command:
  if (system_capture_out(strcmd, strcmd_out) != 0) {
    my_throw("exim: " + strcmd_out);
  }

  // The temp email file will be deleted by the email_dir object when it destructs...
}
