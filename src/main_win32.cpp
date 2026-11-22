// Windows window layer (Win32 + GDI), same job as main_x11.cpp. NOT compiled or tested on Windows by the author.
// usage: Tangram_x64.exe [game.adf]   (default: the files in the "data" folder next to the exe)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <thread>

#include <string>
#include <vector>

#include "data_files.hpp"
#include "game.hpp"
#include "scale_filter.hpp"

static tg::Game g_game;
// The high-score table ("scoredisk") lives in %APPDATA%\Tangram\tangram.sco: a game installed in Program Files may not write
// next to its exe. Older versions kept tangram.sco in the current folder; that file is still read when there is none in %APPDATA%.
static std::filesystem::path scoreFile() {
    const wchar_t* appData = _wgetenv(L"APPDATA");
    return (appData && *appData ? std::filesystem::path(appData) / L"Tangram" : std::filesystem::path(L".")) / L"tangram.sco";
}
static void saveScoreFile(const tg::Bytes& d) {
    std::error_code ec; std::filesystem::create_directories(scoreFile().parent_path(), ec);
    std::ofstream out(scoreFile(), std::ios::binary); out.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size()));
}
static const char* const kTitle = sizeof(void*) == 8 ? "Tangram 64-bit" : "Tangram 32-bit";   // window title: Tangram_x64.exe / Tangram_x86.exe
static int g_scale = 4;        // window size = 320x200 times this (starts at 1280x800); F5 cycles 4 .. g_maxScale
static int g_maxScale = 4;
static tg::ScaleFilter g_filter = tg::ScaleFilter::Monitor1084;   // F8: 1084S -> CRT -> smoothed -> off
static int minScale() { return 4; }   // 1280x800 is the smallest window
static bool g_fullscreen = false;     // F11 / Alt+Enter: borderless window over the whole monitor, picture 16:10 as large as fits
static WINDOWPLACEMENT g_placement = {sizeof(WINDOWPLACEMENT)};
static RECT g_view = {0, 0, 1, 1};    // where the picture is drawn in the client area (black bars around it in fullscreen)
static std::vector<uint32_t> g_pix;
static std::recursive_mutex g_lock;            // the game is touched by the window thread and the audio thread
static std::atomic<bool> g_quit{false};

// Audio: 4 buffers of 40 ms through waveOut, refilled by a thread that renders the original music player.
static void audioThread() {
    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM; fmt.nChannels = 2; fmt.nSamplesPerSec = 44100; fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 4; fmt.nAvgBytesPerSec = 44100 * 4;
    HANDLE ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    HWAVEOUT out = nullptr;
    if (waveOutOpen(&out, WAVE_MAPPER, &fmt, reinterpret_cast<DWORD_PTR>(ev), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) return;
    constexpr int kBuffers = 4, kFrames = 1764;
    static short data[kBuffers][kFrames * 2];
    WAVEHDR hdr[kBuffers] = {};
    for (int i = 0; i < kBuffers; i++) {
        hdr[i].lpData = reinterpret_cast<LPSTR>(data[i]); hdr[i].dwBufferLength = sizeof data[i];
        waveOutPrepareHeader(out, &hdr[i], sizeof hdr[i]);
        hdr[i].dwFlags |= WHDR_DONE;   // treat as free
    }
    while (!g_quit) {
        for (int i = 0; i < kBuffers; i++) {
            if (!(hdr[i].dwFlags & WHDR_DONE)) continue;
            { std::lock_guard<std::recursive_mutex> l(g_lock); g_game.audio(data[i], kFrames); }
            hdr[i].dwFlags &= ~WHDR_DONE;
            waveOutWrite(out, &hdr[i], sizeof hdr[i]);
        }
        WaitForSingleObject(ev, 50);
    }
    waveOutReset(out);
    for (int i = 0; i < kBuffers; i++) waveOutUnprepareHeader(out, &hdr[i], sizeof hdr[i]);
    waveOutClose(out);
    CloseHandle(ev);
}

static RECT workArea() { RECT r; SystemParametersInfoA(SPI_GETWORKAREA, 0, &r, 0); return r; }

// Outer size of a window whose client area is 320*n x 200*n.
static SIZE outerSize(int n) {
    RECT r = {0, 0, 320 * n, 200 * n};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    return SIZE{r.right - r.left, r.bottom - r.top};
}

static void computeMaxScale() {   // the largest size that still fits the work area of the screen
    RECT wa = workArea(); g_maxScale = 4;   // never below 1280x800
    for (int n = 4;; n++) {
        SIZE s = outerSize(n);
        if (s.cx > wa.right - wa.left || s.cy > wa.bottom - wa.top) break;
        g_maxScale = n;
    }
}

static void setScale(HWND h, int n) {   // resizes the window and centres it in the work area
    g_scale = n;
    SIZE s = outerSize(n); RECT wa = workArea();
    SetWindowPos(h, nullptr, wa.left + (wa.right - wa.left - s.cx) / 2, wa.top + (wa.bottom - wa.top - s.cy) / 2, s.cx, s.cy, SWP_NOZORDER | SWP_NOACTIVATE);
}

// The largest 16:10 rectangle that fits the client area, centred (the whole client area for the F5 window sizes).
static RECT viewRect(int cw, int ch) {
    int w = cw, h = cw * 200 / 320;
    if (h > ch) { h = ch; w = ch * 320 / 200; }
    const int x = (cw - w) / 2, y = (ch - h) / 2;
    return RECT{x, y, x + std::max(1, w), y + std::max(1, h)};
}

static void present(HDC dc, int cw, int ch) {
    g_view = viewRect(cw, ch);
    const int w = g_view.right - g_view.left, h = g_view.bottom - g_view.top;
    g_pix.resize(size_t(w) * h);
    g_game.scaleFrame(g_pix.data(), w, h, g_filter);   // filtered directly at the final size (no stretching afterwards)
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, g_view.left, g_view.top, DWORD(w), DWORD(h), 0, 0, 0, UINT(h), g_pix.data(), &bi, DIB_RGB_COLORS);
    // black bars
    if (g_view.top > 0) PatBlt(dc, 0, 0, cw, g_view.top, BLACKNESS);
    if (g_view.bottom < ch) PatBlt(dc, 0, g_view.bottom, cw, ch - g_view.bottom, BLACKNESS);
    if (g_view.left > 0) PatBlt(dc, 0, g_view.top, g_view.left, h, BLACKNESS);
    if (g_view.right < cw) PatBlt(dc, g_view.right, g_view.top, cw - g_view.right, h, BLACKNESS);
}

static void toggleFullscreen(HWND h) {
    if (!g_fullscreen) {
        MONITORINFO mi = {sizeof mi};
        if (!GetWindowPlacement(h, &g_placement) || !GetMonitorInfoA(MonitorFromWindow(h, MONITOR_DEFAULTTOPRIMARY), &mi)) return;
        SetWindowLongA(h, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(h, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        g_fullscreen = true;
    } else {
        SetWindowLongA(h, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPlacement(h, &g_placement);
        SetWindowPos(h, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        g_fullscreen = false;
    }
    InvalidateRect(h, nullptr, FALSE);
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    RECT rc; GetClientRect(h, &rc);
    int cw = rc.right ? rc.right : 1, ch = rc.bottom ? rc.bottom : 1;
    // window position -> game pixel, through the picture rectangle (outside it: clamped to the edge)
    auto mx = [&] { g_view = viewRect(cw, ch); return std::max(0, std::min(319, (int(short(LOWORD(l))) - int(g_view.left)) * 320 / std::max(1, int(g_view.right - g_view.left)))); };
    auto my = [&] { g_view = viewRect(cw, ch); return std::max(0, std::min(199, (int(short(HIWORD(l))) - int(g_view.top)) * 200 / std::max(1, int(g_view.bottom - g_view.top)))); };
    tg::Event e;
    switch (m) {
    case WM_MOUSEMOVE: e.type = tg::Event::Move; e.x = mx(); e.y = my(); g_game.event(e); return 0;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_LBUTTONUP: case WM_RBUTTONUP:
        e.type = (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN) ? tg::Event::Down : tg::Event::Up;
        e.x = mx(); e.y = my(); e.button = (m == WM_RBUTTONDOWN || m == WM_RBUTTONUP);
        if (e.type == tg::Event::Down) SetCapture(h); else ReleaseCapture();   // the release is delivered even outside the window
        g_game.event(e); return 0;
    case WM_SYSKEYDOWN:   // F10 and Alt+key arrive as system keys in Windows
        if (w == VK_RETURN && (l & (1 << 29))) { if (!(l & (1 << 30))) toggleFullscreen(h); return 0; }   // Alt+Enter (not auto-repeat)
        if (w != VK_F10) break;
        e.type = tg::Event::Key; e.key = tg::kKeyF1 + 9; g_game.event(e); return 0;
    case WM_KEYDOWN:
        if (w == VK_F11) { if (!(l & (1 << 30))) toggleFullscreen(h); return 0; }   // fullscreen on / off
        if (w == VK_F5) { if (!g_fullscreen) setScale(h, g_scale >= g_maxScale ? minScale() : g_scale + 1); return 0; }   // next window size
        if (w == VK_F8) { g_filter = tg::nextFilter(g_filter); return 0; }   // filter: 1084S -> CRT -> smoothed -> off
        e.type = tg::Event::Key;
        if (w == VK_RETURN) e.key = tg::kKeyEnter; else if (w == VK_ESCAPE) e.key = tg::kKeyEsc; else if (w == VK_BACK) e.key = tg::kKeyBackspace;
        else if (w == VK_SPACE) e.key = ' ';
        else if (w >= VK_F1 && w <= VK_F10) e.key = tg::kKeyF1 + int(w - VK_F1);
        else if (w >= '0' && w <= '9') e.key = int(w); else if (w >= 'A' && w <= 'Z') e.key = int(w);
        else if (w >= VK_NUMPAD0 && w <= VK_NUMPAD9) e.key = int('0' + (w - VK_NUMPAD0)); else break;
        g_game.event(e); return 0;
    case WM_SYSCHAR: if (w == VK_RETURN) return 0; break;   // no beep after Alt+Enter
    case WM_CHAR:   // punctuation for the high-score name (letters, digits and space come from WM_KEYDOWN)
        if (w >= 33 && w <= 95 && !((w >= '0' && w <= '9') || (w >= 'A' && w <= 'Z'))) { e.type = tg::Event::Key; e.key = int(w); g_game.event(e); }
        return 0;
    case WM_TIMER: g_game.step();
        if (g_game.takeScoresDirty()) saveScoreFile(g_game.saveScores()); if (g_game.quitRequested()) { DestroyWindow(h); return 0; } InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); present(dc, cw, ch); EndPaint(h, &ps); return 0; }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR cmd, int show) {
    // The game data is read from the "data" folder next to the exe; a disk image path on the command line replaces it.
    std::string path = cmd ? cmd : "";
    if (path.size() > 1 && path.front() == '"' && path.back() == '"') path = path.substr(1, path.size() - 2);
    char exe[MAX_PATH] = {}; GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string exeDir = exe; exeDir = exeDir.substr(0, exeDir.find_last_of("\\/") + 1);
    const std::string dataDir = tg::findDataFolder(exeDir);
    tg::Adf adf; std::string err;
    const bool loaded = path.empty() ? tg::loadDataFolder(adf, dataDir, err) : adf.loadFile(path, err);
    if (!loaded || !g_game.init(adf, err)) { MessageBoxA(nullptr, err.c_str(), kTitle, MB_ICONERROR); return 1; }
    {   // high scores: %APPDATA%\Tangram\tangram.sco, or the old tangram.sco in the current folder
        std::ifstream in(scoreFile(), std::ios::binary);
        if (!in) in.open("tangram.sco", std::ios::binary);
        if (in) g_game.loadScores(tg::Bytes(std::istreambuf_iterator<char>(in), {}));
    }
    {   // music1.wav / music2.wav in the data folder (or next to the exe, or in the current folder) replace the emulated songs
        for (int song = 1; song <= 2; song++) {
            const std::string name = "music" + std::to_string(song) + ".wav";
            for (const std::string& path : {dataDir + name, exeDir + name, name}) {
                std::ifstream in(path, std::ios::binary);
                if (!in) continue;
                std::string werr;
                if (!g_game.loadMusicWav(song, std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {}), werr)) MessageBoxA(nullptr, (path + ": " + werr).c_str(), kTitle, MB_ICONWARNING);
                break;
            }
        }
    }
    WNDCLASSA wc = {}; wc.lpfnWndProc = proc; wc.hInstance = inst; wc.lpszClassName = "TangramWnd"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(1));   // icon from assets/tangram.rc
    RegisterClassA(&wc);
    computeMaxScale();
    g_scale = std::max(minScale(), std::min(g_scale, g_maxScale));
    SIZE sz = outerSize(g_scale); RECT wa = workArea();
    HWND h = CreateWindowA("TangramWnd", kTitle, WS_OVERLAPPEDWINDOW | WS_VISIBLE, wa.left + (wa.right - wa.left - sz.cx) / 2, wa.top + (wa.bottom - wa.top - sz.cy) / 2, sz.cx, sz.cy, nullptr, nullptr, inst, nullptr);
    {   // the smallest window (1280x800) does not fit this screen: start in fullscreen
        SIZE need = outerSize(minScale());
        if (need.cx > wa.right - wa.left || need.cy > wa.bottom - wa.top) toggleFullscreen(h);
    }
    ShowCursor(FALSE);
    timeBeginPeriod(1);
    SetTimer(h, 1, 20, nullptr);   // 50 Hz (the timer is coarse; good enough for a first version)
    std::thread audio(audioThread);
    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    g_quit = true;
    audio.join();
    return 0;
}
