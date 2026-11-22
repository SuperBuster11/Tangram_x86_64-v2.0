// Linux window layer (Xlib): opens a scaled window, feeds Events to tg::Game at 50 Hz.
// usage: tangram_x11 [game.adf] [--scale N] [--frames N [--shot out.png] [--wav out.wav]] [--audio-stdout] [--mute]
//   F5 cycles the window size (1280x800, 1600x1000, ... up to the screen); F8 cycles the filter (1084S -> CRT -> smoothed -> off);
//   F11 / Alt+Enter: fullscreen (16:10 picture as large as the monitor allows, black bars around it)
//   --frames: exit after N frames, optionally saving a PNG and the audio as a WAV file
//   --audio-stdout: write the audio as raw 16-bit stereo 44100 Hz to stdout (play with: ... | aplay -f S16_LE -r 44100 -c 2)
//   --mute: no sound. Otherwise the music plays through PulseAudio / PipeWire or ALSA (loaded at run time; none = silent)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <filesystem>
#include <fstream>
#include <iterator>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <vector>

#include "audio_linux.hpp"
#include "data_files.hpp"
#include "game.hpp"
#include "icon_data.hpp"
#include "scale_filter.hpp"

int main(int argc, char** argv) {
    // The game data is compiled in; a disk image given as the first argument (not starting with "--") overrides it.
    const char* adfPath = argc >= 2 && std::strncmp(argv[1], "--", 2) ? argv[1] : nullptr;
    int scale = 4, maxFrames = -1; const char* shot = nullptr; const char* wavPath = nullptr; bool audioStdout = false, mute = false;
    for (int i = adfPath ? 2 : 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--scale") && i + 1 < argc) scale = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) maxFrames = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
        else if (!std::strcmp(argv[i], "--wav") && i + 1 < argc) wavPath = argv[++i];
        else if (!std::strcmp(argv[i], "--audio-stdout")) audioStdout = true;
        else if (!std::strcmp(argv[i], "--mute")) mute = true;
    }
    // the game data: the "data" folder next to the program (or in the current folder), or a disk image given on the command line
    char exe[4096] = {}; const ssize_t exeLen = readlink("/proc/self/exe", exe, sizeof exe - 1);
    std::string exeDir = exeLen > 0 ? std::string(exe, size_t(exeLen)) : std::string(); exeDir = exeDir.substr(0, exeDir.find_last_of('/') + 1);
    const std::string dataDir = tg::findDataFolder(exeDir);
    tg::Adf adf; std::string err;
    if (adfPath ? !adf.loadFile(adfPath, err) : !tg::loadDataFolder(adf, dataDir, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    tg::Game game;
    if (!game.init(adf, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    // the "scoredisk": high-score table in $XDG_DATA_HOME/tangram (default ~/.local/share/tangram); the old tangram.sco in the
    // current folder is still read when there is none there
    std::string scoreDir;
    if (const char* x = std::getenv("XDG_DATA_HOME"); x && *x) scoreDir = std::string(x) + "/tangram";
    else if (const char* home = std::getenv("HOME"); home && *home) scoreDir = std::string(home) + "/.local/share/tangram";
    else scoreDir = ".";
    const std::string scorePath = scoreDir + "/tangram.sco";
    {
        std::ifstream in(scorePath, std::ios::binary);
        if (!in) in.open("tangram.sco", std::ios::binary);
        if (in) game.loadScores(tg::Bytes(std::istreambuf_iterator<char>(in), {}));
    }
    {   // music1.wav / music2.wav in the data folder (or next to the program, or in the current folder) replace the emulated songs
        for (int song = 1; song <= 2; song++) {
            const std::string name = "music" + std::to_string(song) + ".wav";
            for (const std::string& path : {dataDir + name, exeDir + name, name}) {
                std::ifstream in(path, std::ios::binary);
                if (!in) continue;
                std::string werr;
                if (!game.loadMusicWav(song, std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {}), werr)) std::fprintf(stderr, "%s: %s\n", path.c_str(), werr.c_str());
                break;
            }
        }
    }

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) { std::fprintf(stderr, "cannot open X display\n"); return 1; }
    int scr = DefaultScreen(dpy);
    // F5 cycles the window size through 1280x800, 1600x1000, ... up to the largest multiple of 320x200 that fits the screen
    const int minScale = 4;   // 1280x800 is the smallest window
    int maxScale = minScale;
    while (320 * (maxScale + 1) <= DisplayWidth(dpy, scr) && 200 * (maxScale + 1) <= DisplayHeight(dpy, scr) - 80) maxScale++;
    scale = std::max(minScale, std::min(scale, maxScale));
    tg::ScaleFilter filter = tg::ScaleFilter::Monitor1084;   // F8: 1084S -> CRT -> smoothed -> off
    int W = 320 * scale, H = 200 * scale;   // window (client) size
    int VX = 0, VY = 0, VW = W, VH = H;     // the picture inside it (16:10, centred; black bars in fullscreen)
    bool fullscreen = false;
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), (DisplayWidth(dpy, scr) - W) / 2, (DisplayHeight(dpy, scr) - H) / 2, W, H, 0, 0, BlackPixel(dpy, scr));   // centred on the screen
    XStoreName(dpy, win, sizeof(void*) == 8 ? "Tangram 64-bit" : "Tangram 32-bit");   // same titles as the Windows builds
    {   // window icon (_NET_WM_ICON: width, height, then ARGB pixels as longs)
        std::vector<unsigned long> ic; ic.push_back(64); ic.push_back(64);
        for (uint32_t v : kIcon64) ic.push_back(v);
        XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_ICON", False), XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<unsigned char*>(ic.data()), int(ic.size()));
    }
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);
    Atom del = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &del, 1);
    XMapWindow(dpy, win);
    Visual* vis = DefaultVisual(dpy, scr);
    int depth = DefaultDepth(dpy, scr);
    if (depth < 24) { std::fprintf(stderr, "need a 24/32-bit display\n"); return 1; }
    std::vector<uint32_t> pix(size_t(W) * H);
    XImage* img = XCreateImage(dpy, vis, depth, ZPixmap, 0, reinterpret_cast<char*>(pix.data()), W, H, 32, 0);
    GC gc = XCreateGC(dpy, win, 0, nullptr);
    XSetForeground(dpy, gc, BlackPixel(dpy, scr));
    auto layout = [&](int cw, int ch) {   // new window size: largest 16:10 picture that fits, new image buffer
        W = std::max(1, cw); H = std::max(1, ch);
        VW = W; VH = W * 200 / 320;
        if (VH > H) { VH = H; VW = H * 320 / 200; }
        VW = std::max(1, VW); VH = std::max(1, VH); VX = (W - VW) / 2; VY = (H - VH) / 2;
        img->data = nullptr; XDestroyImage(img);
        pix.assign(size_t(VW) * VH, 0);
        img = XCreateImage(dpy, vis, depth, ZPixmap, 0, reinterpret_cast<char*>(pix.data()), VW, VH, 32, 0);
        XFillRectangle(dpy, win, gc, 0, 0, unsigned(W), unsigned(H));
    };
    auto toggleFullscreen = [&] {   // EWMH: ask the window manager for _NET_WM_STATE_FULLSCREEN
        fullscreen = !fullscreen;
        XEvent x = {}; x.type = ClientMessage; x.xclient.window = win; x.xclient.format = 32;
        x.xclient.message_type = XInternAtom(dpy, "_NET_WM_STATE", False);
        x.xclient.data.l[0] = fullscreen ? 1 : 0; x.xclient.data.l[1] = long(XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False)); x.xclient.data.l[3] = 1;
        XSendEvent(dpy, RootWindow(dpy, scr), False, SubstructureRedirectMask | SubstructureNotifyMask, &x);
    };
    auto gameX = [&](int x) { return std::max(0, std::min(319, (x - VX) * 320 / VW)); };
    auto gameY = [&](int y) { return std::max(0, std::min(199, (y - VY) * 200 / VH)); };

    std::vector<int16_t> pcm, chunk(2 * 882);   // 882 stereo frames = 20 ms at 44100 Hz
    // sound card: a thread renders the music in 512-frame pieces and blocks in the device's write (the game is shared: one lock)
    std::mutex gameLock;
    std::atomic<bool> audioRun{false};
    tg::LinuxAudio sound;
    std::thread audioThread;
    if (!mute && !wavPath && !audioStdout && maxFrames < 0) {
        std::string backend;
        if (sound.open(backend)) {
            audioRun = true;
            audioThread = std::thread([&] {
                std::vector<int16_t> buf(2 * 512);
                const auto t0 = std::chrono::steady_clock::now(); long long written = 0;
                while (audioRun) {
                    { std::lock_guard<std::mutex> l(gameLock); game.audio(buf.data(), 512); }
                    if (!sound.write(buf.data(), 512)) break;
                    written += 512;
                    // a device that does not block (e.g. a null sink) must not run the music ahead: stay within 150 ms of real time
                    const auto due = t0 + std::chrono::microseconds(written * 1000000 / 44100 - 150000);
                    if (std::chrono::steady_clock::now() < due) std::this_thread::sleep_until(due);
                }
            });
        }
        std::fprintf(stderr, "sound: %s\n", backend.c_str());
    }
    auto next = std::chrono::steady_clock::now();
    int frames = 0;
    bool running = true;
    while (running) {
        std::unique_lock<std::mutex> frameLock(gameLock);   // released before the wait at the end of the frame
        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            tg::Event e;
            if (ev.type == ClientMessage && Atom(ev.xclient.data.l[0]) == del) running = false;
            else if (ev.type == ConfigureNotify && (ev.xconfigure.width != W || ev.xconfigure.height != H)) layout(ev.xconfigure.width, ev.xconfigure.height);
            else if (ev.type == Expose) XFillRectangle(dpy, win, gc, 0, 0, unsigned(W), unsigned(H));
            else if (ev.type == KeyPress && (XLookupKeysym(&ev.xkey, 0) == XK_F11 || (XLookupKeysym(&ev.xkey, 0) == XK_Return && (ev.xkey.state & Mod1Mask)))) toggleFullscreen();
            else if (ev.type == KeyPress && XLookupKeysym(&ev.xkey, 0) == XK_F5) {   // next size, back to the smallest after the largest
                if (fullscreen) continue;
                scale = scale >= maxScale ? minScale : scale + 1;
                layout(320 * scale, 200 * scale);
                XResizeWindow(dpy, win, unsigned(W), unsigned(H));
                XMoveWindow(dpy, win, std::max(0, (DisplayWidth(dpy, scr) - W) / 2), std::max(0, (DisplayHeight(dpy, scr) - H) / 2));
            }
            else if (ev.type == KeyPress && XLookupKeysym(&ev.xkey, 0) == XK_F8) filter = tg::nextFilter(filter);
            else if (ev.type == MotionNotify) { e.type = tg::Event::Move; e.x = gameX(ev.xmotion.x); e.y = gameY(ev.xmotion.y); game.event(e); }
            else if (ev.type == ButtonPress || ev.type == ButtonRelease) {
                if (ev.xbutton.button != 1 && ev.xbutton.button != 3) continue;
                e.type = ev.type == ButtonPress ? tg::Event::Down : tg::Event::Up;
                e.x = gameX(ev.xbutton.x); e.y = gameY(ev.xbutton.y); e.button = ev.xbutton.button == 3;
                game.event(e);
            } else if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                e.type = tg::Event::Key;
                if (ks == XK_Return || ks == XK_KP_Enter) e.key = tg::kKeyEnter;
                else if (ks == XK_Escape) e.key = tg::kKeyEsc;
                else if (ks == XK_BackSpace) e.key = tg::kKeyBackspace;
                else if (ks == XK_space) e.key = ' ';
                else if (ks >= XK_F1 && ks <= XK_F10) e.key = tg::kKeyF1 + int(ks - XK_F1);
                else if (ks >= XK_0 && ks <= XK_9) e.key = int(ks);
                else if (ks >= XK_a && ks <= XK_z) e.key = int(ks - XK_a + 'A');
                else if (ks >= XK_A && ks <= XK_Z) e.key = int(ks - XK_A + 'A');
                else {   // punctuation for the high-score name (the keyboard's own character)
                    char buf[8]; int n = XLookupString(&ev.xkey, buf, sizeof buf, nullptr, nullptr);
                    if (n != 1 || buf[0] < 33 || buf[0] > 95) continue;
                    e.key = buf[0];
                }
                game.event(e);
            }
        }
        game.step();
        if (game.takeScoresDirty()) {
            tg::Bytes d = game.saveScores();
            std::error_code ec; std::filesystem::create_directories(scoreDir, ec);
            std::ofstream out(scorePath, std::ios::binary); out.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size()));
        }
        if (game.quitRequested()) break;
        if (wavPath || audioStdout) {
            game.audio(chunk.data(), 882);
            if (wavPath) pcm.insert(pcm.end(), chunk.begin(), chunk.end());
            if (audioStdout) std::fwrite(chunk.data(), 2, chunk.size(), stdout);
        }
        const tg::Image& f = game.frame();
        game.scaleFrame(pix.data(), VW, VH, filter);
        XPutImage(dpy, win, gc, img, 0, 0, VX, VY, unsigned(VW), unsigned(VH));
        XFlush(dpy);
        if (maxFrames >= 0 && ++frames >= maxFrames) {
            if (shot) tg::writePng(shot, f, 1);
            if (wavPath) {
                std::FILE* w = std::fopen(wavPath, "wb");
                if (w) {
                    auto le32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) std::fputc(int((v >> (8 * i)) & 255), w); };
                    auto le16 = [&](uint16_t v) { std::fputc(v & 255, w); std::fputc(v >> 8, w); };
                    std::fwrite("RIFF", 1, 4, w); le32(uint32_t(36 + pcm.size() * 2)); std::fwrite("WAVEfmt ", 1, 8, w); le32(16); le16(1); le16(2);
                    le32(44100); le32(44100 * 4); le16(4); le16(16); std::fwrite("data", 1, 4, w); le32(uint32_t(pcm.size() * 2));
                    std::fwrite(pcm.data(), 2, pcm.size(), w); std::fclose(w);
                }
            }
            break;
        }
        frameLock.unlock();
        next += std::chrono::milliseconds(20);   // 50 Hz
        std::this_thread::sleep_until(next);
    }
    audioRun = false;
    if (audioThread.joinable()) audioThread.join();
    sound.close();
    img->data = nullptr;   // buffer is owned by `pix`
    XDestroyImage(img);
    XCloseDisplay(dpy);
    return 0;
}
