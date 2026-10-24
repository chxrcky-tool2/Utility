#include "icons.hpp"
#include <Geode/modify/PlayerObject.hpp>

class $modify(CkyIconHook, PlayerObject) {
    void update(float dt) {
        PlayerObject::update(dt);
    }
};
