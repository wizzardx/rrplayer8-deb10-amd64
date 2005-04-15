#include "testing.h"

// Misc base conversion:
// - Base 36: (0-9 and A-Z)
unsigned long base36_to_dec(const string & strbase36) {
  undefined_throw;
}

string dec_to_base36(const unsigned long lngdec_val, const int intplaces) {
  undefined_throw;
}

// calculate permutations - mathematical function "x!"
int calc_permutations(int x) {
testing_throw;
  // Return the number of different permutations possible with a given number of elements.
  // eg - 3 elements can be sorted in 6 different orders.
  long lngresult = x;
  while (x > 2) {  // eg if x is 4 then multiply 4 by 3, then 2.
    --x;
    lngresult *=x;
  }
  return lngresult;              // return the result...
}
