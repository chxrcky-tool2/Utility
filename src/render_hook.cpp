// Rendering (beta): grab one video frame after every drawScene while a render is running.
#include "render/ChxrRender.hpp"
#include <Geode/modify/CCDirector.hpp>

using namespace geode::prelude;

class $modify(CkyRenderDirector, CCDirector) {
    void drawScene() {
        CCDirector::drawScene();
        chxr::Renderer::get().captureFrame();
    }
};
