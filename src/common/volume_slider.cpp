
#include "volume_slider.h"
#include "testing.h"
#include <sys/time.h>
#include "linein.h"
#include "my_time.h"
#include "my_string.h"

volume_slider::volume_slider(const SOUND_TYPE sound_type_arg, xmms_controller * xmms_arg, const int intlengthms_arg, const int intvol1, const int intvol2){
  // Constructor. Args: [sound_type] = the type of sound to do fading on. [xmms] = a pointer to an xmms_controller object.
  // Leave NULL if sound_type is not XMMS. [intlengthms] = the length of the fade in milliseconds. If [intvol2] is left
  // as the default -1, then [intvol1] = means fade from current volume to [intvol1]. If [intvol2] is set, then fade from
  // [intvol1] to [intvol2]

  // Check args:
  // * sound_type, xmms_controller. Also fetch the current volume.
  switch(sound_type_arg) {
    case ST_XMMS: {
      if (xmms_arg == NULL) my_throw("sound_type is ST_XMMS, but xmms arg is NULL!");
      intcurrent_vol = xmms_arg->getvol();
    } break;
    case ST_LINEIN: {
      if (xmms_arg != NULL) my_throw("sound_type is ST_LINEIN, but xmms arg is set!");
      intcurrent_vol = linein_getvol();
    } break;
    default: my_throw("Bad sound_type arg passed to this function!");
  }
  // * intlengthms:
  if (intlengthms_arg <= 0 || intlengthms_arg > 60*1000) my_throw("Bad intlengthms argument!");
  // * intvol1
  if (intvol1 < 0 || intvol1 > 100) my_throw("Bad vol1 argument!");
  // * intvol2
  if ((intvol2 != -1) && (intvol2 < 0 || intvol2 > 100)) my_throw("Bad vol2 argument!");

  // Fetch the sound type:
  sound_type = sound_type_arg;

  // Fetch the XMMS arg:
  xmms = xmms_arg;

  // Setup start time, and total length:
  gettimeofday(&start_timeval, NULL);
  intlength_ms = intlengthms_arg;

  // Setup intstart_vol and intend_vol:
  if (intvol2 == -1) {
    // User gave the end volume. Start volume is the current volume.
    intstart_vol  = intcurrent_vol;
    intend_vol = intvol1;
  }
  else {
    // User gave the end vol.
    intstart_vol = intvol1;
    intend_vol   = intvol2;
  }

  // Also setup a control variable in case the system time gets set backwards:
  intelapsed_ms = 0;

  // Log that we have initiated a volume slide:
  string strmessage = "Initiated a " + itostr(intlength_ms) + "ms ";

  switch(sound_type) {
    case ST_XMMS: strmessage += "XMMS (session " + itostr(xmms->get_session()) + ")"; break;
    case ST_LINEIN: strmessage += "LineIn"; break;
    default: LOGIC_ERROR;
  }

  strmessage += " volume slide from " + itostr(intstart_vol) + "% to " + itostr(intend_vol) + "%";
  log_message(strmessage);
}

bool volume_slider::run() {
  // run(): Catch up with any fading (checks the current time in ms). Returns immediately. Returns true if
  //        the fade is still busy, false when done.

  // If the current volume is already at the end volume, then quit:
  if (intcurrent_vol == intend_vol) return false; // false means control loops calling run() can stop.
  // Get the current time:
  timeval now;
  gettimeofday(&now, NULL);
  // Calculate the milliseconds elapsed since the start time:
  int intnew_elapsed_ms = (now.tv_sec  - start_timeval.tv_sec)  * 1000 +
                          (now.tv_usec - start_timeval.tv_usec) / 1000;
  // Is this less than our last elapsed ms (ie, system clock has been set back):
  if (intnew_elapsed_ms < intelapsed_ms) {
    // System clock was set back!
    testing_func_throw;
    log_warning("System time has moved backwards by " + itostr(intelapsed_ms - intnew_elapsed_ms) + "ms! Resynchronizing timing...");
    start_timeval.tv_usec -= (intelapsed_ms - intnew_elapsed_ms) * 1000;
    normalise_timeval(start_timeval);
    // And quit the func here also. We couldn't calculate how much time has really passed.
    // Let the control loop keep calling this func.
    return true;
  }

  // Now we have our new elapsed ms since the slide began:
  intelapsed_ms = intnew_elapsed_ms;

  // Get the volume to set the sound to:
  int intnew_vol = -1;         // This gets set next:
  bool blnslide_done = false; // Set to true if the slide has finished;
  // * Has the fade period finished?
  if (intelapsed_ms >= intlength_ms) {
    // Yes:
    intnew_vol = intend_vol;
    blnslide_done = true;
  }
  else {
    // Still before the end of the slide. Calculate the volume to use:
    intnew_vol = intstart_vol + ((intend_vol - intstart_vol) * intelapsed_ms)/intlength_ms;
  }

  // Now set the volume:
  if (intnew_vol != intcurrent_vol) {
    bool blnverbose = (intnew_vol == intend_vol); // Be verbose if this is our last volume update.
    switch(sound_type) {
      case ST_XMMS:   xmms->setvol(intnew_vol, blnverbose); break;
      case ST_LINEIN: linein_setvol(intnew_vol, blnverbose); break;
      default: LOGIC_ERROR;
    }
    // Our new volume is the new current volume:
    intcurrent_vol = intnew_vol;
  }

  // If our volume slide is finished then tell the controlling code that we're done:
  return !blnslide_done; // False means done, True means still busy.
}

void volume_slider::run_until_done() {
  // Do volume slide maintenance 5 times a second until the slide is done
  do usleep(200*1000); while (run());
}
