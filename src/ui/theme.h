#pragma once

#include <array>
#include <cstddef>

#include <raylib.h>

namespace game2048::theme {

constexpr int kDefaultAnimationMs = 110;
constexpr int kSlowAnimationMs = 220;
constexpr int kTurboAnimationMs = 0;

constexpr float kAnimMergePhaseStart = 0.45F;
constexpr float kAnimSpawnPhaseStart = 0.35F;
constexpr float kAnimMergeBounceMag = 0.24F;
constexpr float kAnimSpawnMinScale = 0.72F;
constexpr float kAnimSlideFadeRate = 0.12F;

constexpr float kBasePanelHeight = 784.0F;

inline constexpr std::array<Color, 13> kTileFillColors {{
    Color{200, 190, 178, 255},
    Color{240, 232, 222, 255},
    Color{235, 218, 194, 255},
    Color{249, 178, 109, 255},
    Color{252, 148, 86, 255},
    Color{252, 110, 72, 255},
    Color{252, 72, 36, 255},
    Color{249, 212, 92, 255},
    Color{249, 206, 62, 255},
    Color{249, 200, 36, 255},
    Color{249, 194, 18, 255},
    Color{56, 202, 168, 255},
    Color{92, 68, 168, 255},
}};

inline constexpr std::array<Color, 13> kTileTextColors {{
    Color{119, 110, 101, 255},
    Color{119, 110, 101, 255},
    Color{119, 110, 101, 255},
    RAYWHITE,
    RAYWHITE,
    RAYWHITE,
    RAYWHITE,
    RAYWHITE,
    RAYWHITE,
    RAYWHITE,
    RAYWHITE,
    RAYWHITE,
    RAYWHITE,
}};

inline std::size_t TilePaletteIndex(int value) {
    if (value <= 0) {
        return 0;
    }

    std::size_t rank = 0;
    int current = value;
    while (current > 1) {
        current >>= 1;
        ++rank;
    }
    return rank < kTileFillColors.size() ? rank : kTileFillColors.size() - 1;
}

inline Color TileFillColor(int value) {
    return kTileFillColors[TilePaletteIndex(value)];
}

inline Color TileTextColor(int value) {
    return kTileTextColors[TilePaletteIndex(value)];
}

}  // namespace game2048::theme
