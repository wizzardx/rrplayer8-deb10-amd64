/***************************************************************************
                          rr_security.cpp  -  description
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

#ifdef __linux__ // blowfish encryption is only available under Linux at this time!

#include "rr_security.h"
#include <openssl/blowfish.h> // Need to have the "ssl" library linked
#include "rr_utils.h" // for log_error, itostr
#include "char_array_maths.h" // For char array manipulation.
#include "temp_dir.h"
#include "string_splitter.h"

using namespace rr;

/**************************************************
        Utility function:
**************************************************/

string get_rr_encrypt_key() {
  // Returns a standard key to use for encryption and decryption.
  // Recommended: Use random key "modifier" characters at the start of your
  // buffers. The key mod characters are prepended to the encryption key
  // just before encryption. Also use the "decrypt check characters" option.
  // Our code is "RR_8l0\\/\\/Ph1S|-|_K3Y". Use an encrypted version of this (using a key of "RRK3Y" to return the key)
  string strKey = "vLCxn9rieni58vC2B1VAs7i3D5FrXBwRUcG";

  // Indirectly generate the string "RRK3Y"
  char KeyDecoder[10];
  memset (KeyDecoder, 0, sizeof(KeyDecoder));
  KeyDecoder[2] = toupper('m'-2);
  KeyDecoder[0] = KeyDecoder[2] + 7;
  KeyDecoder[4] = toupper('z'-1);
  KeyDecoder[1] = toupper(KeyDecoder[4]-7);
  KeyDecoder[3] = toupper('9'-6);
  // Decrypt the encrypted key using the "RRK3Y" code, and return the final key
//  return "RR_8l0\\/\\/Ph1S|-|_K3Y";
  string_encryption RRKey(strKey);
  RRKey.decrypt(KeyDecoder, 2);

  return strKey;
}

/**************************************************
        Buffer encryption
**************************************************/

// Constructor
buffer_encryption::buffer_encryption(unsigned char * buffer, const int intbuffer_len) {
  // Reset the attributes
  Buffer = NULL;
  intBuffLen = 0;
  intLeadingKeyModChars = 0;

  // Is the buffer argument ok?
  if (buffer == NULL) {
    rr_throw("NULL buffer pointer");
  }
  // Is the buffer length argument ok?
  else if (intbuffer_len <= 0) {
    rr_throw("Buffer size is " + itostr(intbuffer_len));
  }
  else {
    // Arguments are ok, assign the object vars...
    Buffer = buffer;
    intBuffLen = intbuffer_len;
  }  
}

// Encryption
void buffer_encryption::encrypt(const string & strKey, const int intNumLeadingKeyModChars, const bool blnDecryptCheckChars) {
  // Is the buffer large enough for the requested encryption options?
  if (intBuffLen <= get_num_buffer_encrypt_leading_chars(intNumLeadingKeyModChars, blnDecryptCheckChars)) {
    // Buffer is not large enough for the requested encryption options...
    rr_throw("Buffer size is too small for the requested encryption options.");
  }
  else {
    // Buffer is large enough, proceed.
    // Set the leading non-zero random key modifier characters, andprepend to the key...
    string strTempKey = strKey;
    for (int i=0; i<intNumLeadingKeyModChars; i++) {
      char chrand = 0;
      while (chrand == 0) {
        chrand = rand() % 256;
      }
      strTempKey = (string) "" + chrand + strTempKey;
      Buffer[i] = chrand;

      // If decryption check characters are to be included, then set them now also...
      if (blnDecryptCheckChars) {
        Buffer[intNumLeadingKeyModChars + i] = chrand;
      }
    }

    // Create the blowfish BF_KEY structure from this key.
    BF_KEY blowfish_key;

    // NB: If the next line brings up an "undefined reference to 'BF_set_key' then you need to
    // include the ssl library in your project.
    BF_set_key(&blowfish_key, strTempKey.length(), (unsigned char *) strTempKey.c_str());

    // Do the blowfish stuff.
    unsigned char ivec[8]; // initialization vector. Is ok for this to be 0 even.
    memset(ivec, 0, sizeof(ivec));
    strcpy((char *)ivec, "RR");
    int intcounter = 0; // Used when we use multiple calls to BF_cfb64_encrypt...

    BF_cfb64_encrypt((unsigned char *) Buffer + intNumLeadingKeyModChars,
                                  (unsigned char *) Buffer + intNumLeadingKeyModChars,
                                  intBuffLen - intNumLeadingKeyModChars,
                                  &blowfish_key, ivec, &intcounter, BF_ENCRYPT);
  }
}

// Decryption
void buffer_encryption::decrypt(const string & strKey, const int intNumLeadingKeyModChars, const bool blnDecryptCheckChars) {
  // Is the buffer large enough to hold all the chars we will need?
  if (intBuffLen <=get_num_buffer_encrypt_leading_chars(intNumLeadingKeyModChars, blnDecryptCheckChars)) {
    // Buffer is too small for the encryption options!
    rr_throw("Buffer size is too small for the requested decryption options.");
  }
  else {
    // Buffer size is not too small...
    // Fetch the leading non-zero random key modifier characters, and prepend to the key...
    string strTempKey = strKey;
    for (int i=0; i<intNumLeadingKeyModChars; i++) {
      strTempKey = (string) "" + (char) Buffer[i] + strTempKey;
    }

    // Create the blowfish BF_KEY structure from this key.
    BF_KEY blowfish_key;

    // NB: If the next line brings up an "undefined reference to 'BF_set_key' then you need to
    // include the ssl library in your project.
    BF_set_key(&blowfish_key, strTempKey.length(), (unsigned char *) strTempKey.c_str());

    // Do the blowfish stuff.
    unsigned char ivec[8]; // initialization vector. Is ok for this to be 0 even.
    memset(ivec, 0, sizeof(ivec));
    strcpy((char *)ivec, "RR");
    int intcounter = 0; // Used when we use multiple calls to BF_cfb64_encrypt...

    BF_cfb64_encrypt((unsigned char *) Buffer + intNumLeadingKeyModChars,
                                  (unsigned char *) Buffer + intNumLeadingKeyModChars,
                                  intBuffLen - intNumLeadingKeyModChars,
                                  &blowfish_key, ivec, &intcounter, BF_DECRYPT);

    // If decryption check characters are specified, then check them now.They
    // occur at the start of the buffer. Their values should be exactly the same
    // as the leading random key mod characters...
    if (blnDecryptCheckChars) {
      int i = 0;
      while (i < intNumLeadingKeyModChars) {
        if (Buffer[i] != Buffer[i + intNumLeadingKeyModChars]) {
          rr_throw("Invalid encrypted data.");
        }
        i++;
      }
    }
  }
};

// Use this function to determine how many leading chars will be required in the buffer, with
// a given set of encryption options...
int get_num_buffer_encrypt_leading_chars(const int intNumLeadingKeyModChars, bool blnDecryptCheckChars) {
  int intRet = intNumLeadingKeyModChars;
  if (blnDecryptCheckChars) {
    intRet *= 2;
  }
  return intRet;
}

/**************************************************
        String encryption
**************************************************/

// Constructor
string_encryption::string_encryption(string & strstring) : strString(strstring) {
  if (strString == "") {
    rr_throw("Cannot perform encryption/decryption with an empty string!");
  }
}

// Encryption
void string_encryption::encrypt(const string & strKey, const int intNumLeadingKeyModChars) {
  // The string this object is operating on, is cleared if encryption fails.
  // Create and clear a buffer for encoding the string... (enclude a space for the final \0)
  // - Also include additional chars for the "decryption check characters"
  int intnum_leading_chars = get_num_buffer_encrypt_leading_chars(intNumLeadingKeyModChars, true);

  int intbuffer_size = strString.length() + intnum_leading_chars + 1;
  unsigned char * Buffer = (unsigned char *) alloca(intbuffer_size); // Allocate (auto-dealloc at function exit)
  memset(Buffer, 0, intbuffer_size); // Clear

  // Now copy the string over to the buffer... (including the final \0)
  memcpy(Buffer + intnum_leading_chars, strString.c_str(), strString.length() + 1);

  // Perform a buffer encryption, using the key (and the randomly generated leading random bytes)
  buffer_encryption BuffEnc(Buffer, intbuffer_size);
  BuffEnc.encrypt(strKey, intNumLeadingKeyModChars, true);
  
  // Encryption went fine
  // Now attempt to extract the encrypted buffer into the characters used for encrypted passwords..
  char_array_maths buffer_calc(Buffer, intbuffer_size);
  strString = buffer_calc.extract_string(strencrypted_string_chars, char_array_maths::char_repeat_allowed);
}

// Decryption
void string_encryption::decrypt(const string & strKey, const int intNumLeadingKeyModChars) {
  // The string this object is operating on, is cleared if decryption fails.
  // Create and clear a buffer for decoding the string...
  int intbuffer_size = strString.length();
  unsigned char * Buffer = (unsigned char *) alloca(intbuffer_size + 1); // Allocate (auto-dealloc at function exit)
  memset(Buffer, 0, intbuffer_size + 1); // Clear  (the +1 is for an extra \0 character, so that conversion to string
                                                              // ends at the correct character.

  // Extract the text into the buffer...
  char_array_maths buffer_calc(Buffer, intbuffer_size);

  buffer_calc.include_string(strString, strencrypted_string_chars, char_array_maths::char_repeat_allowed);

  // Buffer was successfully populated.
  // - Now decrypt the buffer, using the specified key and number of key mod chars.
  int intnum_leading_chars = get_num_buffer_encrypt_leading_chars(intNumLeadingKeyModChars, true);

  buffer_encryption BuffEnc(buffer_calc.first_used_char(), buffer_calc.num_used_chars());
  BuffEnc.decrypt(strKey, intNumLeadingKeyModChars, true);

  // Decryption went fine.
  // Copy the decrypted string to the return string...
  strString = (char *) (buffer_calc.first_used_char() + intnum_leading_chars);
}

void encrypt_file(const string & strFile, const string & strPassword) {
  // Create [file].rrcrypt, which is actually a tar file containing a
  // "ccrypt"-encrypted version of the file, and a control file containing
  // an "rrencrypt_string" version of the password to use for decryption.
  // - This is only secure because RR source code is closed source...

  // Check that the ccrypt utility is installed:
  if (!FileExists("/usr/bin/ccrypt")) rr_throw("ccrypt is not installed!");
  
  // File exists?
  if (!FileExists(strFile)) rr_throw("File not found: " + strFile);

  // Fetch the file's directory and base name:
  string strfile_dir, strfile_name;
  break_down_file_path(strFile, strfile_dir, strfile_name);

  // Create a temporary directory to work under:
  temp_dir work_dir("rrcrypt_work");

  // Make a copy of the file:
  cp(strFile, (string)work_dir + "file");

  // Export the password into the environment:
  setenv("RRCRYPT", strPassword.c_str(), 1);
  
  // Run ccrypt:
  int intresult = system(((string)"/usr/bin/ccrypt -e -E RRCRYPT " + (string)work_dir + "file").c_str());

  // Reset the password variable in the environment:
  unsetenv("RRCRYPT");
  
  // Check the output:
  if (intresult != 0) rr_throw("ccrypt had an error!");

  // Encrypt the password:
  string strEncPassword = strPassword;  
  string_encryption str(strEncPassword);
  str.encrypt(get_rr_encrypt_key(), 2);

  // Write the encrypted password to a control file used for decryption:
  ofstream TextFile;
  TextFile.open(((string)work_dir + "control").c_str(), ios::out);
  TextFile << "Encrypted-Password: " << strEncPassword << endl;
  TextFile.close();

  // Create a tar file in a different temporary directory:
  temp_dir tar_dir("rrcrypt_tar");

  string strprevdir=getcwd();
  chdir(((string)work_dir).c_str());
  int intret=system(((string)"/bin/tar -cf " + (string)tar_dir + strfile_name + ".rrcrypt *").c_str());
  chdir(strprevdir.c_str());
  if (intret != 0) rr_throw("tar had an error!");

  // Move the .rrcrypt file back to the original directory
  mv((string)tar_dir + strfile_name + ".rrcrypt", strfile_dir);

  // Remove the original, uncompressed file:
  rm(strFile);
}

void decrypt_file(const string & strFile) {
  // Decrypt a file created by "encrypt_file"
  // If strDestDir is specified then extract to that directory, otherwise
  // overwrite the original file.

  // Check that the ccrypt utility is installed:
  if (!FileExists("/usr/bin/ccrypt")) rr_throw("ccrypt is not installed!");

  // File exists?
  if (!FileExists(strFile)) rr_throw("File not found: " + strFile);

  // Fetch the file's directory and base name:
  string strfile_dir, strfile_name;
  break_down_file_path(strFile, strfile_dir, strfile_name);

  // Check for ".rrcrypt" at the end of the filename:
  if (substr(strFile, strFile.size()-8) != ".rrcrypt") rr_throw("File to be decrypted must end with \".rrcrypt\"");
  
  // Fetch the original filename (before encryption):
  string strOrigFilename = substr(strfile_name, 0, strfile_name.size() - 8);
  
  // Create a temporary directory to work under:
  temp_dir work_dir("rrcrypt_work");

  // Extract contents of the .rrcrypt file:
  string strprevdir=getcwd();

  chdir(((string)work_dir).c_str());  
  int intret = system(((string)"tar -xf \"" + strFile + "\"").c_str());
  chdir(strprevdir.c_str());
  if (intret != 0) rr_throw("There was an error with tar!");

  // Fetch and decrypt the password:
  if(!FileExists((string)work_dir + "control")) rr_throw("Could not find control file!");

  // Open the file
  ifstream TextFile(((string)work_dir + "control").c_str());
  if (!TextFile) rr_throw("An error opening " + strFile + "!");

  // Fetch the line with the password on it:
  string strPasswordLine;  
  getline(TextFile, strPasswordLine);
  TextFile.close();

  // Extract the parts of the line:
  string_splitter password_split(strPasswordLine, " ");

  // Line starts with "Encrypted-Password:"
  if ((string)password_split != "Encrypted-Password:") rr_throw("Invalid control file!");

  // Now fetch & decrypt the password:
  string strPassword=password_split;

  string_encryption str(strPassword);
  str.decrypt(get_rr_encrypt_key(), 2);

  // Now check for the file to be decrypted:
  string strExtractFile=substr(strfile_name, 0, strfile_name.size() - 8);

  // File exists?
  if (!FileExists((string)work_dir + "file.cpt")) rr_throw((string)"File not found: " + (string)work_dir + strExtractFile);

  // Export the password into the environment:
  setenv("RRCRYPT", strPassword.c_str(), 1);

  // Run ccrypt:
  int intresult = system(((string)"/usr/bin/ccrypt -d -E RRCRYPT " + (string)work_dir + "file.cpt").c_str());

  // Reset the password variable in the environment:
  unsetenv("RRCRYPT");
  
  // Check the output:
  if (intresult != 0) rr_throw("ccrypt had an error!");

  // Move the decrypted file to the directory the encrypted file was in,
  // and restore the original filename):
  mv((string)work_dir + "file", strfile_dir + strOrigFilename);

  // Remove the encrypted file:  
  rm(strFile);
}

#endif // END: #ifdef linux
