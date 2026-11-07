// hitboxtrail.cpp — hitbox trail: the player hitboxes left behind at every physics step.
//
// How it works:
//  - Once per real physics step (ckyTrailCapture, called from processCommands) the outer and
//    inner hitbox polygons of each player are recorded.
//  - Records are grouped in chunks of kChunk steps. Each chunk owns ONE CCDrawNode that is drawn
//    once, incrementally, so a long trail costs nothing per frame (no full redraw).
//  - The oldest chunk is dropped when the trail is longer than the length setting.
//  - Back step / checkpoint rewinds drop every record at or after the step we went back to.
//  - Colors are cky::trailOuter / cky::trailInner (RGBA floats), editable in both menus.
#include "state.hpp"

#include <Geode/modify/PlayLayer.hpp>
#include <array>

namespace {
    constexpr size_t kChunk = 20;          // steps per draw node
    constexpr int    kMinLen = 20;
    constexpr int    kMaxLen = 1200;

    using Quad = std::array<CCPoint, 4>;

    struct Rec {
        uint32_t step = 0;
        int      count = 0;                // players recorded (1, or 2 in dual mode)
        Quad     outer[2];
        Quad     inner[2];
    };

    struct Chunk {
        Ref<CCDrawNode> node;
        std::vector<Rec> recs;
    };

    struct Trail {
        Ref<CCNode> root;
        std::deque<Chunk> chunks;
    };
    Trail g_tr;

    // same hitbox shape the trajectory's death box uses (size-portal and dart corrections)
    Quad quadFor(PlayerObject* player, CCRect rect, float rotation) {
        Quad v = {
            ccp(rect.getMinX(), rect.getMaxY()), ccp(rect.getMaxX(), rect.getMaxY()),
            ccp(rect.getMaxX(), rect.getMinY()), ccp(rect.getMinX(), rect.getMinY())
        };
        CCPoint c = ccp((rect.getMinX() + rect.getMaxX()) / 2.f, (rect.getMinY() + rect.getMaxY()) / 2.f);
        float size = static_cast<float>(static_cast<int>(rect.getMaxX() - rect.getMinX()));

        auto scaleAround = [&](float k) {
            for (auto& p : v) { p.x = c.x + (p.x - c.x) * k; p.y = c.y + (p.y - c.y) * k; }
        };
        if ((size == 18 || size == 5) && player->getScale() == 1) scaleAround(1.f / 0.6f);
        if ((size == 7 || size == 30 || size == 29 || size == 9) && player->getScale() != 1) scaleAround(0.6f);
        if (player->m_isDart) scaleAround(0.3f);

        float a = CC_DEGREES_TO_RADIANS(-rotation);
        for (auto& p : v) {
            float x = p.x - c.x, y = p.y - c.y;
            p.x = c.x + x * std::cos(a) - y * std::sin(a);
            p.y = c.y + x * std::sin(a) + y * std::cos(a);
        }
        return v;
    }

    ccColor4F col(float const* c, float aMul) {
        return ccc4f(c[0], c[1], c[2], std::clamp(c[3] * aMul, 0.f, 1.f));
    }

    void drawRec(CCDrawNode* n, Rec& r) {
        for (int i = 0; i < r.count; ++i) {
            n->drawPolygon(r.outer[i].data(), 4, col(cky::trailOuter, 0.12f), 0.5f, col(cky::trailOuter, 1.f));
            n->drawPolygon(r.inner[i].data(), 4, col(cky::trailInner, 0.12f), 0.4f, col(cky::trailInner, 1.f));
        }
    }

    void redrawChunk(Chunk& c) {
        if (!c.node) return;
        c.node->clear();
        for (auto& r : c.recs) drawRec(c.node, r);
    }

    Chunk makeChunk() {
        Chunk c;
        c.node = CCDrawNode::create();
        ccBlendFunc bf;
        bf.src = GL_SRC_ALPHA;
        bf.dst = GL_ONE_MINUS_SRC_ALPHA;
        c.node->setBlendFunc(bf);
        if (g_tr.root) g_tr.root->addChild(c.node);
        c.recs.reserve(kChunk);
        return c;
    }
}

void ckyTrailReset() {
    if (g_tr.root) g_tr.root->removeAllChildren();
    g_tr.chunks.clear();
}

// drop every record at or after `step` (back step, checkpoint, any rewind)
void ckyTrailRewind(uint32_t step) {
    while (!g_tr.chunks.empty()) {
        auto& c = g_tr.chunks.back();
        size_t before = c.recs.size();
        while (!c.recs.empty() && c.recs.back().step >= step) c.recs.pop_back();
        if (c.recs.empty()) {
            if (c.node) c.node->removeFromParent();
            g_tr.chunks.pop_back();
            continue;
        }
        if (c.recs.size() != before) redrawChunk(c);
        break;
    }
}

void ckyTrailCapture(GJBaseGameLayer* l) {
    if (!cky::hitboxTrail || !l || !g_tr.root) return;
    if (cky::trajCreating || !l->m_player1 || l->m_player1->m_isDead) return;
    auto pl = PlayLayer::get();
    if (!pl || static_cast<GJBaseGameLayer*>(pl) != l) return;

    Rec r;
    r.step = cky::step;
    auto add = [&](PlayerObject* p) {
        if (!p || r.count >= 2) return;
        r.outer[r.count] = quadFor(p, p->GameObject::getObjectRect(), p->getRotation());
        r.inner[r.count] = quadFor(p, p->GameObject::getObjectRect(0.3f, 0.3f), p->getRotation());
        ++r.count;
    };
    add(l->m_player1);
    if (l->m_gameState.m_isDualMode) add(l->m_player2);
    if (r.count == 0) return;

    ckyTrailRewind(r.step); // the step counter went backwards: forget the future

    if (g_tr.chunks.empty() || g_tr.chunks.back().recs.size() >= kChunk) {
        g_tr.chunks.push_back(makeChunk());
    }
    auto& c = g_tr.chunks.back();
    c.recs.push_back(r);
    drawRec(c.node, c.recs.back());

    size_t maxChunks = static_cast<size_t>(std::clamp(cky::trailLen, kMinLen, kMaxLen)) / kChunk + 1;
    while (g_tr.chunks.size() > maxChunks) {
        if (g_tr.chunks.front().node) g_tr.chunks.front().node->removeFromParent();
        g_tr.chunks.pop_front();
    }
}

// show/hide after the toggle changed
void ckyTrailApply() {
    if (g_tr.root) g_tr.root->setVisible(cky::hitboxTrail);
    if (!cky::hitboxTrail) ckyTrailReset();
}

// persist settings and re-color what is already on screen
void ckyTrailSave() {
    cky::trailLen = std::clamp(cky::trailLen, kMinLen, kMaxLen);
    for (int i = 0; i < 4; ++i) {
        cky::trailOuter[i] = std::clamp(cky::trailOuter[i], 0.f, 1.f);
        cky::trailInner[i] = std::clamp(cky::trailInner[i], 0.f, 1.f);
        Mod::get()->setSavedValue<float>(fmt::format("trail-outer-{}", i), cky::trailOuter[i]);
        Mod::get()->setSavedValue<float>(fmt::format("trail-inner-{}", i), cky::trailInner[i]);
    }
    Mod::get()->setSavedValue<int>("trail-len", cky::trailLen);
    for (auto& c : g_tr.chunks) redrawChunk(c);
}

void ckyTrailLoad() {
    cky::trailLen = std::clamp(Mod::get()->getSavedValue<int>("trail-len", cky::trailLen), kMinLen, kMaxLen);
    for (int i = 0; i < 4; ++i) {
        cky::trailOuter[i] = Mod::get()->getSavedValue<float>(fmt::format("trail-outer-{}", i), cky::trailOuter[i]);
        cky::trailInner[i] = Mod::get()->getSavedValue<float>(fmt::format("trail-inner-{}", i), cky::trailInner[i]);
    }
}

class $modify(CkyTrailPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        static bool s_loaded = false;
        if (!s_loaded) { s_loaded = true; ckyTrailLoad(); }

        g_tr.chunks.clear();
        g_tr.root = CCNode::create();
        g_tr.root->setVisible(cky::hitboxTrail);
        m_objectLayer->addChild(g_tr.root, 450); // below the trajectory node (500)
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        ckyTrailReset();
    }

    void onQuit() {
        g_tr.chunks.clear();
        g_tr.root = nullptr;
        PlayLayer::onQuit();
    }
};

// END OF FILE (hitboxtrail.cpp)
