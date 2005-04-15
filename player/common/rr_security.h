
#ifndef RR_SECURITY_H
#define RR_SECURITY_H

#include <string>
  
// Makes use of OpenSSL
// Make sure that you link with the "ssl" library.

// Utility function:

using namespace std;

const unsigned char rr_encryption_ver = 1; // Used when the version is included in an encrypted buffer.
                                                                     // Value can be from 0 to 255.

string get_rr_encrypt_key(); // Returns a standard key to use for encryption and decryption.
                             // Recommended: Use random key "modifier" characters at the start of your
                             // buffers. The key mod characters are prepended to the encryption key
                             // just before encryption.

// Buffer encryption.

class buffer_encryption {
public:  
  // Constructor
  buffer_encryption(unsigned char * buffer, const int intbuffer_len);

  // blnDecryptCheckChars - if this is set to true in the encrypt and decrypt methods, then
  // extra "leading" characters are used (the number matches "intNumLeadingKeyModChars")
  
  // Encryption
  void encrypt(const string & strKey, const int intNumLeadingKeyModChars, const bool blnDecryptCheckChars); // strKey is modified by any leading key mod chars in the buffer..
  // Decryption
  void decrypt(const string & strKey, const int intNumLeadingKeyModChars, const bool blnDecryptCheckChars); // strKey is modified by any leading key mod chars in the buffer..
private:
  unsigned char * Buffer;
  int intBuffLen;
  int intLeadingKeyModChars;  
};

// Use this function to determine how many leading chars will be required in the buffer, with
// a given set of encryption options...
int get_num_buffer_encrypt_leading_chars(const int intNumLeadingKeyModChars, bool blnDecryptCheckChars);

// String encryption

// These characters will be used for encrypted strings:
const string strencrypted_string_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";

// String encryption:
string encrypt_string(const string & str, const string & strkey, const int intleading_key_mod_chars);
string decrypt_string(const string & str, const string & strkey, const int intleading_key_mod_chars);

// File encryption:
void encrypt_file(const string & strFile, const string & strPassword);
void decrypt_file(const string & strFile);

#endif

