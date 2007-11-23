/// @file
/// Basic maths-related functions

#include <string>

using namespace std;

// Straight from the glib macros:

#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#undef ABS
#define ABS(a)     (((a) < 0) ? -(a) : (a))

#undef CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

/// A generic base-conversion function for string representations of numbers:
string convert_base(const string & strvalue, const int intold_base, const int intnew_base);

/// Calculate permutations - mathematical function "x!"
int calc_permutations(int x);

