/***************************************************************************
 *   String Hash Set
 ***************************************************************************/

#include "string_hash_set.h"
 
bool key_in_string_hash_set(string_hash_set & Set, const string & str) {
  // Return true if the key is in the string hash set, otherwise false
  string_hash_set::const_iterator IT;
  IT = Set.find(str);
  return IT != Set.end();
};
