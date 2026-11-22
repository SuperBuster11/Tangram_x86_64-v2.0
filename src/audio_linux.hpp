// Sound output for the Linux window layer without any build-time dependency: PulseAudio (also served by PipeWire) through
// libpulse-simple, otherwise ALSA through libasound; both are loaded at run time with dlopen. Without either the game stays silent.
#pragma once
#include <cstdint>
#include <string>

namespace tg {

class LinuxAudio {
public:
    ~LinuxAudio() { close(); }
    // Opens 44100 Hz, 16-bit stereo playback; `backend` says which library is used ("PulseAudio" / "ALSA"), or why none is.
    bool open(std::string& backend);
    // Plays interleaved stereo frames; blocks until the device has room (so the caller's thread is paced by the sound card).
    bool write(const int16_t* pcm, int frames);
    void close();

private:
    void* lib_ = nullptr;
    void* handle_ = nullptr;   // pa_simple* or snd_pcm_t*
    bool pulse_ = false;
};

}  // namespace tg
