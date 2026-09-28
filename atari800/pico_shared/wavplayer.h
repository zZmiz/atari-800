#pragma once
// WAV artwork previews are not offered by the Atari browser. Audio test images
// run through the emulator's real POKEY path, not through menu playback.
#include <stdint.h>
namespace wavplayer {
inline void pump(uint32_t) {}
inline uint32_t sample_rate() { return 0; }
inline bool use_file(const char *) { return false; }
inline bool isPlaying() { return false; }
inline void reset() {}
inline void pause() {}
inline void resume() {}
}
