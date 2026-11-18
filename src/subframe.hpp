#pragma once
#include <Geode/Geode.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>

enum class CkySfPhase : uint8_t { Begin = 0, Input = 1, Half = 2 };

struct CkySfBlob {
    uint32_t frame = 0;
    uint8_t dual = 0;
    uint16_t n[2] = { 0, 0 };
    float px[2] = { 0.f, 0.f };
    float py[2] = { 0.f, 0.f };
    float rot[2] = { 0.f, 0.f };
    std::vector<uint8_t> bytes;
};

namespace cky {
    inline bool sfOn = true;
    inline bool sfRecord = true;
    inline bool sfApply = true;
    inline int sfDepth = 240;
    inline int sfInterval = 120;
    inline std::vector<CkySfBlob> sfBlobs;
    inline size_t sfCursor = 0;
    inline uint32_t sfRingChecks = 0;
    inline uint32_t sfRingMisses = 0;
    inline uint32_t sfBlobChecks = 0;
    inline uint32_t sfBlobMisses = 0;
}

void ckySfCapture(GJBaseGameLayer* layer, CkySfPhase phase);
void ckySfClear(bool release = false);
void ckySfSettle(GJBaseGameLayer* layer, uint32_t step);
void ckySfResimStep(GJBaseGameLayer* layer, uint32_t step, bool half);
size_t ckySfCount();
size_t ckySfBytes();
uint32_t ckySfLayoutHash();
CkySfBlob ckySfMakeBlob(GJBaseGameLayer* layer, uint32_t frame);
bool ckySfApplyBlob(GJBaseGameLayer* layer, CkySfBlob const& blob);
