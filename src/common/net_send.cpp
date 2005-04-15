
void net_send(const string & strDest, const string & strMessage) {
  // Send a "NET SEND" popup message to a Windows user.

  // Valid arguments?
  if (trim(strDest) == "") {
    rr_throw("User not specified!");
  }

  if (trim(strMessage) == "") {
    rr_throw("Message not specified!");
  }

  // Does the smbclient binary exist?
  if (!FileExists("/usr/bin/smbclient")) {
    rr_throw("smbclient not installed!");
  }

  // Create and run the NET SEND command:
  string strcmd = "echo \"" + strMessage + "\" | /usr/bin/smbclient -M " + strDest;
  string strcmd_out; // Output of the executed command.
  
  if (system_capture_out(strcmd, strcmd_out) != 0) {
    rr_throw("smbclient: " + strcmd_out);
  }
}
