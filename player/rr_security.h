/***************************************************************************
                          rr_security.h  -  description
                             -------------------
    version              : v0.05
    begin                : Wed Oct 1 2003
    copyright            : (C) 2003 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef RR_SECURITY_H
#define RR_SECURITY_H
#define RR_SECURITY_H_VERSION 5 // Meaning 0.05

/**
  *@author David Purdy
  */

#include <string>
#include "check_library_versions.h" // Always last: Check the versions of included libraries.
  
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

class string_encryption {
public:  
  string_encryption(string & strstring);

  // Encryption
  void encrypt(const string & strKey, const int intNumLeadingKeyModChars); // random characters are generated for the leading key mod chars...
  // Decryption
  void decrypt(const string & strKey, const int intNumLeadingKeyModChars); // strKey is modified by any leading key mod chars in the buffer..
private:
  string & strString;
};

// File encryption:

void encrypt_file(const string & strFile, const string & strPassword);
void decrypt_file(const string & strFile);

#endif

