#include "ui/widgets.h"

#include <algorithm>
#include <string>

#include "ui/theme.h"

namespace game2048 {

int FitFontSize(const std::string& text, float maxWidth, int fontSize, int minFontSize, int step) {
    int fittedFontSize = fontSize;
    while (fittedFontSize > minFontSize && MeasureText(text.c_str(), fittedFontSize) > static_cast<int>(maxWidth)) {
        fittedFontSize -= step;
    }
    return fittedFontSize;
}

PanelStyle MakeStyle(float panelHeight, float innerWidth) {
    const float scale = std::clamp(panelHeight / theme::kBasePanelHeight, 0.52F, 1.0F);
    PanelStyle style {};
    style.scale = scale;
    style.infoFs = std::max(11, static_cast<int>(18.0F * scale));
    style.headerFs = std::max(11, static_cast<int>(20.0F * scale));
    style.evalFs = std::max(10, static_cast<int>(15.0F * scale));
    style.keyFs = std::max(10, static_cast<int>(16.0F * scale));
    style.infoRowH = std::max(14.0F, 24.0F * scale);
    style.headerH = std::max(18.0F, 32.0F * scale);
    style.evalRowH = std::max(13.0F, 27.0F * scale);
    style.keyRowH = std::max(13.0F, 20.0F * scale);
    style.divGap = std::max(3.0F, 6.0F * scale);
    style.labelColW = std::clamp(innerWidth * 0.44F, 60.0F, 118.0F);
    return style;
}

void DrawControlButton(Rectangle rect, const char* label, bool active, bool disabled) {
    Color fill = disabled ? Fade(Color{170, 162, 152, 255}, 0.55F)
                          : active ? Color{56, 202, 168, 255}
                                   : Color{119, 110, 101, 235};
    DrawRectangleRounded(rect, 0.22F, 8, fill);
    DrawRectangleLinesEx(rect, 1.2F, Fade(RAYWHITE, 0.22F));
    const int fontSize = std::max(12, static_cast<int>(rect.height * 0.32F));
    const int textWidth = MeasureText(label, fontSize);
    DrawText(label,
             static_cast<int>(rect.x + (rect.width - static_cast<float>(textWidth)) * 0.5F),
             static_cast<int>(rect.y + (rect.height - static_cast<float>(fontSize)) * 0.5F),
             fontSize,
             RAYWHITE);
}

void DrawMetricBox(Rectangle rect, const char* label, const std::string& value, Color fill) {
    DrawRectangleRounded(rect, 0.18F, 8, fill);
    const int labelFs = std::max(12, static_cast<int>(rect.height * 0.28F));
    DrawText(label, static_cast<int>(rect.x + 10.0F), static_cast<int>(rect.y + 5.0F), labelFs, Fade(RAYWHITE, 0.72F));

    const int valueFs = FitFontSize(value, rect.width - 18.0F, std::max(14, static_cast<int>(rect.height * 0.46F)), 12, 2);
    const int valueY = static_cast<int>(rect.y + rect.height - static_cast<float>(valueFs) - 5.0F);
    DrawText(value.c_str(), static_cast<int>(rect.x + 10.0F), valueY, valueFs, RAYWHITE);
}

void DrawFittedText(const std::string& text, float x, float y, float maxWidth, int fontSize, Color color) {
    DrawText(text.c_str(), static_cast<int>(x), static_cast<int>(y), FitFontSize(text, maxWidth, fontSize), color);
}

void DrawSectionHeader(const Rectangle& panelRect, float& y, const char* title, const PanelStyle& style) {
    DrawRectangle(static_cast<int>(panelRect.x + 1.0F),
                  static_cast<int>(y - 3.0F),
                  static_cast<int>(panelRect.width - 2.0F),
                  static_cast<int>(style.headerH),
                  Fade(Color{143, 122, 102, 255}, 0.13F));
    DrawText(title, static_cast<int>(panelRect.x + 14.0F), static_cast<int>(y), style.headerFs, Color{100, 88, 76, 255});
    y += style.headerH + 2.0F;
}

void DrawDivider(const Rectangle& panelRect, float y) {
    DrawLineEx({panelRect.x + 8.0F, y},
               {panelRect.x + panelRect.width - 8.0F, y},
               1.0F,
               Fade(Color{143, 122, 102, 255}, 0.30F));
}

void DrawInfoLine(float x, float& y, float innerWidth, const std::string& label, const std::string& value, const PanelStyle& style) {
    DrawText(label.c_str(), static_cast<int>(x), static_cast<int>(y), style.infoFs, Color{119, 110, 101, 255});

    const float valueX = x + style.labelColW;
    const int valueFs = FitFontSize(value, innerWidth - style.labelColW, style.infoFs);
    DrawText(value.c_str(), static_cast<int>(valueX), static_cast<int>(y), valueFs, Color{58, 59, 84, 255});
    y += style.infoRowH;
}

void DrawEvalRow(float x, float& y, float innerWidth, const char* label, double value, const PanelStyle& style) {
    const float maxExpected = 400.0F;
    const float clamped = std::clamp(static_cast<float>(value), 0.0F, maxExpected);
    const float barWidth = innerWidth * (clamped / maxExpected);
    const float barHeight = std::max(3.0F, 5.0F * style.scale);

    DrawText(label, static_cast<int>(x), static_cast<int>(y), style.evalFs, Color{119, 110, 101, 255});

    const std::string valueText = TextFormat("%.1f", value);
    const float valueX = x + style.labelColW;
    const int valueFs = FitFontSize(valueText, innerWidth - style.labelColW, style.evalFs);
    DrawText(valueText.c_str(), static_cast<int>(valueX), static_cast<int>(y), valueFs, Color{58, 59, 84, 255});

    const float barY = y + static_cast<float>(style.evalFs) + 2.0F;
    DrawRectangleRounded({x, barY, innerWidth, barHeight}, 1.0F, 4, Fade(Color{143, 122, 102, 255}, 0.18F));
    if (barWidth > 0.5F) {
        DrawRectangleRounded({x, barY, barWidth, barHeight}, 1.0F, 4, Fade(Color{119, 110, 101, 255}, 0.55F));
    }
    y += style.evalRowH;
}

}  // namespace game2048
