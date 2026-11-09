// ChxrRender.hpp - ChxrCky 1.0.6 Rendering (Beta)
// Talks to eclipse.ffmpeg-api through its events.hpp (copy it from that mod's repo into
// src/render/ffmpeg_events.hpp). Hard caps: 1920x1080, 30 FPS.
//
// Flow: arm() -> level restart calls onRestart() -> each video frame you run
// ticksForNextFrame(tps) physics ticks, then captureFrame() (called from CCDirector::drawScene).
// Frames go to a bounded queue; a worker thread feeds FFmpeg so physics never waits on encoding
// except when the queue is full (back-pressure, keeps RAM flat on Android).
#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>
#include "ffmpeg_events.hpp"

namespace chxr {

struct RenderCfg {
    std::string codec = "libx264";   // empty = auto pick
    std::string filters;             // goes to RenderSettings::m_colorspaceFilters (ffmpeg -vf style)
    int width = 1920, height = 1080; // clamped to 1920x1080
    int fps = 30;                    // clamped to 1..30
    int bitrateMbps = 15;            // clamped to 1..60
    std::string name = "render";
};

class Renderer {
public:
    enum class Phase { Idle, Armed, Rendering };
    static Renderer& get() { static Renderer r; return r; }

    RenderCfg cfg;
    Phase phase() const { return m_phase; }
    bool rendering() const { return m_phase == Phase::Rendering; }
    uint64_t frames() const { return m_frames; }
    int fps() const { return m_fps; }

    static bool available() { return geode::Loader::get()->isModLoaded("eclipse.ffmpeg-api"); }
    static std::vector<std::string> codecs() {
        return available() ? ffmpeg::events::Recorder::getAvailableCodecs() : std::vector<std::string>{};
    }

    void arm() {
        if (!available()) { say("Install the FFmpeg API mod (eclipse.ffmpeg-api) first.", true); return; }
        m_phase = Phase::Armed;
    }
    void cancel() { if (rendering()) finish("stopped"); else m_phase = Phase::Idle; }
    void onRestart() {
        if (m_phase == Phase::Armed) begin();
        else if (rendering() && m_frames > 1) finish("level restarted");
    }
    void notePlayerEnded(int tailSeconds) { if (rendering() && m_stopIn < 0) m_stopIn = tailSeconds * m_fps; }

    // Physics ticks to run before the next video frame. Carries the remainder so 240 TPS @ 30 FPS = 8,
    // and 360 TPS @ 30 FPS = 12, etc., with no drift.
    int ticksForNextFrame(int tps) {
        m_debt += double(tps) / double(m_fps);
        int n = int(m_debt);
        m_debt -= n;
        return std::max(1, n);
    }

    // Call after the ticks for one video frame ran (from CCDirector::drawScene post-hook).
    void captureFrame() {
        if (!rendering() || !m_rt) return;
        { std::lock_guard g(m_mx); if (m_broken) { fail(m_error); return; } }
        auto* scene = cocos2d::CCDirector::get()->getRunningScene();
        if (!scene) return;

        auto buf = acquire();
        m_rt->beginWithClear(0.f, 0.f, 0.f, 1.f);
        scene->visit();
        glReadPixels(0, 0, m_w, m_h, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
        m_rt->end();
        push(std::move(buf));
        ++m_frames;

        if (m_stopIn >= 0 && --m_stopIn <= 0) finish("done");
        else if (m_frames > uint64_t(m_fps) * 3600) finish("one hour limit");
    }

private:
    static constexpr size_t kMaxQueued = 4, kMaxSpare = 2;
    Phase m_phase = Phase::Idle;
    std::unique_ptr<ffmpeg::events::Recorder> m_enc;
    geode::Ref<cocos2d::CCRenderTexture> m_rt;
    std::filesystem::path m_out;
    int m_w = 0, m_h = 0, m_fps = 30, m_stopIn = -1;
    uint64_t m_frames = 0;
    double m_debt = 0.0;

    std::thread m_th;
    std::mutex m_mx;
    std::condition_variable m_cv;
    std::deque<std::vector<uint8_t>> m_q;
    std::vector<std::vector<uint8_t>> m_spare;
    bool m_closing = false, m_broken = false;
    std::string m_error;

    static void say(std::string const& t, bool err = false) {
        geode::Notification::create(t, err ? geode::NotificationIcon::Error : geode::NotificationIcon::Info)->show();
    }

    std::vector<uint8_t> acquire() {
        std::vector<uint8_t> f;
        { std::lock_guard g(m_mx); if (!m_spare.empty()) { f = std::move(m_spare.back()); m_spare.pop_back(); } }
        f.resize(size_t(m_w) * m_h * 4);
        return f;
    }
    void push(std::vector<uint8_t> f) {
        std::unique_lock g(m_mx);
        m_cv.wait(g, [&] { return m_q.size() < kMaxQueued || m_broken; });
        if (m_broken) return;
        m_q.push_back(std::move(f));
        m_cv.notify_all();
    }
    void worker() {
        for (;;) {
            std::vector<uint8_t> f;
            {
                std::unique_lock g(m_mx);
                m_cv.wait(g, [&] { return m_closing || !m_q.empty(); });
                if (m_q.empty()) return;
                f = std::move(m_q.front());
                m_q.pop_front();
            }
            m_cv.notify_all();
            if (!m_broken) {
                auto r = m_enc->writeFrame(std::span<uint8_t const>(f.data(), f.size()));
                if (r.isErr()) {
                    std::lock_guard g(m_mx);
                    m_error = r.unwrapErr();
                    m_broken = true;
                    m_cv.notify_all();
                }
            }
            std::lock_guard g(m_mx);
            if (m_spare.size() < kMaxSpare) m_spare.push_back(std::move(f));
        }
    }
    void stopWorker() {
        { std::lock_guard g(m_mx); m_closing = true; }
        m_cv.notify_all();
        if (m_th.joinable()) m_th.join();
        m_q.clear(); m_spare.clear();
        m_closing = false; m_broken = false;
    }

    std::filesystem::path pickPath() {
        auto dir = geode::Mod::get()->getConfigDir() / "Renders";
        (void)geode::utils::file::createDirectoryAll(dir);
        std::filesystem::path base = dir.string();
        std::string n = cfg.name.empty() ? "render" : cfg.name;
        for (char& c : n) if (std::string("\\/:*?\"<>|").find(c) != std::string::npos) c = '_';
        auto p = base / (n + ".mp4");
        for (int i = 1; std::filesystem::exists(p) && i < 10000; ++i) p = base / (n + "_" + std::to_string(i) + ".mp4");
        return p;
    }

    void begin() {
        m_w = std::clamp(cfg.width, 320, 1920) & ~1;
        m_h = std::clamp(cfg.height, 180, 1080) & ~1;
        m_fps = std::clamp(cfg.fps, 1, 30);
        m_out = pickPath();

        auto avail = codecs();
        std::string codec = cfg.codec;
        if (!avail.empty() && !codec.empty() && std::find(avail.begin(), avail.end(), codec) == avail.end()) {
            m_phase = Phase::Idle;
            say("Codec '" + codec + "' isn't available in FFmpeg API", true);
            return;
        }

        ffmpeg::RenderSettings s;
        s.m_pixelFormat = ffmpeg::PixelFormat::RGBA;
        s.m_codec = codec;
        s.m_colorspaceFilters = cfg.filters;
        s.m_doVerticalFlip = true; // GL reads bottom-up
        s.m_bitrate = int64_t(std::clamp(cfg.bitrateMbps, 1, 60)) * 1'000'000;
        s.m_width = uint32_t(m_w); s.m_height = uint32_t(m_h);
        s.m_fps = uint16_t(m_fps);
        s.m_outputFile = m_out;

        m_enc = std::make_unique<ffmpeg::events::Recorder>();
        if (!m_enc->isValid()) { fail("FFmpeg API didn't answer (restart the game)"); return; }
        auto r = m_enc->init(s);
        if (!r) { fail(r.unwrapErr()); return; }

        m_rt = cocos2d::CCRenderTexture::create(m_w, m_h, cocos2d::kCCTexture2DPixelFormat_RGBA8888);
        if (!m_rt) { fail("couldn't create render texture"); return; }

        m_frames = 0; m_debt = 0.0; m_stopIn = -1;
        m_th = std::thread([this] { worker(); });
        m_phase = Phase::Rendering;
        say(fmt::format("Rendering {}x{} @ {} FPS", m_w, m_h, m_fps));
    }

    void release() {
        stopWorker();
        if (m_enc) { m_enc->stop(); m_enc.reset(); }
        m_rt = nullptr;
        m_phase = Phase::Idle;
        m_frames = 0; m_stopIn = -1; m_debt = 0.0;
    }
    void fail(std::string const& why) { release(); say("Render failed: " + why, true); }
    void finish(char const* why) {
        auto name = m_out.filename().string();
        auto frames = m_frames; auto fps = m_fps;
        release();
        say(fmt::format("Saved {} ({:.1f}s, {})", name, double(frames) / fps, why));
    }
};

} // namespace chxr
