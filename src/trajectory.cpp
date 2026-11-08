// trajectory.cpp ChxrCky menu
#include "state.hpp"
#include "traj_fields.hpp"

#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

#include <unordered_set>

namespace {
    // portals the fake player needs to react to (size / speed); gravity+mode portals are ignored
    std::unordered_set<int> const kPortalIDs = { 101, 99, 11, 10, 200, 201, 202, 203, 1334 };
    // object types that may collide with the fake player: 0 solid, 2 hazard, 47, 25
    std::unordered_set<int> const kSolidTypes = { 0, 2, 47, 25 };

    struct Traj {
        Ref<PlayerObject> fake1, fake2;
        Ref<CCDrawNode>   node;
        bool  cancel = false;
        float deathRot = 0.f;
        float delta = 1.f / 240.f;
        ccColor4F holdColor{ 0.30f, 1.00f, 0.40f, 1.f };
        ccColor4F freeColor{ 1.00f, 0.35f, 0.35f, 1.f };
        ccColor4F sameColor{ 0.95f, 0.95f, 0.60f, 1.f };
        CCPoint p1Hold[512];
        CCPoint p2Hold[512];
        bool loggedCopy = false;
        bool hasKiller = false;      // a fake player died on an object (Frame Window Calculator reads it)
        CCPoint killer{ 0.f, 0.f };  // top-centre of that object, object-layer coordinates
    };
    Traj g_t;

    // copies every listed member that exists in this SDK; returns names that were skipped
    std::string copyState(PlayerObject* dst, PlayerObject* src, int* okCount) {
        std::string skipped;
        int ok = 0;
        auto copyAll = [&](auto* d, auto* s) {
#define CKY_COPY_ONE(f) \
            if constexpr (requires { d->f = s->f; }) { d->f = s->f; ++ok; } \
            else { skipped += #f; skipped += ' '; }
            CKY_TRAJ_FIELDS(CKY_COPY_ONE)
#undef CKY_COPY_ONE
        };
        copyAll(dst, src);
        if (okCount) *okCount = ok;
        return skipped;
    }

    void handlePortal(PlayerObject* p, int id) {
        if (!kPortalIDs.contains(id)) return;
        switch (id) {
        case 101: p->togglePlayerScale(true, true);  p->updatePlayerScale(); break;
        case 99:  p->togglePlayerScale(false, true); p->updatePlayerScale(); break;
        case 200: p->m_playerSpeed = 0.7f; break;
        case 201: p->m_playerSpeed = 0.9f; break;
        case 202: p->m_playerSpeed = 1.1f; break;
        case 203: p->m_playerSpeed = 1.3f; break;
        case 1334: p->m_playerSpeed = 1.6f; break;
        default: break;
        }
    }

    std::vector<CCPoint> hitboxVertices(PlayerObject* player, CCRect rect, float rotation) {
        std::vector<CCPoint> v = {
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

    void drawDeathHitbox(PlayerObject* player) {
        CCRect big = player->GameObject::getObjectRect();
        CCRect small = player->GameObject::getObjectRect(0.3f, 0.3f);
        auto v = hitboxVertices(player, big, g_t.deathRot);
        g_t.node->drawPolygon(v.data(), 4, ccc4f(g_t.freeColor.r, g_t.freeColor.g, g_t.freeColor.b, 0.2f),
                              0.5f, g_t.freeColor);
        v = hitboxVertices(player, small, g_t.deathRot);
        g_t.node->drawPolygon(v.data(), 4, ccc4f(g_t.sameColor.r, g_t.sameColor.g, g_t.sameColor.b, 0.2f),
                              0.35f, ccc4f(g_t.sameColor.r, g_t.sameColor.g, g_t.sameColor.b, 0.55f));
    }

    void simulate(PlayLayer* pl, PlayerObject* fake, PlayerObject* real, bool hold) {
        bool isP2 = pl->m_player2 == real;
        int steps = std::clamp(cky::trajLen, 60, 480);

        int ok = 0;
        std::string skipped = copyState(fake, real, &ok);
        if (!g_t.loggedCopy) {
            g_t.loggedCopy = true;
            log::info("[cky] trajectory: copied {} PlayerObject members, skipped {}", ok,
                skipped.empty() ? std::string("none") : skipped);
        }
        fake->setPosition(real->getPosition());
        fake->setRotation(real->getRotation());
        fake->m_isDead = false;

        g_t.cancel = false;

        for (int i = 0; i < steps; ++i) {
            CCPoint prev = fake->getPosition();
            if (hold) (isP2 ? g_t.p2Hold : g_t.p1Hold)[i] = prev;

            if (fake->m_collisionLogTop)    fake->m_collisionLogTop->removeAllObjects();
            if (fake->m_collisionLogBottom) fake->m_collisionLogBottom->removeAllObjects();
            if (fake->m_collisionLogLeft)   fake->m_collisionLogLeft->removeAllObjects();
            if (fake->m_collisionLogRight)  fake->m_collisionLogRight->removeAllObjects();

            pl->checkCollisions(fake, g_t.delta, false);

            if (g_t.cancel) {
                fake->updatePlayerScale();
                drawDeathHitbox(fake);
                break;
            }

            if (i == 0) {
                if (hold) fake->pushButton(static_cast<PlayerButton>(1));
                else      fake->releaseButton(static_cast<PlayerButton>(1));
                if (pl->m_levelSettings->m_platformerMode) {
                    // keep moving in the direction the real player is going
                    fake->pushButton(static_cast<PlayerButton>(real->m_isGoingLeft ? 2 : 3));
                }
            }

            fake->update(g_t.delta);
            fake->updateRotation(g_t.delta);
            fake->updatePlayerScale();

            ccColor4F color = hold ? g_t.holdColor : g_t.freeColor;
            if (!hold) {
                CCPoint ref = (isP2 ? g_t.p2Hold : g_t.p1Hold)[i];
                if (ref == prev) color = g_t.sameColor; // both lines overlap here
            }
            if (i >= steps - 40) color.a = static_cast<float>(steps - i) / 40.f;

            g_t.node->drawSegment(prev, fake->getPosition(), 0.6f, color);
        }
    }

    void trajUpdate(PlayLayer* pl) {
        if (!g_t.fake1 || !g_t.fake2 || !g_t.node) return;
        if (!pl->m_player1 || pl->m_player1->m_isDead) { g_t.node->clear(); return; }

        cky::trajCreating = true;
        g_t.node->setVisible(true);
        g_t.node->clear();

        simulate(pl, g_t.fake1, pl->m_player1, true);
        simulate(pl, g_t.fake2, pl->m_player1, false);

        if (pl->m_gameState.m_isDualMode && pl->m_player2) {
            simulate(pl, g_t.fake2, pl->m_player2, true);
            simulate(pl, g_t.fake1, pl->m_player2, false);
        }
        cky::trajCreating = false;
    }
}

bool ckyTrajHandleDeath(PlayerObject* player, GameObject* object) {
    if (cky::trajCreating || (player && (player == g_t.fake1.data() || player == g_t.fake2.data()))) {
        if (player) g_t.deathRot = player->getRotation();
        if (object) {
            CCRect r = object->getObjectRect();
            g_t.killer = ccp(r.getMidX(), r.getMaxY());
            g_t.hasKiller = true;
        }
        g_t.cancel = true;
        return true;
    }
    return false;
}

float ckyTrajStepDelta() { return g_t.delta; }
void ckyTrajResetCancel() { g_t.cancel = false; g_t.hasKiller = false; }
bool ckyTrajCancelled() { return g_t.cancel; }
bool ckyTrajKiller(CCPoint* out) {
    if (!g_t.hasKiller) return false;
    if (out) *out = g_t.killer;
    return true;
}

void ckyTrajClear() {
    if (g_t.node) {
        g_t.node->clear();
        g_t.node->setVisible(false);
    }
}

class $modify(CkyTrajPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        g_t.cancel = false;
        cky::trajCreating = false;
        g_t.loggedCopy = false;

        g_t.fake1 = PlayerObject::create(1, 1, this, this, true);
        g_t.fake2 = PlayerObject::create(1, 1, this, this, true);
        for (auto f : { g_t.fake1.data(), g_t.fake2.data() }) {
            if (!f) continue;
            f->setPosition({ 0.f, 105.f });
            f->setVisible(false);
            m_objectLayer->addChild(f);
        }

        g_t.node = CCDrawNode::create();
        ccBlendFunc bf;
        bf.src = GL_SRC_ALPHA;
        bf.dst = GL_ONE_MINUS_SRC_ALPHA;
        g_t.node->setBlendFunc(bf);
        g_t.node->setVisible(cky::trajectory);
        m_objectLayer->addChild(g_t.node, 500);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!cky::trajectory || cky::trajCreating) return;
        trajUpdate(this);
    }

    void onQuit() {
        ckyTrajClear();
        g_t.fake1 = nullptr;
        g_t.fake2 = nullptr;
        g_t.node = nullptr;
        g_t.cancel = false;
        cky::trajCreating = false;
        PlayLayer::onQuit();
    }

    void playEndAnimationToPos(CCPoint p0) {
        if (!cky::trajCreating) PlayLayer::playEndAnimationToPos(p0);
    }
};

class $modify(CkyTrajGameLayer, GJBaseGameLayer) {
    // while simulating, only solids, hazards and size/speed portals can touch the fake player
    void collisionCheckObjects(PlayerObject* p0, gd::vector<GameObject*>* objects, int p2, float p3) {
        if (!cky::trajCreating) return GJBaseGameLayer::collisionCheckObjects(p0, objects, p2, p3);

        std::vector<GameObject*> disabled;
        for (auto obj : *objects) {
            if (!obj) continue;
            if (!kSolidTypes.contains(static_cast<int>(obj->m_objectType)) && !kPortalIDs.contains(obj->m_objectID)) {
                if (obj->m_isDisabled || obj->m_isDisabled2) continue;
                disabled.push_back(obj);
                obj->m_isDisabled = true;
                obj->m_isDisabled2 = true;
            }
        }
        GJBaseGameLayer::collisionCheckObjects(p0, objects, p2, p3);
        for (auto obj : disabled) {
            obj->m_isDisabled = false;
            obj->m_isDisabled2 = false;
        }
    }

    bool canBeActivatedByPlayer(PlayerObject* p0, EffectGameObject* p1) {
        if (cky::trajCreating) {
            handlePortal(p0, p1->m_objectID);
            return false;
        }
        return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
    }

    void playerTouchedRing(PlayerObject* p0, RingObject* p1) {
        if (!cky::trajCreating) GJBaseGameLayer::playerTouchedRing(p0, p1);
    }

    void playerTouchedTrigger(PlayerObject* p0, EffectGameObject* p1) {
        if (!cky::trajCreating) GJBaseGameLayer::playerTouchedTrigger(p0, p1);
        else handlePortal(p0, p1->m_objectID);
    }
};

class $modify(CkyTrajPlayer, PlayerObject) {
    void update(float dt) {
        PlayerObject::update(dt);
        g_t.delta = dt;
    }

    void incrementJumps() {
        if (!cky::trajCreating) PlayerObject::incrementJumps();
    }

    void ringJump(RingObject* p0, bool p1) {
        if (!cky::trajCreating) PlayerObject::ringJump(p0, p1);
    }

    void playSpiderDashEffect(CCPoint p0, CCPoint p1) {
        if (!cky::trajCreating) PlayerObject::playSpiderDashEffect(p0, p1);
    }
};

// END OF FILE (trajectory.cpp)
