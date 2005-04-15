  // Misc base conversion:
  // - Base 36: (0-9 and A-Z)
  unsigned long base36_to_dec(const string & strbase36);
  string dec_to_base36(const unsigned long lngdec_val, const int intplaces);

  // calculate permutations - mathematical function "x!"
  int calc_permutations(int x);
