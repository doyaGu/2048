#include "renderer.h"

#include <array>
#include <cmath>
#include <string>

#include <raymath.h>

namespace game2048 {

namespace {

Color TileFillColor(int value) {
    switch (value) {
        case 0:    return {200, 190, 178, 255};
        case 2:    return {240, 232, 222, 255};
        case 4:    return {235, 218, 194, 255};
        case 8:    return {249, 178, 109, 255};
        case 16:   return {252, 148, 86,  255};
        case 32:   return {252, 110, 72,  255};
        case 64:   return {252,  72, 36,  255};
        case 128:  return {249, 212, 92,  255};
        case 256:  return {249, 206, 62,  255};
        case 512:  return {249, 200, 36,  255};
        case 1024: return {249, 194, 18,  255};
        case 2048: return { 56, 202, 168, 255};  // teal — win tile stands apart
        case 4096: return {118,  82, 214, 255};  // vivid purple
        default:   return { 92,  68, 168, 255};  // deep purple for 8192+
    }
}

Color TileTextColor(int value) {
    return value <= 4 ? Color{119, 110, 101, 255} : RAYWHITE;
}

void DrawCenteredText(const std::string& text, Rectangle rect, Color color) {
    Font font = GetFontDefault();
    float fontSize = rect.height * 0.42F;
    if (text.size() >= 4) fontSize *= 0.84F;
    if (text.size() >= 5) fontSize *= 0.72F;
    const Vector2 size = MeasureTextEx(font, text.c_str(), fontSize, 1.0F);
    const Vector2 pos {
        rect.x + (rect.width - size.x) * 0.5F,
        rect.y + (rect.height - size.y) * 0.5F
    };
    DrawTextEx(font, text.c_str(), pos, fontSize, 1.0F, color);
}

Rectangle ScaleRect(Rectangle rect, float scale) {
    const float newWidth = rect.width * scale;
    const float newHeight = rect.height * scale;
    return {
        rect.x + (rect.width - newWidth) * 0.5F,
        rect.y + (rect.height - newHeight) * 0.5F,
        newWidth,
        newHeight
    };
}

void DrawTile(Rectangle rect, int value, float scale, unsigned char alpha = 255) {
    const Rectangle scaled = ScaleRect(rect, scale);
    const Color fill = TileFillColor(value);

    // Glow rings for high-value tiles (drawn first — furthest back)
    if (value >= 512 && alpha == 255) {
        const float glowRadii[] = {12.0F, 7.0F, 3.5F};
        const unsigned char glowAlphas[] = {10, 20, 36};
        for (int i = 0; i < 3; ++i) {
            const float exp = glowRadii[i];
            Rectangle glow = {scaled.x - exp, scaled.y - exp, scaled.width + exp * 2.0F, scaled.height + exp * 2.0F};
            Color gc = fill;
            gc.a = glowAlphas[i];
            DrawRectangleRounded(glow, 0.22F, 8, gc);
        }
    }

    // Drop shadow (only for fully opaque tiles)
    if (value != 0 && alpha == 255) {
        Rectangle shadow = scaled;
        shadow.x += 3.0F;
        shadow.y += 4.0F;
        DrawRectangleRounded(shadow, 0.16F, 8, Color{20, 16, 12, 30});
    }

    Color tileFill = fill;
    tileFill.a = alpha;
    DrawRectangleRounded(scaled, 0.16F, 8, tileFill);
    if (value != 0) {
        Color text = TileTextColor(value);
        text.a = alpha;
        DrawCenteredText(std::to_string(value), scaled, text);
    }
}

}  // namespace

void Renderer::DrawBoard(const LayoutMetrics& layout, const Board& board, const AnimationController& animation) const {
    // Warmer gradient background
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), Color{250, 244, 235, 255}, Color{236, 228, 216, 255});
    // Decorative background blobs
    DrawCircle(GetScreenWidth() - 110, 120, 190.0F, Fade(Color{249, 194, 18,  255}, 0.08F));  // warm amber top-right
    DrawCircle(100, GetScreenHeight() - 110, 170.0F, Fade(Color{56,  202, 168, 255}, 0.09F)); // teal bottom-left
    DrawCircle(GetScreenWidth() / 2, GetScreenHeight() - 60, 140.0F, Fade(Color{200, 190, 178, 255}, 0.07F)); // faint center-bottom

    // Apply screen-shake offset
    const Vector2 shake = animation.ShakeOffset();
    LayoutMetrics shaken = layout;
    shaken.boardRect.x += shake.x;
    shaken.boardRect.y += shake.y;
    for (auto& r : shaken.tileRects) {
        r.x += shake.x;
        r.y += shake.y;
    }

    DrawRectangleRounded(shaken.boardRect, 0.05F, 10, Color{187, 173, 160, 255});
    for (const auto& rect : shaken.tileRects) {
        DrawRectangleRounded(rect, 0.16F, 8, Color{200, 190, 178, 255});
    }

    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const CellCoord cell {row, col};
            const Rectangle rect = shaken.tileRects[row * kBoardSize + col];
            const int value = board.At(row, col);
            if (value == 0) {
                continue;
            }

            if (animation.Active() && animation.IsMovingTo(cell) && animation.SlideProgress() < 1.0F) {
                continue;
            }

            float scale = 1.0F;
            if (animation.Active()) {
                scale = std::max(animation.MergeScale(cell), animation.SpawnScale(cell));
            }
            DrawTile(rect, value, scale);
        }
    }

    if (!animation.Active()) {
        return;
    }

    const float t = animation.SlideProgress();
    const auto& snapshot = animation.Snapshot();
    for (const auto& move : snapshot.trace.moves) {
        const Rectangle fromRect = shaken.tileRects[move.from.row * kBoardSize + move.from.col];
        const Rectangle toRect   = shaken.tileRects[move.to.row   * kBoardSize + move.to.col];
        Rectangle current {
            Lerp(fromRect.x, toRect.x, t),
            Lerp(fromRect.y, toRect.y, t),
            fromRect.width,
            fromRect.height
        };
        DrawTile(current, move.value, 1.0F, static_cast<unsigned char>(255.0F * (1.0F - 0.12F * t)));
    }
}

}  // namespace game2048
