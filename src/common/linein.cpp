#include "linein.h"
#include "my_string.h"
#include "string_splitter.h"
#include "exception.h"
#include <stdio.h>

int linein_getvol() {
  // Use an "aumix -q" call combined with a "grep" to fetch the linein level
  FILE *fp;
  char line[130];   /* line of data from unix command*/
  int intreturn_vol = -1; // Value read in from aumix

  string strcmd = "aumix -q | grep \"line \"";
  fp = popen(strcmd.c_str(), "r");   /* Issue the command. */

  /* Read a line                        */
  if (fgets(line, sizeof line, fp))
  {
    // Decode the line
    string strline = substr(line, 5);
    string_splitter split(strline, ", ");
    if (split.size() < 2) my_throw("Bad output from aumix!");

    string strleft  = split;
    string strright = split;

    // Check that the extracted values are valid
    if (isint(strleft) && isint(strright)) {
      // Values are read in from aumix
      intreturn_vol = (strtoi(strleft) + strtoi(strright)) / 2;
    }
    else {
      // Values extracted from aumix are not numeric!
      my_throw("Bad linein values read from aumix!");
    }
  }
  pclose(fp);
  return intreturn_vol;
}

void linein_setvol(const int intvol) {
  int intprev_vol=linein_getvol();
  if (intprev_vol == intvol) return;
  if (system(string(string("/usr/bin/aumix -l ") + itostr(intvol)).c_str())!=0) my_throw("There was a problem running aumix!");
  log_message(string("LineIn volume set to ") + itostr(intvol) + "%");
}
