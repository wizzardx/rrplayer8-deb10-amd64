/// @file
/// A class for managing volume slides.

#ifndef VOLUME_SLIDER_H
#define VOLUME_SLIDER_H

#include "xmms_controller.h"

/**A class for managing a volume slide. Construct it with info about the slide, then run it regularly until it's done.
  *@author David Purdy
  */

class volume_slider {
public:
  /// A type passed to the constructor
  enum SOUND_TYPE {
    ST_XMMS,  ///< Sound type: XMMS
    ST_LINEIN ///< Sound type: LineIn
  };

  /// Constructor. Args: [sound_type] = the type of sound to do fading on. [xmms] = a pointer to an xmms_controller object.
  /// Leave NULL if sound_type is not XMMS. [intlengthms] = the length of the fade in milliseconds. If [intvol2] is left
  /// as the default -1, then [intvol1] = means fade from current volume to [intvol1]. If [intvol2] is set, then fade from
  /// [intvol1] to [intvol2]

  volume_slider(const SOUND_TYPE sound_type, xmms_controller * xmms, const int intlengthms, const int intvol1, const int intvol2 = -1);
  ~volume_slider() {}; // Destructor

  /// run(): Catch up with any fading (checks the current time in ms). Returns immediately. Returns true if
  ///        the fade is still busy, false when done.
  bool run();
  void run_until_done(); ///< Do the remaining vol slides, don't return until done.

  /// quick & easy checks. Let you use a loop like this while(fader) { usleep(200*1000); }
  operator void*() { return (void*)run(); }

private:
  volume_slider(); ///< Don't let anyone use the default argument-less constructor.

  // Attributes:
  SOUND_TYPE sound_type;
  xmms_controller * xmms; ///< If this is an XMMS volume slide, then this variable points to the session.

  timeval start_timeval; ///< Precise time when the constructor was called (including microseconds)
  int intlength_ms;      ///< Length of the fade in milliseconds.

  int intstart_vol;   ///< Starting volume
  int intend_vol;     ///< Ending volume
  int intcurrent_vol; ///< The current sound volume. We don't set the same volume twice!

  /// A control value, in case the clock gets set backwards during execution.
  /// How many milliseconds have elapsed since the slide began.
  int intelapsed_ms;
};

#endif
