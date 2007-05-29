/// @file
/// Utility function for sending e-mails

/// Sends an e-mail. Constructs a buffer file and then calls exim to send the mail.
void send_email(const string & strFrom, const string & strTo, const string & strSubject, const string & strBody);
