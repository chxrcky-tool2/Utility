#if __has_include(<imgui-cocos.hpp>) && __has_include(<imgui_internal.h>)
#include <imgui.h>
#include <imgui_internal.h>
#define CKY_HAS_IMGUI_INTERNAL 1
#endif

#include "state.hpp"
#include "render/ChxrRender.hpp"

#if defined(CKY_IMGUI_ENABLED) && !__has_include(<imgui-cocos.hpp>)
#error "CKY_IMGUI is ON but imgui-cocos.hpp was not found. Check the matcool/gd-imgui-cocos CPMAddPackage block in CMakeLists.txt."
#endif

#if __has_include(<imgui-cocos.hpp>)

#include <imgui-cocos.hpp>
#include <Geode/modify/CCTouchDispatcher.hpp>
#include <Geode/utils/web.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cfloat>
#include <vector>

namespace {
    bool g_ready = false;

    bool wantsImGui() {
        return Mod::get()->getSettingValue<std::string>("ui-mode") == "ImGui";
    }

    ImU32 U(float r, float g, float b, float a = 1.f) { return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a)); }
    ImU32 U(ImVec4 c, float a = -1.f) { if (a >= 0.f) c.w = a; return ImGui::ColorConvertFloat4ToU32(c); }
    float lerpf(float a, float b, float t) { return a + (b - a) * t; }

    ImVec4 g_accent(0.93f, 0.30f, 0.50f, 1.f);
    ImVec4 g_accentLo(0.62f, 0.14f, 0.30f, 1.f);

    constexpr float kUiScale = 1.4f;

    void applyColors(int theme) {
        int t = std::clamp(theme, 0, cky::ThemeCount - 1);
        auto const& hi = cky::kThemeDefs[t].hi;
        auto const& lo = cky::kThemeDefs[t].lo;
        g_accent   = ImVec4(hi[0], hi[1], hi[2], 1.f);
        g_accentLo = ImVec4(lo[0], lo[1], lo[2], 1.f);
        ImVec4 accHot(std::min(hi[0] + 0.12f, 1.f), std::min(hi[1] + 0.12f, 1.f), std::min(hi[2] + 0.12f, 1.f), 1.f);

        auto& st = ImGui::GetStyle();
        auto& c = st.Colors;

        c[ImGuiCol_Text]             = ImVec4(0.92f, 0.92f, 0.94f, 1.f);
        c[ImGuiCol_TextDisabled]     = ImVec4(0.52f, 0.52f, 0.57f, 1.f);
        c[ImGuiCol_WindowBg]         = ImVec4(0.075f, 0.075f, 0.085f, 1.f);
        c[ImGuiCol_ChildBg]          = ImVec4(0.055f, 0.055f, 0.065f, 1.f);
        c[ImGuiCol_PopupBg]          = ImVec4(0.095f, 0.095f, 0.11f, 1.f);
        c[ImGuiCol_Border]           = ImVec4(0.20f, 0.20f, 0.23f, 1.f);
        c[ImGuiCol_BorderShadow]     = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_TitleBg]          = ImVec4(0.115f, 0.115f, 0.13f, 1.f);
        c[ImGuiCol_TitleBgActive]    = ImVec4(0.14f, 0.14f, 0.16f, 1.f);
        c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.115f, 0.115f, 0.13f, 1.f);
        c[ImGuiCol_MenuBarBg]        = ImVec4(0.115f, 0.115f, 0.13f, 1.f);
        c[ImGuiCol_FrameBg]          = ImVec4(0.135f, 0.135f, 0.155f, 1.f);
        c[ImGuiCol_FrameBgHovered]   = ImVec4(0.18f, 0.18f, 0.21f, 1.f);
        c[ImGuiCol_FrameBgActive]    = ImVec4(0.22f, 0.22f, 0.26f, 1.f);
        c[ImGuiCol_CheckMark]        = g_accent;
        c[ImGuiCol_SliderGrab]       = g_accent;
        c[ImGuiCol_SliderGrabActive] = accHot;
        c[ImGuiCol_Button]           = ImVec4(0.17f, 0.17f, 0.20f, 1.f);
        c[ImGuiCol_ButtonHovered]    = g_accentLo;
        c[ImGuiCol_ButtonActive]     = g_accent;
        c[ImGuiCol_Header]           = g_accentLo;
        c[ImGuiCol_HeaderHovered]    = ImVec4(lerpf(lo[0], hi[0], 0.4f), lerpf(lo[1], hi[1], 0.4f), lerpf(lo[2], hi[2], 0.4f), 1.f);
        c[ImGuiCol_HeaderActive]     = g_accent;
        c[ImGuiCol_Separator]        = ImVec4(0.22f, 0.22f, 0.26f, 1.f);
        c[ImGuiCol_SeparatorHovered] = g_accent;
        c[ImGuiCol_SeparatorActive]  = accHot;
        c[ImGuiCol_ScrollbarBg]      = ImVec4(0.055f, 0.055f, 0.065f, 1.f);
        c[ImGuiCol_ScrollbarGrab]    = ImVec4(0.26f, 0.26f, 0.30f, 1.f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.34f, 0.34f, 0.39f, 1.f);
        c[ImGuiCol_ScrollbarGrabActive]  = g_accent;
        c[ImGuiCol_ResizeGrip]       = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_ResizeGripHovered]= g_accentLo;
        c[ImGuiCol_ResizeGripActive] = g_accent;
        c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.f, 0.f, 0.f, 0.55f);

        st.Alpha = 1.f;
        st.WindowPadding = ImVec2(8.f, 6.f);
        st.FramePadding = ImVec2(8.f, 5.f);
        st.ItemSpacing = ImVec2(8.f, 5.f);
        st.ItemInnerSpacing = ImVec2(6.f, 4.f);
        st.IndentSpacing = 12.f;
        st.GrabMinSize = 16.f;
        st.ScrollbarSize = 18.f;
        st.WindowBorderSize = 1.f;
        st.FrameBorderSize = 0.f;
        st.PopupBorderSize = 1.f;
        st.WindowRounding = 3.f;
        st.FrameRounding = 2.f;
        st.GrabRounding = 2.f;
        st.ChildRounding = 2.f;
        st.PopupRounding = 3.f;
        st.ScrollbarRounding = 2.f;
        st.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        st.WindowMenuButtonPosition = ImGuiDir_Left;
    }

    bool mhToggle(char const* label, bool* v, bool* expand = nullptr) {
        ImGuiID id = ImGui::GetID(label);
        float h = ImGui::GetFrameHeight();
        float aw = expand ? h : 0.f;
        float W = std::max(ImGui::GetContentRegionAvail().x, 40.f);
        ImVec2 p = ImGui::GetCursorScreenPos();

        ImGui::PushID(label);
        bool pressed = ImGui::InvisibleButton("##t", ImVec2(W - aw, h));
        bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
        bool hovArrow = false;
        if (expand) {
            ImGui::SameLine(0.f, 0.f);
            if (ImGui::InvisibleButton("##x", ImVec2(aw, h))) *expand = !*expand;
            hovArrow = ImGui::IsItemHovered();
        }
        ImGui::PopID();
        if (pressed) *v = !*v;

        ImGuiStorage* S = ImGui::GetStateStorage();
        float target = *v ? 1.f : 0.f;
        float a = S->GetFloat(id, target);
        a += (target - a) * std::min(1.f, ImGui::GetIO().DeltaTime * 16.f);
        S->SetFloat(id, a);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 rmin = p, rmax(p.x + W, p.y + h);
        if (act)      dl->AddRectFilled(rmin, rmax, U(1.f, 1.f, 1.f, 0.07f), 2.f);
        else if (hov) dl->AddRectFilled(rmin, rmax, U(1.f, 1.f, 1.f, 0.04f), 2.f);

        char const* end = std::strstr(label, "##");
        ImVec2 ts = ImGui::CalcTextSize(label, end, true);
        ImU32 tcol = U(lerpf(0.58f, 0.95f, a), lerpf(0.58f, 0.95f, a), lerpf(0.62f, 0.97f, a), 1.f);
        dl->AddText(ImVec2(p.x + 4.f, p.y + (h - ts.y) * 0.5f), tcol, label, end);

        float s = h * 0.58f;
        float bx1 = p.x + W - aw - 5.f, bx0 = bx1 - s;
        float by0 = p.y + (h - s) * 0.5f, by1 = by0 + s;
        dl->AddRectFilled(ImVec2(bx0, by0), ImVec2(bx1, by1), U(0.045f, 0.045f, 0.055f, 1.f), 2.f);
        if (a > 0.01f) {
            float m = 2.f;
            dl->AddRectFilled(ImVec2(bx0 + m, by0 + m), ImVec2(bx1 - m, by1 - m), U(g_accent, a), 1.f);
        }
        dl->AddRect(ImVec2(bx0, by0), ImVec2(bx1, by1),
                    U(lerpf(0.30f, g_accent.x, a), lerpf(0.30f, g_accent.y, a), lerpf(0.34f, g_accent.z, a), 1.f), 2.f, 0, hov ? 1.8f : 1.2f);

        if (expand) {
            float cx = p.x + W - aw * 0.5f, cy = p.y + h * 0.5f, r = h * 0.17f;
            ImU32 col = hovArrow ? U(1.f, 1.f, 1.f, 0.95f) : U(1.f, 1.f, 1.f, *expand ? 0.85f : 0.45f);
            if (*expand)
                dl->AddTriangleFilled(ImVec2(cx - r * 1.3f, cy - r * 0.7f), ImVec2(cx + r * 1.3f, cy - r * 0.7f), ImVec2(cx, cy + r * 0.9f), col);
            else
                dl->AddTriangleFilled(ImVec2(cx - r * 0.7f, cy - r * 1.3f), ImVec2(cx - r * 0.7f, cy + r * 1.3f), ImVec2(cx + r * 0.9f, cy), col);
        }
        return pressed;
    }

    float g_subY = 0.f;
    void subBegin() {
        g_subY = ImGui::GetCursorScreenPos().y;
        ImGui::Indent(12.f);
    }
    void subEnd() {
        ImGui::Unindent(12.f);
        ImVec2 q = ImGui::GetCursorScreenPos();
        float x = q.x + 5.f;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(x, g_subY), ImVec2(x, q.y - ImGui::GetStyle().ItemSpacing.y),
                                            U(g_accent, 0.55f), 1.5f);
    }

    void mhHeader(char const* text) {
        ImGui::Dummy(ImVec2(0.f, 2.f));
        ImGui::PushStyleColor(ImGuiCol_Text, g_accent);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    void mhHint(char const* text) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("%s", text);
        ImGui::PopStyleColor();
    }

    bool mhButton(char const* label) { return ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvail().x, 0.f)); }

    int mhButtonPair(char const* a, char const* b) {
        float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        int r = 0;
        if (ImGui::Button(a, ImVec2(w, 0.f))) r = 1;
        ImGui::SameLine();
        if (ImGui::Button(b, ImVec2(w, 0.f))) r = 2;
        return r;
    }

    bool mhSliderF(char const* label, float* v, float lo, float hi, char const* fmt = "%.2f") {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool r = ImGui::SliderFloat("##s", v, lo, hi, fmt);
        ImGui::PopID();
        return r;
    }
    bool mhSliderI(char const* label, int* v, int lo, int hi, char const* fmt = "%d") {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool r = ImGui::SliderInt("##s", v, lo, hi, fmt);
        ImGui::PopID();
        return r;
    }

    bool mhSliderStep(char const* label, float* v, float lo, float hi, char const* fmt, float delta, bool& nudged) {
        float h = ImGui::GetFrameHeight(), sp = ImGui::GetStyle().ItemSpacing.x;
        nudged = false;
        bool changed = false;
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SetNextItemWidth(std::max(40.f, ImGui::GetContentRegionAvail().x - (h + sp) * 2.f));
        if (ImGui::SliderFloat("##s", v, lo, hi, fmt)) changed = true;
        ImGui::SameLine();
        if (ImGui::Button("-", ImVec2(h, h))) { *v = std::clamp(*v - delta, lo, hi); changed = nudged = true; }
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(h, h))) { *v = std::clamp(*v + delta, lo, hi); changed = nudged = true; }
        ImGui::PopID();
        return changed;
    }

#ifdef CKY_HAS_IMGUI_INTERNAL
    int    g_dragMode = 0;
    ImGuiID g_dragOwner = 0;

    void dragScrollFrame() {
        if (ImGui::IsMouseClicked(0) || !ImGui::IsMouseDown(0)) { g_dragMode = 0; g_dragOwner = 0; }
    }

    void dragScroll() {
        if (!ImGui::IsMouseDown(0)) return;
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::GetScrollMaxY() <= 0.f) return;
        if (ImGui::GetMousePos().y < ImGui::GetWindowPos().y + ImGui::GetFrameHeight()) return;
        if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) return;
        ImGuiID wid = ImGui::GetCurrentWindow()->ID;
        if (g_dragMode == 0) {
            if (!ImGui::IsMouseDragging(0, 10.f)) return;
            ImVec2 d = ImGui::GetMouseDragDelta(0, 0.f);
            if (std::fabs(d.y) >= std::fabs(d.x) * 1.5f) {
                g_dragMode = 1;
                g_dragOwner = wid;
                ImGui::ClearActiveID();
            } else {
                g_dragMode = 2;
            }
            return;
        }
        if (g_dragMode != 1 || g_dragOwner != wid) return;
        float maxY = ImGui::GetScrollMaxY();
        if (maxY <= 0.f) return;
        ImGui::SetScrollY(std::clamp(ImGui::GetScrollY() - io.MouseDelta.y, 0.f, maxY));
    }

#else
    void dragScrollFrame() {}
    void dragScroll() {}
#endif

    struct Rect4 { float x0, y0, x1, y1; };
    std::vector<Rect4> g_rects;
    std::vector<Rect4> g_rectsBuild;
    bool g_anyPopup = false;

    struct IconCircle { bool valid = false; float cx = 0.f, cy = 0.f, r = 0.f; };
    IconCircle g_icon;
    bool  g_closing = false, g_snapBack = false, g_touchDbg = false;
    float g_closeT = 0.f;
    float g_startX = 10.f;
    constexpr float kCloseSecs = 0.42f;
    ImVec2 g_home[8];
    bool   g_homeOk[8] = {};

    void startClose() {
        if (cky::imguiOpen && !g_closing) { g_closing = true; g_closeT = 0.f; }
    }

    ImVec2 touchToIm(CCTouch* t) {
        ImVec2 ds = ImGui::GetIO().DisplaySize;
        CCSize win = CCDirector::get()->getWinSize();
        CCPoint p = t->getLocation();
        if (win.width <= 0.f || win.height <= 0.f) return ImVec2(0.f, 0.f);
        return ImVec2(p.x * ds.x / win.width, (win.height - p.y) * ds.y / win.height);
    }

    bool overMenu(ImVec2 q) {
        if (g_closing) return false;
        if (g_icon.valid) {
            float dx = q.x - g_icon.cx, dy = q.y - g_icon.cy;
            if (dx * dx + dy * dy <= g_icon.r * g_icon.r) return false;
        }
        if (g_anyPopup) return true;
        for (auto const& r : g_rects)
            if (q.x >= r.x0 && q.x <= r.x1 && q.y >= r.y0 && q.y <= r.y1) return true;
        return false;
    }

    CCTouch* g_hookTouch = nullptr;

    bool hookFeed(CCSet* set, unsigned int type) {
        if (!set || !cky::imguiOpen || !ImGui::GetCurrentContext()) return false;
        ImGuiIO& io = ImGui::GetIO();
        bool took = false;
        for (auto it = set->begin(); it != set->end(); ++it) {
            auto t = static_cast<CCTouch*>(*it);
            if (!t) continue;
            ImVec2 q = touchToIm(t);
            if (type == CCTOUCHBEGAN) {
                bool over = overMenu(q);
                if (g_touchDbg) geode::log::debug("[cky] touch {:.0f},{:.0f} overMenu={} windows={} closing={}", q.x, q.y, over, g_rects.size(), g_closing);
                if (g_hookTouch || !over) continue;
                g_hookTouch = t;
                io.AddMousePosEvent(q.x, q.y);
                io.AddMouseButtonEvent(0, true);
                took = true;
            } else if (t == g_hookTouch) {
                io.AddMousePosEvent(q.x, q.y);
                if (type == CCTOUCHENDED || type == CCTOUCHCANCELLED) {
                    io.AddMouseButtonEvent(0, false);
                    g_hookTouch = nullptr;
                }
                took = true;
            }
        }
        return took;
    }

    class ImTouch : public CCLayer {
        CCTouch* m_active = nullptr;

        static ImVec2 toIm(CCTouch* t) { return touchToIm(t); }

        void finish(CCTouch* t) {
            if (t != m_active) return;
            m_active = nullptr;
            if (!ImGui::GetCurrentContext()) return;
            ImGuiIO& io = ImGui::GetIO();
            ImVec2 q = toIm(t);
            io.AddMousePosEvent(q.x, q.y);
            io.AddMouseButtonEvent(0, false);
        }

    public:
        static ImTouch* create() {
            auto r = new ImTouch();
            if (r && r->init()) { r->autorelease(); return r; }
            CC_SAFE_DELETE(r);
            return nullptr;
        }

        bool init() override {
            if (!CCLayer::init()) return false;
            this->setTouchEnabled(true);
            return true;
        }

        void registerWithTouchDispatcher() override {
            CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(this, -2000, true);
        }

        bool ccTouchBegan(CCTouch* t, CCEvent*) override {
            if (m_active || !cky::imguiOpen || !ImGui::GetCurrentContext()) return false;
            ImVec2 q = toIm(t);
            if (!overMenu(q)) return false;
            m_active = t;
            ImGuiIO& io = ImGui::GetIO();
            io.AddMousePosEvent(q.x, q.y);
            io.AddMouseButtonEvent(0, true);
            return true;
        }

        void ccTouchMoved(CCTouch* t, CCEvent*) override {
            if (t != m_active || !ImGui::GetCurrentContext()) return;
            ImVec2 q = toIm(t);
            ImGui::GetIO().AddMousePosEvent(q.x, q.y);
        }

        void ccTouchEnded(CCTouch* t, CCEvent*) override { finish(t); }
        void ccTouchCancelled(CCTouch* t, CCEvent*) override { finish(t); }
    };

    ImTouch* g_touchLayer = nullptr;

    void touchBridgeRegister() {
        if (!cky::imguiOpen) return;
        CCScene* scene = CCDirector::get()->getRunningScene();
        if (!scene) return;
        if (!g_touchLayer) {
            g_touchLayer = ImTouch::create();
            if (!g_touchLayer) return;
            g_touchLayer->retain();
        }
        if (g_touchLayer->getParent() == scene) return;
        g_touchLayer->removeFromParentAndCleanup(false);
        scene->addChild(g_touchLayer, 2147483000);
    }

    void styleSetup() {
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
        ImGui::GetStyle() = ImGuiStyle();
        ImGui::StyleColorsDark();
        ImGui::GetIO().FontGlobalScale = kUiScale;
        applyColors(cky::theme);
    }

    void winBot() {
        static bool exStep = false, exFix = false;

        bool rec = cky::recording;
        if (mhToggle("Record", &rec)) {
            if (!rec) {
                cky::recording = false;
                cky::recArmed = false;
            } else {
                cky::recording = true;
                cky::playing = false;
                cky::playArmed = false;
                cky::macroTps = cky::effTps();
                cky::recArmed = (cky::step == 0);
                if (!cky::recArmed) ckyNotify("Recording starts when you restart the level");
            }
        }

        bool play = cky::playing;
        if (mhToggle("Playback", &play)) {
            if (!play) {
                cky::playing = false;
                cky::playArmed = false;
            } else if (cky::macro.empty()) {
                ckyNotify("No macro loaded", false);
            } else {
                cky::playing = true;
                cky::recording = false;
                cky::recArmed = false;
                cky::cursor = 0;
                cky::playArmed = (cky::step == 0);
                if (!cky::playArmed) ckyNotify("Playback ON: restart the level to start it");
            }
        }

        mhHeader("Stepping and fixes");
        bool fz = cky::frozen;
        if (mhToggle("Frame stepper", &fz, &exStep)) {
            cky::frozen = fz;
            cky::pending = 0;
            ckyApplyAudio();
        }
        if (exStep) {
            subBegin();
            int si = cky::sfInterval;
            if (mhSliderI("Snapshot interval (steps)", &si, 1, 960)) cky::sfInterval = std::clamp(si, 1, 960);
            int sd = cky::sfDepth;
            if (mhSliderI("Snapshot depth (steps)", &sd, 30, 960)) cky::sfDepth = std::clamp(sd, 30, 960);
            subEnd();
        }

        bool ff = cky::frameFixes;
        if (mhToggle("Frame fixes", &ff, &exFix)) {
            if (cky::recording) ckyNotify("Stop recording first", false);
            else cky::frameFixes = ff;
        }
        if (exFix) {
            subBegin();
            int fi = cky::fixInterval;
            if (mhSliderI("Fix interval (steps)", &fi, 1, 240)) {
                if (cky::recording) ckyNotify("Stop recording first", false);
                else cky::fixInterval = std::clamp(fi, 1, 240);
            }
            mhToggle("Fix velocity", &cky::fixVelocity);
            subEnd();
        }

        bool pf = cky::practiceFixes;
        if (mhToggle("Practice fixes", &pf)) {
            if (cky::recording) ckyNotify("Stop recording first", false);
            else cky::practiceFixes = pf;
        }

        mhHeader("Macro");
        {
            static char macroBuf[cky::MACRO_NAME_MAX + 1] = {};
            static std::string lastSeen;
            if (lastSeen != cky::macroName) {
                std::snprintf(macroBuf, sizeof macroBuf, "%s", cky::macroName.c_str());
                lastSeen = cky::macroName;
            }
            auto filter = [](ImGuiInputTextCallbackData* d) -> int {
                return (d->EventChar < 128 && cky::macroCharOk(static_cast<char>(d->EventChar))) ? 0 : 1;
            };
            ImGui::TextUnformatted("Macro name");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputText("##macroName", macroBuf, sizeof macroBuf, ImGuiInputTextFlags_CallbackCharFilter, filter)) {
                cky::setMacroName(macroBuf);
                lastSeen = cky::macroName;
            }
        }

        static std::vector<cky::MacroEntry> s_picks;
        int pr = mhButtonPair("Save", "Load");
        if (pr == 1) {
            if (!cky::macroNameOk()) {
                ckyNotify("Type a macro name first", false);
            } else {
                bool ok = cky::save();
                ckyNotify(ok ? fmt::format("Saved {}", cky::macroFileName()).c_str() : "Save failed", ok);
            }
        } else if (pr == 2) {
            s_picks = cky::listMacros();
            ImGui::OpenPopup("Load macro##pick");
        }
        {
            ImVec2 mid(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
            ImGui::SetNextWindowPos(mid, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("Load macro##pick", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("Tap a saved macro to load it");
                if (ImGui::BeginChild("##pickList", ImVec2(300.f, 220.f), true)) {
                    if (s_picks.empty()) ImGui::TextDisabled("No saved macros yet");
                    for (size_t i = 0; i < s_picks.size(); ++i) {
                        ImGui::PushID(static_cast<int>(i));
                        if (ImGui::Selectable(s_picks[i].display.c_str(), false, 0, ImVec2(0.f, 26.f))) {
                            bool ok = cky::loadMacroEntry(s_picks[i]);
                            ckyNotify(ok ? fmt::format("Loaded {}", s_picks[i].display).c_str() : "Load failed", ok);
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::PopID();
                    }
                    dragScroll();
                }
                ImGui::EndChild();
                if (mhButton("Cancel")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }

        int cr = mhButtonPair("Clear", "Import GDR");
        if (cr == 1) {
            cky::clearMacro();
            ckyNotify("Macro buffer cleared");
        } else if (cr == 2) {
            ckyImportGdr();
        }

        mhSliderI("Input offset", &cky::frameOffset, -10, 10);

        ImGui::Separator();
        ImGui::TextDisabled("Inputs: %llu   Macro TPS: %d", static_cast<unsigned long long>(cky::macro.size()),
                            static_cast<int>(cky::macroTps));
        if (cky::macroNameOk()) {
            ImGui::TextDisabled("%s: %s", cky::macroFileName().c_str(), cky::slotExists() ? "saved" : "empty");
        } else {
            ImGui::TextDisabled("Type a macro name to save or load");
        }
    }

    void winGlobal() {
        mhHeader("TPS");
        bool tpsOn = cky::tpsOn;
        if (mhToggle("TPS bypass", &tpsOn)) {
            if (cky::recording) ckyNotify("Stop recording first", false);
            else { cky::tpsOn = tpsOn; cky::resetMeasure(); }
        }

        {
            float t = cky::tps;
            bool nudged = false;
            if (mhSliderStep("TPS", &t, cky::TPS_MIN, cky::TPS_MAX, "%.0f", cky::tpsStep, nudged)) {
                if (cky::recording) ckyNotify("Stop recording first", false);
                else { cky::tps = nudged ? t : std::round(t); cky::tpsOn = true; cky::resetMeasure(); }
            }
        }

        int sr = mhButtonPair(fmt::format("Step: {}", static_cast<int>(cky::tpsStep)).c_str(),
                              (cky::tpsOn && cky::tps >= 480.f)
                                  ? fmt::format("High: {}", static_cast<int>(cky::tps)).c_str()
                                  : "High: OFF");
        if (sr == 1) {
            float n = cky::nextVal(cky::tpsStepList, cky::tpsStep, 1);
            if (n == cky::tpsStep) n = cky::tpsStepList.front();
            cky::tpsStep = n;
        } else if (sr == 2) {
            if (cky::recording) ckyNotify("Stop recording first", false);
            else {
                cky::tps = cky::nextHighTps(cky::tpsOn ? cky::tps : 240.f);
                cky::tpsOn = true;
                cky::resetMeasure();
            }
        }

        mhToggle("Delta lock (>240 TPS)", &cky::deltaLock);
        if (cky::deltaLock) {
            subBegin();
            mhSliderF("Delta lock TPS", &cky::deltaLockTps, 20.f, 120.f, "%.0f TPS");
            subEnd();
        }
        if (cky::actualTps > 0.f) {
            ImGui::TextDisabled("Real TPS: %d / %d", static_cast<int>(cky::actualTps + 0.5f), static_cast<int>(cky::effTps()));
        }

        mhHeader("Speedhack");
        bool spOn = cky::speedOn;
        if (mhToggle("Speedhack", &spOn)) {
            cky::speedOn = spOn;
            ckyApplyAudio();
        }
        {
            float sp = cky::speed;
            bool nudged = false;
            if (mhSliderStep("Speed", &sp, 0.01f, 4.f, "%.2fx", 0.01f, nudged)) {
                cky::speed = std::clamp(std::round(sp * 100.f) / 100.f, 0.01f, 4.f);
                cky::speedOn = true;
                ckyApplyAudio();
            }
        }
        int pr = mhButtonPair("Slower", "Faster");
        if (pr) {
            cky::speed = cky::nextVal(cky::speedList, cky::speed, pr == 1 ? -1 : 1);
            cky::speedOn = true;
            ckyApplyAudio();
        }

        bool au = cky::audioSpeed;
        if (mhToggle("Audio follows speed", &au)) {
            cky::audioSpeed = au;
            ckyApplyAudio();
        }
    }

    void winPlayer() {
        static bool exFw = false;

        bool nc = cky::noclip;
        if (mhToggle("Noclip", &nc)) {
            cky::noclip = nc;
            cky::ncReset();
        }
        mhToggle("Safe mode", &cky::safeMode);

        mhHeader("Frame window");
        bool fw = cky::fwOn;
        if (mhToggle("Frame window calculator", &fw, &exFw)) {
            cky::fwOn = fw;
            ckyFwSave();
        }
        if (cky::fwOn && cky::hudOn) {
            ImGui::TextDisabled("Turn HUD off (Display) to see the label.");
            if (mhButton("Turn HUD off")) {
                cky::hudOn = false;
                if (auto h = ckyHudNode()) h->setVisible(false);
            }
        }
        if (exFw) {
            subBegin();
            bool ch = false;
            ch |= mhToggle("Show label", &cky::fwShow);
            ch |= mhToggle("Jingle Bells on perfect", &cky::fwJingle);
            ch |= mhSliderF("Count speed", &cky::fwSpeed, 0.25f, 2.f, "%.2fx");
            ch |= mhSliderI("Frames ahead", &cky::fwAhead, 1, 10);
            ch |= mhSliderI("Frames behind", &cky::fwBack, 0, 10);
            ch |= mhSliderI("Check steps", &cky::fwLook, 20, 240);
            ch |= mhSliderI("Perfect (steps)", &cky::fwPerfect, 1, 6);
            ch |= mhSliderI("Jingle volume", &cky::fwVol, 0, 200, "%d%%");
            ch |= mhSliderF("Label size", &cky::fwScale, 0.5f, 2.5f, "%.1fx");
            if (ch) ckyFwSave();
            if (mhButton("Test jingle")) ckyFwTestSound();
            auto const& fr = ckyFwResult();
            if (fr.valid) ImGui::TextDisabled("Last click: %.2f ms, %d sims", fr.ms, fr.sims);
            subEnd();
        }
    }

    void winDisplay() {
        static bool exTrail = false, exHud = false, exTraj = false;

        bool hb = cky::hitboxes;
        if (mhToggle("Hitboxes", &hb)) {
            cky::hitboxes = hb;
            ckyApplyHitboxes();
        }

        bool ht = cky::hitboxTrail;
        if (mhToggle("Hitbox trail", &ht, &exTrail)) {
            cky::hitboxTrail = ht;
            ckyTrailApply();
        }
        if (exTrail) {
            subBegin();
            if (mhSliderI("Trail length (steps)", &cky::trailLen, 20, 1200)) ckyTrailSave();
            if (ImGui::ColorEdit4("Outer hitbox", cky::trailOuter, ImGuiColorEditFlags_NoInputs)) ckyTrailSave();
            if (ImGui::ColorEdit4("Inner hitbox", cky::trailInner, ImGuiColorEditFlags_NoInputs)) ckyTrailSave();
            subEnd();
        }

        bool hud = cky::hudOn;
        if (mhToggle("HUD", &hud, &exHud)) {
            cky::hudOn = hud;
            if (auto h = ckyHudNode()) h->setVisible(cky::hudOn);
        }
        if (exHud) {
            subBegin();
            float hs = cky::hudScale;
            if (mhSliderF("HUD size", &hs, 0.5f, 2.f, "%.1fx")) {
                cky::hudScale = hs;
                ckyApplyHudScale();
            }
            int hop = static_cast<int>(std::lround(cky::hudOpacity * 100.f));
            if (mhSliderI("HUD opacity", &hop, 10, 100, "%d%%")) {
                cky::hudOpacity = std::clamp(static_cast<float>(hop) / 100.f, 0.1f, 1.f);
                cky::hudOpacitySave();
            }
            mhToggle("Show frame stepper", &cky::hudStepShow);
            mhToggle("Show speedhack", &cky::hudSpeedShow);
            bool sb = cky::stepperBtns;
            if (mhToggle("Frame stepper buttons", &sb)) {
                cky::stepperBtns = sb;
                if (!sb) {
                    cky::frozen = false;
                    cky::pending = 0;
                    ckyApplyAudio();
                }
            }
            bool sh = cky::speedBtns;
            if (mhToggle("Speedhack buttons", &sh)) {
                cky::speedBtns = sh;
                ckyApplyAudio();
            }
            bool bs = cky::backStepOn;
            if (mhToggle("Back step", &bs)) {
                cky::backStepOn = bs;
                if (!bs) ckyBackStepClear();
            }
            mhToggle("Buffer while playing", &cky::backAlways);
            ckyHudRefresh();
            subEnd();
        }

        bool sp = cky::startPosOn;
        if (mhToggle("StartPos switcher", &sp)) {
            cky::startPosOn = sp;
            ckySetStartPosVisible(cky::startPosOn);
        }
        mhToggle("Input overlay", &cky::overlayOn);

        bool tj = cky::trajectory;
        if (mhToggle("Show trajectory", &tj, &exTraj)) {
            cky::trajectory = tj;
            if (!tj) ckyTrajClear();
            else cky::dirty = true;
        }
        if (exTraj) {
            subBegin();
            mhSliderI("Trajectory length", &cky::trajLen, 60, 480);
            subEnd();
        }
    }

    void winLabels() {
        bool ch = false;
        ch |= mhToggle("CPS", &cky::clickHudOn);
        ch |= mhToggle("FPS", &cky::lblFps);
        ch |= mhToggle("Noclip accuracy", &cky::ncShowAcc);
        ch |= mhToggle("Noclip deaths", &cky::ncShowDeaths);
        ch |= mhToggle("Startpos run", &cky::lblRun);
        ch |= mhToggle("Level time", &cky::lblTime);
        ch |= mhToggle("Best run", &cky::lblBest);
        if (ch) cky::labelsSave();
    }

    void winClicks() {
        bool cb = cky::clickbotOn;
        if (mhToggle("Clickbot sounds", &cb)) {
            cky::clickbotOn = cb;
            ckyClickRescan();
        }

        ImGui::TextUnformatted("Pack");
        {
            float h = ImGui::GetFrameHeight(), sp = ImGui::GetStyle().ItemSpacing.x;
            if (ImGui::Button("<", ImVec2(h, h))) ckyClickPackStep(-1);
            ImGui::SameLine();
            float tw = std::max(30.f, ImGui::GetContentRegionAvail().x - h - sp);
            std::string nm = ckyClickPackName();
            ImVec2 ts = ImGui::CalcTextSize(nm.c_str());
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::PushClipRect(p, ImVec2(p.x + tw, p.y + h), true);
            ImGui::GetWindowDrawList()->AddText(ImVec2(p.x + std::max(0.f, (tw - ts.x) * 0.5f), p.y + (h - ts.y) * 0.5f),
                                                ImGui::GetColorU32(ImGuiCol_Text), nm.c_str());
            ImGui::PopClipRect();
            ImGui::Dummy(ImVec2(tw, h));
            ImGui::SameLine();
            if (ImGui::Button(">", ImVec2(h, h))) ckyClickPackStep(1);
        }

        int vol = cky::clickVol;
        if (mhSliderI("Volume", &vol, 0, 200, "%d%%")) {
            cky::clickVol = vol;
            Mod::get()->setSavedValue<int>("click-vol", cky::clickVol);
        }
        mhHint("Packs go in game/clicks/<PackName>/ with player1/clicks, releases, softclicks...");
    }

    void winRendering() {
        auto& R = chxr::Renderer::get();
        if (!R.available()) {
            ImGui::TextWrapped("Rendering uses the built-in Android encoder and is only available on Android.");
        }

        auto codecs = chxr::Renderer::codecs();
        ImGui::TextUnformatted("Codec");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (!codecs.empty()) {
            if (ImGui::BeginCombo("##codec", R.cfg.codec.empty() ? "auto" : R.cfg.codec.c_str())) {
                for (auto const& c : codecs) {
                    if (ImGui::Selectable(c.c_str(), c == R.cfg.codec)) { R.cfg.codec = c; R.save(); }
                }
                ImGui::EndCombo();
            }
        } else {
            static char codecBuf[64] = {};
            static bool codecInit = false;
            if (!codecInit) { std::snprintf(codecBuf, sizeof codecBuf, "%s", R.cfg.codec.c_str()); codecInit = true; }
            if (ImGui::InputText("##codec", codecBuf, sizeof codecBuf)) { R.cfg.codec = codecBuf; R.save(); }
        }

        static char nameBuf[64] = {};
        static bool nameInit = false;
        if (!nameInit) { std::snprintf(nameBuf, sizeof nameBuf, "%s", R.cfg.name.c_str()); nameInit = true; }
        ImGui::TextUnformatted("File name");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##fileName", nameBuf, sizeof nameBuf)) { R.cfg.name = nameBuf; R.save(); }

        static char const* resNames[4] = { "1920x1080", "1280x720", "854x480", "640x360" };
        int ri = R.resIndex();
        ImGui::TextUnformatted("Resolution");
        {
            float bw = ImGui::CalcTextSize("Help").x + ImGui::GetStyle().FramePadding.x * 2.f;
            ImGui::SetNextItemWidth(std::max(40.f, ImGui::GetContentRegionAvail().x - bw - ImGui::GetStyle().ItemSpacing.x));
            if (ImGui::Combo("##res", &ri, resNames, 4)) { R.setRes(ri); R.save(); }
            ImGui::SameLine();
            if (ImGui::Button("Help")) ckyNotify(R.autoSetup().c_str(), true);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reads your phone chipset and sets the resolution");
        }

        int fps = R.cfg.fps;
        if (mhSliderI("FPS", &fps, 1, 30)) { R.cfg.fps = fps; R.save(); }
        int br = R.cfg.bitrateMbps;
        if (mhSliderI("Bitrate (Mbps)", &br, 1, 60)) { R.cfg.bitrateMbps = br; R.save(); }

        ImGui::Separator();
        if (R.rendering()) {
            ImGui::Text("Rendering: %llu frames (%.1fs)", static_cast<unsigned long long>(R.frames()),
                static_cast<double>(R.frames()) / static_cast<double>(std::max(1, R.fps())));
            if (mhButton("Stop render")) R.cancel();
        } else if (R.phase() == chxr::Renderer::Phase::Armed) {
            ImGui::TextUnformatted("Armed: restart the level to begin");
            if (mhButton("Disarm")) R.cancel();
        } else {
            if (mhButton("Start render (arm)")) R.arm();
            mhHint("Arm, then restart the level. Video saves to the mod config folder / Renders. Max 1080p, 30 FPS.");
        }
    }

    void winSettings() {
        float is = cky::iconScale;
        if (mhSliderF("Cky icon size", &is, 0.5f, 2.f, "%.1fx")) {
            cky::iconScale = is;
            if (auto scene = CCDirector::get()->getRunningScene()) {
                if (auto ic = scene->getChildByIDRecursive("cky-icon")) ic->setScale(cky::iconScale);
            }
        }
        if (mhButton("Reset icon position")) {
            cky::savePos("cky-icon", CCPoint(-99999.f, -99999.f));
            if (auto scene = CCDirector::get()->getRunningScene()) {
                if (auto ic = scene->getChildByIDRecursive("cky-icon")) ic->setPosition(ckyDefaultIconPos());
            }
        }
        mhToggle("Checkpoint telemetry", &cky::ckptTelemetry);

        int th = std::clamp(cky::theme, 0, cky::ThemeCount - 1);
        if (mhSliderI("Accent theme", &th, 0, cky::ThemeCount - 1, cky::kThemeDefs[th].name)) {
            cky::theme = th;
            Mod::get()->setSavedValue<int>("theme", cky::theme);
            applyColors(th);
        }

        ImGui::Separator();
        if (mhButton("Hide menu")) startClose();
        mhToggle("Touch debug", &g_touchDbg);

        ImGui::Separator();
        ImGui::Text("ChxrCky Macro Bot v1.0.6-beta.5");
        ImGui::TextDisabled("(c) 2026 ChxrCky - GNU GPL v3.0");
        ImGui::TextDisabled("Free software, NO WARRANTY.");
        if (mhButton("Open GPL-3.0 license")) {
            web::openLinkInBrowser("https:\057\057www.gnu.org/licenses/gpl-3.0.html");
        }
    }

    void drawFrameWindow() {
        if (!cky::fwOn || !cky::fwShow || cky::hudOn) return;
        auto pl = PlayLayer::get();
        if (!pl || !pl->m_objectLayer) return;
        auto const& r = ckyFwResult();
        if (!r.valid) return;

        double age = cky::fwNow() - r.stamp;
        float rate = 20.f * std::clamp(cky::fwSpeed, 0.25f, 2.f);
        int start = std::max(r.lo, 0);
        int endv = r.hi;
        bool ahead = r.window && endv >= 0;
        float countSecs = ahead ? static_cast<float>(endv) / rate : 0.f;
        if (age > countSecs + cky::fwHold) return;
        float fade = std::clamp(static_cast<float>((countSecs + cky::fwHold - age) / 0.5), 0.f, 1.f);

        int counted = static_cast<int>(age * rate);
        bool done = !ahead || counted >= endv;
        int shownEnd = std::clamp(counted, start, std::max(start, endv));

        ImGuiIO& io = ImGui::GetIO();
        CCSize win = CCDirector::get()->getWinSize();
        if (win.width <= 0.f || win.height <= 0.f) return;
        CCPoint w = pl->m_objectLayer->convertToWorldSpace(r.anchor);
        float sx = io.DisplaySize.x / win.width, sy = io.DisplaySize.y / win.height;

        static constexpr int kTiers[3] = { 60, 120, 240 };
        int tier = r.window ? ckyFwTier(r.width, r.stepHz) : 0;
        int status[3];
        for (int i = 0; i < 3; ++i) {
            int need = std::max(1, static_cast<int>(std::ceil(r.stepHz / kTiers[i] - 1e-4f)));
            if (r.window && r.width >= need) status[i] = 2;
            else if (r.window && r.hit) status[i] = 1;
            else status[i] = 0;
        }

        ImU32 green = U(0.35f, 1.f, 0.45f, fade), red = U(1.f, 0.35f, 0.35f, fade), amber = U(1.f, 0.78f, 0.25f, fade);
        char l1[48], l2[64];
        ImU32 accent;
        if (!done) {
            accent = amber;
            std::snprintf(l1, sizeof l1, "counting...");
            if (counted < start) std::snprintf(l2, sizeof l2, "frames to click: %d", counted);
            else                 std::snprintf(l2, sizeof l2, "frames to click: %d-%d", start, shownEnd);
        } else if (r.hit) {
            accent = green;
            if (tier > 0) std::snprintf(l1, sizeof l1, "%s%d FW", r.truncated ? ">= " : "", tier);
            else          std::snprintf(l1, sizeof l1, "> 240 FW");
            std::snprintf(l2, sizeof l2, "frames to click: %d-%d", start, endv);
        } else {
            accent = red;
            std::snprintf(l1, sizeof l1, "MISSED");
            if (ahead)            std::snprintf(l2, sizeof l2, "frames to click: %d-%d", start, endv);
            else if (r.window)    std::snprintf(l2, sizeof l2, "window was %d frame%s ago", -endv, endv == -1 ? "" : "s");
            else                  std::snprintf(l2, sizeof l2, "no window in range");
        }
        bool showRow = done && r.window;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        ImFont* font = ImGui::GetFont();
        float fs = ImGui::GetFontSize() * cky::fwScale;
        float pad = fs * 0.45f, gap = fs * 0.8f;
        ImVec2 s1 = font->CalcTextSizeA(fs, FLT_MAX, 0.f, l1);
        ImVec2 s2 = font->CalcTextSizeA(fs * 0.85f, FLT_MAX, 0.f, l2);
        char tb[3][8];
        ImVec2 ts[3];
        float rowW = 0.f;
        for (int i = 0; i < 3; ++i) {
            std::snprintf(tb[i], sizeof tb[i], "%d", kTiers[i]);
            ts[i] = font->CalcTextSizeA(fs * 0.8f, FLT_MAX, 0.f, tb[i]);
            rowW += ts[i].x + (i ? gap : 0.f);
        }
        float bw = std::max({ s1.x, s2.x, showRow ? rowW : 0.f }) + pad * 2.f;
        float bh = s1.y + s2.y + (showRow ? ts[0].y + fs * 0.2f : 0.f) + pad * 2.f;
        ImVec2 tl(w.x * sx - bw * 0.5f, (win.height - w.y) * sy - bh - fs * 0.6f);
        tl.x = std::clamp(tl.x, 2.f, std::max(2.f, io.DisplaySize.x - bw - 2.f));
        tl.y = std::max(2.f, tl.y);

        dl->AddRectFilled(tl, ImVec2(tl.x + bw, tl.y + bh), U(0.f, 0.f, 0.f, 0.72f * fade), fs * 0.3f);
        dl->AddRect(tl, ImVec2(tl.x + bw, tl.y + bh), accent, fs * 0.3f, 0, 1.5f);
        float cx = tl.x + bw * 0.5f, y = tl.y + pad;
        dl->AddText(font, fs, ImVec2(cx - s1.x * 0.5f, y), accent, l1);
        y += s1.y;
        dl->AddText(font, fs * 0.85f, ImVec2(cx - s2.x * 0.5f, y), U(0.85f, 0.85f, 0.85f, fade), l2);
        if (showRow) {
            y += s2.y + fs * 0.2f;
            float x = cx - rowW * 0.5f;
            for (int i = 0; i < 3; ++i) {
                ImU32 col = status[i] == 2 ? green : status[i] == 1 ? amber : red;
                dl->AddText(font, fs * 0.8f, ImVec2(x, y), col, tb[i]);
                x += ts[i].x + gap;
            }
            if (r.perfect) dl->AddText(font, fs * 0.8f, ImVec2(tl.x + bw + fs * 0.3f, tl.y), U(1.f, 0.85f, 0.2f, fade), "PERFECT");
        }
    }

    int scatterDir(float dx, float dy, int slot) {
        if (std::fabs(dx) < 1.f && std::fabs(dy) < 1.f) return slot & 7;
        float ang = std::atan2(dy, dx);
        return (static_cast<int>(std::lround(ang / 0.78539816f)) + 8) & 7;
    }

    void mhWindow(char const* title, int slot, float width, ImVec2 disp, void (*body)()) {
        constexpr float kMargin = 10.f, kGap = 8.f;
        int s = slot & 7;
        int perRow = std::max(1, static_cast<int>((disp.x - g_startX) / (width + kGap)));
        int col = slot % perRow, row = slot / perRow;
        ImGui::SetNextWindowPos(ImVec2(g_startX + static_cast<float>(col) * (width + kGap),
                                       kMargin + static_cast<float>(row) * std::max(disp.y * 0.46f, 120.f)),
                                ImGuiCond_FirstUseEver);
        if (g_snapBack && g_homeOk[s]) ImGui::SetNextWindowPos(g_home[s], ImGuiCond_Always);
        if (g_closing && g_homeOk[s]) {
            static float const DX[8] = { 1.f, 0.7071f, 0.f, -0.7071f, -1.f, -0.7071f, 0.f, 0.7071f };
            static float const DY[8] = { 0.f, 0.7071f, 1.f, 0.7071f, 0.f, -0.7071f, -1.f, -0.7071f };
            float e = std::clamp(g_closeT / kCloseSecs, 0.f, 1.f);
            float dist = std::max(disp.x, disp.y) * 1.6f * e * e;
            int d = scatterDir(g_home[s].x + width * 0.5f - disp.x * 0.5f, g_home[s].y + 90.f - disp.y * 0.5f, slot);
            ImGui::SetNextWindowPos(ImVec2(g_home[s].x + DX[d] * dist, g_home[s].y + DY[d] * dist), ImGuiCond_Always);
        }

        static float need[16] = {};
        float maxH = std::max(120.f, disp.y * 0.88f);
        ImGui::SetNextWindowSize(ImVec2(width, need[slot & 15] > maxH ? maxH : 0.f), ImGuiCond_Always);

        bool visible = ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoResize);
        ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        g_rectsBuild.push_back({ wp.x, wp.y, wp.x + ws.x, wp.y + ws.y });
        if (!g_closing) { g_home[s] = wp; g_homeOk[s] = true; }

        {
            float th = ImGui::GetFrameHeight();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->PushClipRect(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), false);
            dl->AddRectFilled(ImVec2(wp.x + 1.f, wp.y + th - 1.f), ImVec2(wp.x + ws.x - 1.f, wp.y + th + 1.f), U(g_accent, 0.9f));
            dl->PopClipRect();
        }

        if (visible) {
            body();
            dragScroll();
#ifdef CKY_HAS_IMGUI_INTERNAL
            ImGuiWindow* w = ImGui::GetCurrentWindow();
            need[slot & 15] = w->ContentSize.y + w->TitleBarHeight + ImGui::GetStyle().WindowPadding.y * 2.f;
#endif
        }
        ImGui::End();
    }

    void updateIcon(ImVec2 disp) {
        g_icon.valid = false;
        auto scene = CCDirector::get()->getRunningScene();
        if (!scene) return;
        auto ic = scene->getChildByIDRecursive("cky-icon");
        if (!ic || !ic->getParent() || !ic->isVisible()) return;
        CCSize win = CCDirector::get()->getWinSize();
        if (win.width <= 0.f || win.height <= 0.f) return;
        CCNode* par = ic->getParent();
        CCPoint w0 = par->convertToWorldSpace(ic->getPosition());
        CCPoint w1 = par->convertToWorldSpace(CCPoint(ic->getPositionX() + 22.f * ic->getScale(), ic->getPositionY()));
        float sx = disp.x / win.width, sy = disp.y / win.height;
        g_icon.valid = true;
        g_icon.cx = w0.x * sx;
        g_icon.cy = (win.height - w0.y) * sy;
        g_icon.r = std::max(8.f, std::fabs(w1.x - w0.x) * sx);
    }

    void drawIconMarker() {
        if (!g_icon.valid) return;
        bool covered = false;
        for (auto const& r : g_rects) {
            float nx = std::clamp(g_icon.cx, r.x0, r.x1), ny = std::clamp(g_icon.cy, r.y0, r.y1);
            float dx = g_icon.cx - nx, dy = g_icon.cy - ny;
            if (dx * dx + dy * dy < g_icon.r * g_icon.r) { covered = true; break; }
        }
        if (!covered) return;
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        ImVec2 c(g_icon.cx, g_icon.cy);
        fg->AddCircleFilled(c, g_icon.r, U(0.05f, 0.05f, 0.07f, 0.96f), 32);
        fg->AddCircle(c, g_icon.r, U(g_accent, 1.f), 32, 2.5f);
        ImFont* font = ImGui::GetFont();
        float fs = g_icon.r * 0.75f;
        ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.f, "CKY");
        fg->AddText(font, fs, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), U(1.f, 1.f, 1.f, 1.f), "CKY");
    }

    void drawMenu() {
        touchBridgeRegister();
        dragScrollFrame();
        drawFrameWindow();
        g_rectsBuild.clear();
        if (!cky::imguiOpen) {
            g_rects.clear();
            g_anyPopup = false;
            g_icon.valid = false;
            return;
        }

        ImVec2 disp = ImGui::GetIO().DisplaySize;
        updateIcon(disp);
        g_startX = 10.f;
        if (g_icon.valid && g_icon.cx < disp.x * 0.3f && g_icon.cy < disp.y * 0.4f)
            g_startX = g_icon.cx + g_icon.r + 12.f;

        float width = std::clamp(disp.x * 0.16f, 290.f, 380.f);
        width = std::min(width, std::max(200.f, disp.x - g_startX - 10.f));

        mhWindow("Bot",              0, width, disp, winBot);
        mhWindow("Global",           1, width, disp, winGlobal);
        mhWindow("Player",           2, width, disp, winPlayer);
        mhWindow("Display",          3, width, disp, winDisplay);
        mhWindow("Labels",           4, width, disp, winLabels);
        mhWindow("Clicks",           5, width, disp, winClicks);
        mhWindow("Rendering (Beta)", 6, width, disp, winRendering);
        mhWindow("Settings",         7, width, disp, winSettings);
        g_snapBack = false;

        g_anyPopup = !g_closing && ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
        g_rects = g_rectsBuild;
        drawIconMarker();

        if (g_touchDbg) {
            ImGuiIO& io = ImGui::GetIO();
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            fg->AddCircle(io.MousePos, 14.f, U(1.f, 1.f, 0.f, 1.f), 16, 2.f);
            char buf[160];
            std::snprintf(buf, sizeof buf, "mouse %.0f,%.0f down=%d wantMouse=%d windows=%d closing=%d",
                          io.MousePos.x, io.MousePos.y, io.MouseDown[0] ? 1 : 0, io.WantCaptureMouse ? 1 : 0,
                          static_cast<int>(g_rects.size()), g_closing ? 1 : 0);
            fg->AddText(ImVec2(disp.x * 0.5f - 200.f, disp.y - 30.f), U(1.f, 1.f, 0.f, 1.f), buf);
        }

        if (g_closing) {
            g_closeT += std::min(ImGui::GetIO().DeltaTime, 0.05f);
            if (g_closeT >= kCloseSecs) {
                g_closing = false;
                g_snapBack = true;
                cky::imguiOpen = false;
                g_rects.clear();
            }
        }

        cky::settingsSaveIfChanged();
        ckyHudRefresh();
    }
}

class $modify(CkyTouchDispatcher, CCTouchDispatcher) {
    void touches(CCSet* touches, CCEvent* event, unsigned int type) {
        if (hookFeed(touches, type)) return;
        CCTouchDispatcher::touches(touches, event, type);
    }
};

$on_mod(Loaded) {
    if (!wantsImGui()) return;
    cky::theme = Mod::get()->getSavedValue<int>("theme", 0);
    ImGuiCocos::get().setup(styleSetup).draw(drawMenu);
    g_ready = true;
}

bool ckyUiIsImGui() { return g_ready; }
void ckyImGuiToggle() {
    if (g_closing) return;
    if (cky::imguiOpen) startClose();
    else cky::imguiOpen = true;
}

#else

bool ckyUiIsImGui() { return false; }
void ckyImGuiToggle() {}

#endif
