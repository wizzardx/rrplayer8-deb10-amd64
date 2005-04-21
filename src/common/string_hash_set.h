#include <ext/hash_set>
#include <string>

using namespace std;
using namespace __gnu_cxx;

// String Hash Set (used for quick storing (and searching for) unqiue strings in memory)
struct string_hash {
  size_t operator()(string s1) const {
    hash<const char *> H;
    return H(s1.c_str());
  }
};

struct eqstr {
  bool operator()(string s1, string s2) const {
    return s1==s2;
  }
};

typedef hash_set<string, string_hash, eqstr> string_hash_set;
bool key_in_string_hash_set(string_hash_set & Set, const string & str);
