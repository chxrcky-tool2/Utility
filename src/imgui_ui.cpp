// imgui_ui.cpp
#include "state.hpp"

#if __has_include(<imgui-cocos.hpp>)

#include <imgui-cocos.hpp>
#include <Geode/utils/web.hpp>
#include <cstdlib>

namespace {
    bool g_ready = false;

    char const* kThemeNames[5] = { "Neon", "Ocean", "Forest", "Sunset", "Mono" };
    ImVec4 const kThemeHi[5] = {
        ImVec4(0.93f, 0.30f, 0.50f, 1.f), ImVec4(0.24f, 0.62f, 1.00f, 1.f),
        ImVec4(0.30f, 0.76f, 0.40f, 1.f), ImVec4(1.00f, 0.62f, 0.22f, 1.f),
        ImVec4(0.72f, 0.72f, 0.76f, 1.f)
    };
    ImVec4 const kThemeLo[5] = {
        ImVec4(0.62f, 0.14f, 0.30f, 1.f), ImVec4(0.10f, 0.28f, 0.62f, 1.f),
        ImVec4(0.10f, 0.42f, 0.26f, 1.f), ImVec4(0.72f, 0.20f, 0.34f, 1.f),
        ImVec4(0.30f, 0.30f, 0.33f, 1.f)
    };

    bool wantsImGui() {
        return Mod::get()->getSettingValue<std::string>("ui-mode") == "ImGui";
    }

    void applyColors(int theme) {
        int t = std::clamp(theme, 0, 4);
        ImVec4 pink = kThemeHi[t];
        ImVec4 pinkLo = kThemeLo[t];
        ImVec4 pinkHi(std::min(pink.x + 0.12f, 1.f), std::min(pink.y + 0.12f, 1.f),
                      std::min(pink.z + 0.12f, 1.f), 1.f);
        auto& c = ImGui::GetStyle().Colors;
        c[ImGuiCol_WindowBg]       = ImVec4(0.07f, 0.07f, 0.10f, 0.97f);
        c[ImGuiCol_Border]         = pink;
        c[ImGuiCol_TitleBg]        = pinkLo;
        c[ImGuiCol_TitleBgActive]  = pinkLo;
        c[ImGuiCol_FrameBg]        = ImVec4(0.15f, 0.15f, 0.21f, 1.f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.30f, 1.f);
        c[ImGuiCol_FrameBgActive]  = ImVec4(0.28f, 0.28f, 0.38f, 1.f);
        c[ImGuiCol_CheckMark]      = pinkHi;
        c[ImGuiCol_SliderGrab]     = pink;
        c[ImGuiCol_SliderGrabActive] = pinkHi;
        c[ImGuiCol_Button]         = pinkLo;
        c[ImGuiCol_ButtonHovered]  = pink;
        c[ImGuiCol_ButtonActive]   = pinkHi;
        c[ImGuiCol_Header]         = pinkLo;
        c[ImGuiCol_HeaderHovered]  = pink;
        c[ImGuiCol_HeaderActive]   = pinkHi;
        c[ImGuiCol_Tab]            = ImVec4(0.14f, 0.14f, 0.19f, 1.f);
        c[ImGuiCol_TabHovered]     = pink;
        c[ImGuiCol_TabActive]      = pinkLo;
    }

    void styleSetup() {
        ImGui::GetStyle() = ImGuiStyle();
        ImGui::StyleColorsDark();
        auto& s = ImGui::GetStyle();
        s.ScaleAllSizes(1.4f);
        s.WindowRounding = 14.f;
        s.FrameRounding = 9.f;
        s.GrabRounding = 9.f;
        s.TabRounding = 9.f;
        s.ChildRounding = 9.f;
        s.FramePadding = ImVec2(14.f, 10.f);
        s.ItemSpacing = ImVec2(12.f, 12.f);
        s.GrabMinSize = 30.f;
        s.ScrollbarSize = 26.f;
        s.WindowBorderSize = 2.f;
        ImGui::GetIO().FontGlobalScale = 1.4f;
        applyColors(cky::theme);
    }

    bool minusPlus(char const* id, float& v, float delta, float lo, float hi) {
        bool changed = false;
        ImGui::PushID(id);
        ImGui::SameLine();
        if (ImGui::Button(" - ")) { v = std::clamp(v - delta, lo, hi); changed = true; }
        ImGui::SameLine();
        if (ImGui::Button(" + ")) { v = std::clamp(v + delta, lo, hi); changed = true; }
        ImGui::PopID();
        return changed;
    }

    void tabBot() {
        bool rec = cky::recording;
        if (ImGui::Checkbox("Record", &rec)) {
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
        if (ImGui::Checkbox("Playback", &play)) {
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

        bool fz = cky::frozen;
        if (ImGui::Checkbox("Frame stepper", &fz)) {
            cky::frozen = fz;
            cky::pending = 0;
            ckyApplyAudio();
        }

        ImGui::Separator();
        ImGui::SliderInt("Macro slot", &cky::slot, 1, cky::MAX_SLOTS);

        if (ImGui::Button("Save macro")) {
            bool ok = cky::save();
            ckyNotify(ok ? fmt::format("Saved to slot {}", cky::slot).c_str() : "Save failed", ok);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load macro")) {
            bool ok = cky::load();
            ckyNotify(ok ? fmt::format("Loaded slot {}", cky::slot).c_str() : "Load failed", ok);
        }
        if (ImGui::Button("Clear macro")) {
            cky::clearMacro();
            ckyNotify("Macro buffer cleared");
        }
        ImGui::SameLine();
        if (ImGui::Button("Import GDR file")) ckyImportGdr();

        ImGui::SliderInt("Input offset", &cky::frameOffset, -10, 10);

        ImGui::Separator();
        ImGui::Text("Inputs: %llu", static_cast<unsigned long long>(cky::macro.size()));
        ImGui::Text("Macro TPS: %d", static_cast<int>(cky::macroTps));
        ImGui::Text("Slot %d: %s", cky::slot, cky::slotExists() ? "saved" : "empty");
    }

    void tabGlobal() {
        bool tpsOn = cky::tpsOn;
        if (ImGui::Checkbox("TPS bypass", &tpsOn)) {
            if (cky::recording) ckyNotify("Stop recording first", false);
            else { cky::tpsOn = tpsOn; cky::resetMeasure(); }
        }

        float t = cky::tps;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (ImGui::SliderFloat("##tps", &t, cky::TPS_MIN, cky::TPS_MAX, "TPS %.0f")) {
            if (cky::recording) ckyNotify("Stop recording first", false);
            else { cky::tps = std::round(t); cky::tpsOn = true; cky::resetMeasure(); }
        }
        float t2 = cky::tps;
        if (minusPlus("tps", t2, cky::tpsStep, cky::TPS_MIN, cky::TPS_MAX)) {
            if (cky::recording) ckyNotify("Stop recording first", false);
            else { cky::tps = t2; cky::tpsOn = true; cky::resetMeasure(); }
        }

        if (ImGui::Button(fmt::format("Step size: {}", static_cast<int>(cky::tpsStep)).c_str())) {
            float n = cky::nextVal(cky::tpsStepList, cky::tpsStep, 1);
            if (n == cky::tpsStep) n = cky::tpsStepList.front();
            cky::tpsStep = n;
        }
        ImGui::SameLine();
        std::string hi = (cky::tpsOn && cky::tps >= 480.f)
            ? fmt::format("High TPS: {}", static_cast<int>(cky::tps))
            : std::string("High TPS: OFF");
        if (ImGui::Button(hi.c_str())) {
            if (cky::recording) ckyNotify("Stop recording first", false);
            else {
                cky::tps = cky::nextHighTps(cky::tpsOn ? cky::tps : 240.f);
                cky::tpsOn = true;
                cky::resetMeasure();
            }
        }
        if (cky::actualTps > 0.f) {
            ImGui::TextDisabled("Real TPS: %d", static_cast<int>(cky::actualTps + 0.5f));
        }

        ImGui::Separator();
        bool spOn = cky::speedOn;
        if (ImGui::Checkbox("Speedhack", &spOn)) {
            cky::speedOn = spOn;
            ckyApplyAudio();
        }
        float sp = cky::speed;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (ImGui::SliderFloat("##speed", &sp, 0.01f, 4.f, "%.2fx")) {
            cky::speed = std::clamp(std::round(sp * 100.f) / 100.f, 0.01f, 4.f);
            cky::speedOn = true;
            ckyApplyAudio();
        }
        float sp2 = cky::speed;
        if (minusPlus("speed", sp2, 0.01f, 0.01f, 4.f)) {
            cky::speed = std::round(sp2 * 100.f) / 100.f;
            cky::speedOn = true;
            ckyApplyAudio();
        }
        if (ImGui::Button("Slower")) {
            cky::speed = cky::nextVal(cky::speedList, cky::speed, -1);
            cky::speedOn = true;
            ckyApplyAudio();
        }
        ImGui::SameLine();
        if (ImGui::Button("Faster")) {
            cky::speed = cky::nextVal(cky::speedList, cky::speed, 1);
            cky::speedOn = true;
            ckyApplyAudio();
        }

        bool au = cky::audioSpeed;
        if (ImGui::Checkbox("Audio follows speed", &au)) {
            cky::audioSpeed = au;
            ckyApplyAudio();
        }
}
    void tabPlayer() {
        bool nc = cky::noclip;
        if (ImGui::Checkbox("Noclip", &nc)) {
            cky::noclip = nc;
            cky::ncReset();
        }
        ImGui::Checkbox("Safe mode", &cky::safeMode);
        ImGui::Checkbox("Show accuracy", &cky::ncShowAcc);
        ImGui::Checkbox("Show deaths", &cky::ncShowDeaths);
    }

    void tabDisplay() {
        bool hb = cky::hitboxes;
        if (ImGui::Checkbox("Hitboxes", &hb)) {
            cky::hitboxes = hb;
            ckyApplyHitboxes();
        }
        bool hud = cky::hudOn;
        if (ImGui::Checkbox("HUD", &hud)) {
            cky::hudOn = hud;
            if (auto h = ckyHudNode()) h->setVisible(cky::hudOn);
        }
        float hs = cky::hudScale;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (ImGui::SliderFloat("HUD size", &hs, 0.5f, 2.f, "%.1fx")) {
            cky::hudScale = hs;
            ckyApplyHudScale();
        }
        bool sp = cky::startPosOn;
        if (ImGui::Checkbox("StartPos switcher", &sp)) {
            cky::startPosOn = sp;
            ckySetStartPosVisible(cky::startPosOn);
        }
        ImGui::Checkbox("Input overlay", &cky::overlayOn);
        ImGui::Checkbox("Click telemetry", &cky::clickHudOn);
    }

    void tabClicks() {
        bool cb = cky::clickbotOn;
        if (ImGui::Checkbox("Clickbot sounds", &cb)) {
            cky::clickbotOn = cb;
            ckyClickRescan();
        }
        if (ImGui::Button(" < ")) ckyClickPackStep(-1);
        ImGui::SameLine();
        ImGui::Text("Pack: %s", ckyClickPackName().c_str());
        ImGui::SameLine();
        if (ImGui::Button(" > ")) ckyClickPackStep(1);

        int vol = cky::clickVol;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (ImGui::SliderInt("Volume", &vol, 0, 200, "%d%%")) {
            cky::clickVol = vol;
            Mod::get()->setSavedValue<int>("click-vol", cky::clickVol);
        }
        ImGui::TextDisabled("Packs go in game/clicks/<PackName>/");
        ImGui::TextDisabled("with player1/clicks, releases, softclicks...");
    }

    void tabSettings() {
        float is = cky::iconScale;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (ImGui::SliderFloat("Cky icon size", &is, 0.5f, 2.f, "%.1fx")) {
            cky::iconScale = is;
            if (auto scene = CCDirector::get()->getRunningScene()) {
                if (auto ic = scene->getChildByIDRecursive("cky-icon")) ic->setScale(cky::iconScale);
            }
        }
        if (ImGui::Button("Reset icon position")) {
            cky::savePos("cky-icon", CCPoint(-99999.f, -99999.f));
            if (auto scene = CCDirector::get()->getRunningScene()) {
                if (auto ic = scene->getChildByIDRecursive("cky-icon")) ic->setPosition(ckyDefaultIconPos());
            }
        }
        ImGui::Checkbox("Checkpoint telemetry", &cky::ckptTelemetry);

        int th = std::clamp(cky::theme, 0, 4);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (ImGui::SliderInt("Theme", &th, 0, 4, kThemeNames[th])) {
            cky::theme = th;
            Mod::get()->setSavedValue<int>("theme", cky::theme);
            applyColors(th);
        }

        ImGui::Separator();
        ImGui::Text("ChxrCky Macro Bot v1.0.4");
        ImGui::TextDisabled("(c) 2026 ChxrCky - GNU GPL v3.0");
        ImGui::TextDisabled("Free software, NO WARRANTY.");
        if (ImGui::Button("Open GPL-3.0 license")) {
            web::openLinkInBrowser("https://www.gnu.org/licenses/gpl-3.0.html");
        }
    }

    void drawMenu() {
        if (!cky::imguiOpen) return;

        ImVec2 disp = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(
            ImVec2(std::min(disp.x * 0.85f, 1100.f), std::min(disp.y * 0.85f, 700.f)),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(disp.x * 0.5f, disp.y * 0.5f),
            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

        bool open = true;
        if (ImGui::Begin("ChxrCky Macro Bot", &open)) {
            if (ImGui::BeginTabBar("cky-tabs")) {
                if (ImGui::BeginTabItem("Bot"))      { tabBot();      ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Global"))   { tabGlobal();   ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Player"))   { tabPlayer();   ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Display"))  { tabDisplay();  ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Clicks"))   { tabClicks();   ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Settings")) { tabSettings(); ImGui::EndTabItem(); }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
        ckyHudRefresh();
        if (!open) cky::imguiOpen = false;
    }
}

$on_mod(Loaded) {
    if (!wantsImGui()) return;
    cky::theme = Mod::get()->getSavedValue<int>("theme", 0);
    ImGuiCocos::get().setup(styleSetup).draw(drawMenu);
    g_ready = true;
}

bool ckyUiIsImGui() { return g_ready; }
void ckyImGuiToggle() { cky::imguiOpen = !cky::imguiOpen; }

#else

bool ckyUiIsImGui() { return false; }
void ckyImGuiToggle() {}

#endif

// END OF FILE (imgui_ui.cpp)
