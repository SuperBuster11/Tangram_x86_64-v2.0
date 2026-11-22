#include "audio_linux.hpp"

#include <dlfcn.h>

namespace tg {
namespace {

// The few declarations needed from <pulse/simple.h> and <alsa/asoundlib.h> (stable C ABIs).
struct PaSampleSpec { int format; uint32_t rate; uint8_t channels; };
struct PaBufferAttr { uint32_t maxlength, tlength, prebuf, minreq, fragsize; };
constexpr int kPaStreamPlayback = 1, kPaSampleS16LE = 3;
using PaNew = void* (*)(const char*, const char*, int, const char*, const char*, const PaSampleSpec*, const void*, const PaBufferAttr*, int*);
using PaWrite = int (*)(void*, const void*, size_t, int*);
using PaDrain = int (*)(void*, int*);
using PaFree = void (*)(void*);

constexpr int kSndPcmStreamPlayback = 0, kSndPcmFormatS16LE = 2, kSndPcmAccessRwInterleaved = 3;
using SndOpen = int (*)(void**, const char*, int, int);
using SndSetParams = int (*)(void*, int, int, unsigned, unsigned, int, unsigned);
using SndWritei = long (*)(void*, const void*, unsigned long);
using SndRecover = int (*)(void*, int, int);
using SndClose = int (*)(void*);
using SndErrorHandler = void (*)(const char*, int, const char*, int, const char*, ...);
using SndSetErrorHandler = int (*)(SndErrorHandler);
void quietAlsa(const char*, int, const char*, int, const char*, ...) {}   // ALSA prints its errors to stderr by default

template <class F> F sym(void* lib, const char* name) { return reinterpret_cast<F>(dlsym(lib, name)); }

}  // namespace

bool LinuxAudio::open(std::string& backend) {
    close();
    if ((lib_ = dlopen("libpulse-simple.so.0", RTLD_NOW))) {   // PulseAudio / PipeWire
        auto paNew = sym<PaNew>(lib_, "pa_simple_new");
        if (paNew && sym<PaWrite>(lib_, "pa_simple_write")) {
            PaSampleSpec ss{kPaSampleS16LE, 44100, 2};
            const uint32_t bytes = 44100 * 4 * 60 / 1000;   // ~60 ms in the server's buffer: short delay, no gaps
            PaBufferAttr attr{uint32_t(-1), bytes, uint32_t(-1), uint32_t(-1), uint32_t(-1)};
            int err = 0;
            handle_ = paNew(nullptr, "Tangram", kPaStreamPlayback, nullptr, "music", &ss, nullptr, &attr, &err);
            if (handle_) { pulse_ = true; backend = "PulseAudio"; return true; }
        }
        dlclose(lib_); lib_ = nullptr;
    }
    if ((lib_ = dlopen("libasound.so.2", RTLD_NOW))) {   // ALSA
        if (auto h = sym<SndSetErrorHandler>(lib_, "snd_lib_error_set_handler")) h(quietAlsa);
        auto sndOpen = sym<SndOpen>(lib_, "snd_pcm_open");
        auto sndSet = sym<SndSetParams>(lib_, "snd_pcm_set_params");
        if (sndOpen && sndSet && sym<SndWritei>(lib_, "snd_pcm_writei")) {
            void* pcm = nullptr;
            if (sndOpen(&pcm, "default", kSndPcmStreamPlayback, 0) == 0) {
                if (sndSet(pcm, kSndPcmFormatS16LE, kSndPcmAccessRwInterleaved, 2, 44100, 1, 80000) == 0) {   // 80 ms latency, resampling allowed
                    handle_ = pcm; pulse_ = false; backend = "ALSA"; return true;
                }
                if (auto c = sym<SndClose>(lib_, "snd_pcm_close")) c(pcm);
            }
        }
        dlclose(lib_); lib_ = nullptr;
    }
    backend = "no sound (neither libpulse-simple.so.0 nor libasound.so.2 could open a playback device)";
    return false;
}

bool LinuxAudio::write(const int16_t* pcm, int frames) {
    if (!handle_) return false;
    if (pulse_) { int err = 0; return sym<PaWrite>(lib_, "pa_simple_write")(handle_, pcm, size_t(frames) * 4, &err) >= 0; }
    auto writei = sym<SndWritei>(lib_, "snd_pcm_writei");
    while (frames > 0) {
        long n = writei(handle_, pcm, (unsigned long)frames);
        if (n < 0) {   // underrun or suspend: recover and go on
            auto rec = sym<SndRecover>(lib_, "snd_pcm_recover");
            if (!rec || rec(handle_, int(n), 1) < 0) return false;
            continue;
        }
        pcm += 2 * n; frames -= int(n);
    }
    return true;
}

void LinuxAudio::close() {
    if (handle_) {
        if (pulse_) { if (auto f = sym<PaFree>(lib_, "pa_simple_free")) f(handle_); }
        else if (auto c = sym<SndClose>(lib_, "snd_pcm_close")) c(handle_);
        handle_ = nullptr;
    }
    if (lib_) { dlclose(lib_); lib_ = nullptr; }
}

}  // namespace tg
