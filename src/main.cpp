#include "cky.hpp"
#include <Geode/modify/PlayLayer.hpp>

void ckyBuildPanel(PlayLayer* pl);
void ckyBuildHud(PlayLayer* pl);

class $modify(CkyPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        cky::frameClock = 0.0;
        cky::playbackCursor = 0;
        cky::armed = false;
        cky::checkpoints.clear();
        cky::lastCheckpointCount = 0;
        ckyBuildPanel(this);
        ckyBuildHud(this);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        
        if (!cky::checkpoints.empty() && m_isPracticeMode) {
            auto lastCp = cky::checkpoints.back();
            cky::frameClock = lastCp.frame;
            cky::playbackCursor = lastCp.cursor;
        } else {
            cky::frameClock = 0.0;
            cky::playbackCursor = 0;
            cky::checkpoints.clear();
        }

        cky::clickAccumulator = 0.0f;
        cky::currentClicks = 0;

        if (cky::recording) {
            cky::currentMacro.clear();
            cky::armed = true;
        }
        if (cky::playing) cky::armed = true;
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        PlayLayer::processCommands(dt, isHalfTick, isLastTick);

        if (m_isPracticeMode && m_checkpointArray) {
            int currentCount = m_checkpointArray->count();
            if (currentCount > cky::lastCheckpointCount) {
                cky::CheckpointData cp;
                cp.frame = static_cast<uint32_t>(cky::frameClock + 0.5);
                cp.cursor = cky::playbackCursor;
                cky::checkpoints.push_back(cp);
            } else if (currentCount < cky::lastCheckpointCount) {
                if (cky::checkpoints.size() > currentCount) {
                    cky::checkpoints.resize(currentCount);
                }
            }
            cky::lastCheckpointCount = currentCount;
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (cky::noclip && object != m_anticheatSpike) return;
        PlayLayer::destroyPlayer(player, object);
    }
};
