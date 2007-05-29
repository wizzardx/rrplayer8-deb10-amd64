#include "char_array_maths.h"
#include "my_string.h"
#include "exception.h"

// A generic base-conversion function:
string convert_base(const string & _strvalue, const int intold_base, const int intnew_base) {
  // Remove spaces from the value & convert to upper case:
  string strvalue = ucase(trim(_strvalue));

  // Check the arguments:
  if (strvalue == "") my_throw("Value to convert is empty!");
  if (intold_base < 2 || intold_base > 36) my_throw("Invalid old base: " + itostr(intold_base));
  if (intnew_base < 2 || intnew_base > 36) my_throw("Invalid new base: " + itostr(intnew_base));

  // We use functionality from char_array_maths. So setup a buffer:
  unsigned char buffer[strvalue.length()];
  char_array_maths cham(buffer, sizeof(buffer));
  cham.reset();

  // Import the string in the old base:
  cham.include_string(strvalue, get_base_allowed_chars(intold_base));

  // Now extract & return the value in the new base:
  return cham.extract_string(get_base_allowed_chars(intnew_base));
}

// calculate permutations - mathematical function "x!"
int calc_permutations(int x) {
  // Return the number of different permutations possible with a given number of elements.
  // eg - 3 elements can be sorted in 6 different orders.
  long lngresult = x;
  while (x > 2) {  // eg if x is 4 then multiply 4 by 3, then 2.
    --x;
    lngresult *=x;
  }
  return lngresult;              // return the result...
}
