#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include "../compat_v6.hpp"
#include <mutex>
#include <cctype>
#include <initializer_list>
#include <thread>
#include <vector>

#if defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

#if defined(__ANDROID__) && __has_include(<media/NdkMediaCodec.h>)
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaMuxer.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#define CKY_NATIVE_ENC 1
#else
#define CKY_NATIVE_ENC 0
#endif

namespace chxr {

struct RenderCfg {
    std::string codec = "H.264";
    std::string filters;
    int width = 1920, height = 1080;
    int fps = 30;
    int bitrateMbps = 15;
    std::string name = "render";
};

struct DeviceGuess {
    std::string chipset;
    std::string raw;
    int resIndex = 1;
    bool known = false;
};

inline std::string sysProp(char const* key) {
#if defined(__ANDROID__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    char buf[PROP_VALUE_MAX] = {};
    if (__system_property_get(key, buf) > 0) return std::string(buf);
#pragma clang diagnostic pop
#endif
    (void)key;
    return {};
}

inline DeviceGuess detectDevice() {
    DeviceGuess d;
    char const* keys[] = { "ro.soc.model", "ro.board.platform", "ro.hardware", "ro.hardware.chipname",
                           "ro.mediatek.platform", "ro.product.board" };
    std::vector<std::string> tokens;
    for (auto* k : keys) {
        auto v = sysProp(k);
        if (v.empty()) continue;
        if (d.chipset.empty()) d.chipset = v;
        d.raw += fmt::format("{}={} ", k, v);
        for (auto& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        tokens.push_back(v);
    }
    auto has = [&](char const* prefix) {
        std::string pre = prefix;
        for (auto const& t : tokens) if (t.compare(0, pre.size(), pre) == 0) return true;
        return false;
    };
    auto any = [&](std::initializer_list<char const*> list) {
        for (auto* k : list) if (has(k)) return true;
        return false;
    };

    if (any({ "msm8909", "msm8917", "msm8920", "mt6580", "mt6737", "mt6739", "mt6761", "mt6762",
              "sc7731", "sc9832", "sc9863" })) {
        d.resIndex = 2; d.known = true;
    } else if (any({ "msm8937", "msm8940", "msm8953", "mt6765", "mt6768", "mt6769", "mt6771", "bengal",
                     "holi", "trinket", "sdm660", "sdm636", "sdm632", "ums512" })) {
        d.resIndex = 1; d.known = true;
    } else if (any({ "kona", "lahaina", "taro", "kalama", "pineapple", "msmnile", "sdm845", "sdm710",
                     "msm8996", "msm8998", "lito", "atoll", "sm8", "sm7", "mt6785", "mt68", "mt69",
                     "s5e9", "gs101", "gs201", "zuma" })) {
        d.resIndex = 0; d.known = true;
    } else {
        d.resIndex = 1;
    }
    return d;
}

class RenderUi {
public:
    RenderCfg cfg;

    static constexpr int kResW[4] = { 1920, 1280, 854, 640 };
    static constexpr int kResH[4] = { 1080, 720, 480, 360 };
    static constexpr int kRates[9] = { 2, 4, 8, 12, 15, 20, 30, 40, 60 };

    int resIndex() const {
        for (int i = 0; i < 4; ++i) if (cfg.width == kResW[i] && cfg.height == kResH[i]) return i;
        return 0;
    }
    void setRes(int i) { i = std::clamp(i, 0, 3); cfg.width = kResW[i]; cfg.height = kResH[i]; }
    void cycleRes(int d) { setRes(resIndex() + d); save(); }
    void stepFps(int d) { cfg.fps = std::clamp(cfg.fps + d, 1, 30); save(); }
    void cycleBitrate(int d) {
        int idx = 0;
        for (int i = 0; i < 9; ++i) if (kRates[i] <= cfg.bitrateMbps) idx = i;
        cfg.bitrateMbps = kRates[std::clamp(idx + d, 0, 8)];
        save();
    }
    void cycleCodec(std::vector<std::string> const& list, int d) {
        if (list.empty()) return;
        int idx = 0;
        for (int i = 0; i < static_cast<int>(list.size()); ++i) if (list[i] == cfg.codec) idx = i;
        int n = static_cast<int>(list.size());
        cfg.codec = list[((idx + d) % n + n) % n];
        save();
    }

    std::string autoSetup() {
        auto d = detectDevice();
        setRes(d.resIndex);
        save();
        geode::log::info("[cky render] help: {} -> {}x{} ({})", d.raw.empty() ? "no chipset info" : d.raw,
            cfg.width, cfg.height, d.known ? "known chipset" : "unknown chipset, default");
        return fmt::format("{}: set to {}x{}{}", d.chipset.empty() ? "Unknown chipset" : d.chipset,
            cfg.width, cfg.height, d.known ? "" : " (guess)");
    }

    void load() {
        auto* m = geode::Mod::get();
        cfg.codec = m->getSavedValue<std::string>("rn-codec", cfg.codec);
        cfg.filters = m->getSavedValue<std::string>("rn-filters", cfg.filters);
        cfg.name = m->getSavedValue<std::string>("rn-name", cfg.name);
        setRes(m->getSavedValue<int>("rn-res", 0));
        cfg.fps = std::clamp(m->getSavedValue<int>("rn-fps", 30), 1, 30);
        cfg.bitrateMbps = std::clamp(m->getSavedValue<int>("rn-bitrate", 15), 1, 60);
    }
    void save() {
        auto* m = geode::Mod::get();
        m->setSavedValue<std::string>("rn-codec", cfg.codec);
        m->setSavedValue<std::string>("rn-filters", cfg.filters);
        m->setSavedValue<std::string>("rn-name", cfg.name);
        m->setSavedValue<int>("rn-res", resIndex());
        m->setSavedValue<int>("rn-fps", cfg.fps);
        m->setSavedValue<int>("rn-bitrate", cfg.bitrateMbps);
    }
};

#if CKY_NATIVE_ENC
class MediaEncoder {
public:
    ~MediaEncoder() { close(); }

    std::string open(std::string const& path, int w, int h, int fps, int bitrate, bool hevc) {
        m_w = w; m_h = h; m_fps = std::max(1, fps);
        m_ew = (w + 15) & ~15; m_eh = (h + 15) & ~15;
        m_fd = ::open(path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
        if (m_fd < 0) return "couldn't create the output file";

        char const* mime = hevc ? "video/hevc" : "video/avc";
        m_codec = AMediaCodec_createEncoderByType(mime);
        if (!m_codec) { close(); return hevc ? "this device has no HEVC encoder" : "this device has no H.264 encoder"; }

        AMediaFormat* f = AMediaFormat_new();
        AMediaFormat_setString(f, "mime", mime);
        AMediaFormat_setInt32(f, "width", m_ew);
        AMediaFormat_setInt32(f, "height", m_eh);
        AMediaFormat_setInt32(f, "bitrate", bitrate);
        AMediaFormat_setInt32(f, "frame-rate", m_fps);
        AMediaFormat_setInt32(f, "i-frame-interval", 1);
        AMediaFormat_setInt32(f, "color-format", 21);
        media_status_t st = AMediaCodec_configure(m_codec, f, nullptr, nullptr, 1 /* ENCODE */);
        AMediaFormat_delete(f);
        if (st != AMEDIA_OK) { close(); return fmt::format("encoder rejected the settings (code {})", static_cast<int>(st)); }
        st = AMediaCodec_start(m_codec);
        if (st != AMEDIA_OK) { close(); return fmt::format("encoder failed to start (code {})", static_cast<int>(st)); }
        m_codecStarted = true;

        m_muxer = AMediaMuxer_new(m_fd, AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
        if (!m_muxer) { close(); return "couldn't create the MP4 muxer"; }

        size_t ySize = size_t(m_ew) * m_eh;
        m_nv12.assign(ySize * 3 / 2, 0);
        std::memset(m_nv12.data(), 16, ySize);
        std::memset(m_nv12.data() + ySize, 128, ySize / 2);
        m_n = 0; m_started = false;
        return {};
    }

    std::string write(uint8_t const* rgba) {
        if (!m_codecStarted) return "encoder is closed";
        toNv12(rgba);
        for (int tries = 0; tries < 500; ++tries) {
            ssize_t i = AMediaCodec_dequeueInputBuffer(m_codec, 10000);
            if (i >= 0) {
                size_t cap = 0;
                uint8_t* b = AMediaCodec_getInputBuffer(m_codec, static_cast<size_t>(i), &cap);
                if (!b || cap < m_nv12.size()) return "encoder input buffer is smaller than a frame";
                std::memcpy(b, m_nv12.data(), m_nv12.size());
                int64_t pts = m_n * 1000000LL / m_fps;
                if (AMediaCodec_queueInputBuffer(m_codec, static_cast<size_t>(i), 0, m_nv12.size(),
                        static_cast<uint64_t>(pts), 0) != AMEDIA_OK) return "couldn't queue a frame";
                ++m_n;
                return drain(false);
            }
            auto e = drain(false);
            if (!e.empty()) return e;
        }
        return "encoder stalled";
    }

    void close() {
        if (m_codec && m_codecStarted && m_n > 0) {
            for (int tries = 0; tries < 200; ++tries) {
                ssize_t i = AMediaCodec_dequeueInputBuffer(m_codec, 10000);
                if (i >= 0) {
                    AMediaCodec_queueInputBuffer(m_codec, static_cast<size_t>(i), 0, 0,
                        static_cast<uint64_t>(m_n * 1000000LL / m_fps), 4 /* END_OF_STREAM */);
                    drain(true);
                    break;
                }
                if (!drain(false).empty()) break;
            }
        }
        if (m_codec) {
            if (m_codecStarted) AMediaCodec_stop(m_codec);
            AMediaCodec_delete(m_codec);
            m_codec = nullptr; m_codecStarted = false;
        }
        if (m_muxer) {
            if (m_started) AMediaMuxer_stop(m_muxer);
            AMediaMuxer_delete(m_muxer);
            m_muxer = nullptr; m_started = false;
        }
        if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }
    }

private:
    AMediaCodec* m_codec = nullptr;
    AMediaMuxer* m_muxer = nullptr;
    int m_fd = -1, m_track = -1;
    int m_w = 0, m_h = 0, m_ew = 0, m_eh = 0, m_fps = 30;
    int64_t m_n = 0;
    bool m_codecStarted = false, m_started = false;
    std::vector<uint8_t> m_nv12;

    std::string drain(bool eos) {
        int spins = 0;
        for (;;) {
            AMediaCodecBufferInfo info{};
            ssize_t o = AMediaCodec_dequeueOutputBuffer(m_codec, &info, eos ? 10000 : 0);
            if (o >= 0) {
                size_t cap = 0;
                uint8_t* b = AMediaCodec_getOutputBuffer(m_codec, static_cast<size_t>(o), &cap);
                if (b && info.size > 0 && !(info.flags & 2) && m_started) {
                    if (AMediaMuxer_writeSampleData(m_muxer, static_cast<size_t>(m_track), b, &info) != AMEDIA_OK) {
                        AMediaCodec_releaseOutputBuffer(m_codec, static_cast<size_t>(o), false);
                        return "couldn't write to the MP4 file";
                    }
                }
                bool end = (info.flags & 4) != 0;
                AMediaCodec_releaseOutputBuffer(m_codec, static_cast<size_t>(o), false);
                if (end) return {};
            } else if (o == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                AMediaFormat* of = AMediaCodec_getOutputFormat(m_codec);
                ssize_t t = AMediaMuxer_addTrack(m_muxer, of);
                AMediaFormat_delete(of);
                if (t < 0 || AMediaMuxer_start(m_muxer) != AMEDIA_OK) return "couldn't start the MP4 muxer";
                m_track = static_cast<int>(t);
                m_started = true;
            } else if (o == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                if (!eos) return {};
                if (++spins > 300) return "encoder didn't finish";
            }
        }
    }

    void toNv12(uint8_t const* src) {
        uint8_t* Y = m_nv12.data();
        uint8_t* UV = Y + size_t(m_ew) * m_eh;
        for (int y = 0; y < m_h; ++y) {
            uint8_t const* row = src + size_t(m_h - 1 - y) * m_w * 4;
            uint8_t* yo = Y + size_t(y) * m_ew;
            for (int x = 0; x < m_w; ++x) {
                int r = row[x * 4], g = row[x * 4 + 1], b = row[x * 4 + 2];
                yo[x] = static_cast<uint8_t>(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
            }
        }
        for (int y = 0; y + 1 < m_h; y += 2) {
            uint8_t const* r0 = src + size_t(m_h - 1 - y) * m_w * 4;
            uint8_t const* r1 = src + size_t(m_h - 2 - y) * m_w * 4;
            uint8_t* uvo = UV + size_t(y / 2) * m_ew;
            for (int x = 0; x + 1 < m_w; x += 2) {
                int r = (r0[x*4] + r0[x*4+4] + r1[x*4] + r1[x*4+4]) >> 2;
                int g = (r0[x*4+1] + r0[x*4+5] + r1[x*4+1] + r1[x*4+5]) >> 2;
                int b = (r0[x*4+2] + r0[x*4+6] + r1[x*4+2] + r1[x*4+6]) >> 2;
                uvo[x]     = static_cast<uint8_t>(std::clamp(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128, 0, 255));
                uvo[x + 1] = static_cast<uint8_t>(std::clamp(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128, 0, 255));
            }
        }
    }
};

class Renderer : public RenderUi {
public:
    enum class Phase { Idle, Armed, Rendering };
    static Renderer& get() { static Renderer r; r.loadOnce(); return r; }

    ~Renderer() { release(); }
    Phase phase() const { return m_phase; }
    bool rendering() const { return m_phase == Phase::Rendering; }
    uint64_t frames() const { return m_frames; }
    int fps() const { return m_fps; }

    static bool available() { return true; }
    void loadOnce() { if (!m_loaded) { m_loaded = true; load(); } }
    static std::vector<std::string> codecs() { return { "H.264", "HEVC" }; }

    void arm() {
        m_phase = Phase::Armed;
    }
    void cancel() { if (rendering()) finish("stopped"); else m_phase = Phase::Idle; }
    void onRestart() {
        if (m_phase == Phase::Armed) begin();
        else if (rendering() && m_frames > 1) finish("level restarted");
    }
    void notePlayerEnded(int tailSeconds) { if (rendering() && m_stopIn < 0) m_stopIn = tailSeconds * m_fps; }

    int ticksForNextFrame(int tps) {
        m_debt += double(tps) / double(m_fps);
        int n = int(m_debt);
        m_debt -= n;
        return std::max(1, n);
    }

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
    bool m_loaded = false;
    std::unique_ptr<MediaEncoder> m_enc;
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
                auto err = m_enc->write(f.data());
                if (!err.empty()) {
                    std::lock_guard g(m_mx);
                    m_error = err;
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
        std::filesystem::path base = cky::stdPath(geode::Mod::get()->getConfigDir()) / "Renders";
        { std::error_code ec; std::filesystem::create_directories(base, ec); }
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

        bool hevc = false;
        {
            std::string c = cfg.codec;
            for (auto& ch : c) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            hevc = c.find("265") != std::string::npos || c.find("hevc") != std::string::npos;
        }

        m_enc = std::make_unique<MediaEncoder>();
        auto err = m_enc->open(m_out.string(), m_w, m_h, m_fps, std::clamp(cfg.bitrateMbps, 1, 60) * 1'000'000, hevc);
        if (!err.empty()) { fail(err); return; }

        m_rt = cocos2d::CCRenderTexture::create(m_w, m_h, cocos2d::kCCTexture2DPixelFormat_RGBA8888);
        if (!m_rt) { fail("couldn't create render texture"); return; }

        m_frames = 0; m_debt = 0.0; m_stopIn = -1;
        m_th = std::thread([this] { worker(); });
        m_phase = Phase::Rendering;
        say(fmt::format("Rendering {}x{} @ {} FPS", m_w, m_h, m_fps));
    }

    void release() {
        stopWorker();
        if (m_enc) { m_enc->close(); m_enc.reset(); }
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

#else
class Renderer : public RenderUi {
public:
    enum class Phase { Idle, Armed, Rendering };
    static Renderer& get() { static Renderer r; r.loadOnce(); return r; }
    Phase phase() const { return Phase::Idle; }
    bool rendering() const { return false; }
    uint64_t frames() const { return 0; }
    int fps() const { return cfg.fps; }
    static bool available() { return false; }
    static std::vector<std::string> codecs() { return {}; }
    void loadOnce() { if (!m_loaded) { m_loaded = true; load(); } }
    void arm() {
        geode::Notification::create("Rendering is only available on Android", geode::NotificationIcon::Error)->show();
    }
    void cancel() {}
    void onRestart() {}
    void notePlayerEnded(int) {}
    void captureFrame() {}
private:
    bool m_loaded = false;
};
#endif

}
