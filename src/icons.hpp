#pragma once
#include <Geode/Geode.hpp>

namespace cky {
    inline cocos2d::CCSpriteFrame* getIcon(const char* name) {
        return cocos2d::CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(name);
    }
}
