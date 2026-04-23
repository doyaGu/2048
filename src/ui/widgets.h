#pragma once

#include <string>

#include <raylib.h>

namespace game2048 {

struct PanelStyle {
    float scale = 1.0F;
    int infoFs = 0;
    int headerFs = 0;
    int evalFs = 0;
    int keyFs = 0;
    float infoRowH = 0.0F;
    float headerH = 0.0F;
    float evalRowH = 0.0F;
    float keyRowH = 0.0F;
    float divGap = 0.0F;
    float labelColW = 0.0F;
};

int FitFontSize(const std::string& text, float maxWidth, int fontSize, int minFontSize = 9, int step = 1);
PanelStyle MakeStyle(float panelHeight, float innerWidth);
void DrawControlButton(Rectangle rect, const char* label, bool active, bool disabled);
void DrawMetricBox(Rectangle rect, const char* label, const std::string& value, Color fill);
void DrawFittedText(const std::string& text, float x, float y, float maxWidth, int fontSize, Color color);
void DrawSectionHeader(const Rectangle& panelRect, float& y, const char* title, const PanelStyle& style);
void DrawDivider(const Rectangle& panelRect, float y);
void DrawInfoLine(float x, float& y, float innerWidth, const std::string& label, const std::string& value, const PanelStyle& style);
void DrawEvalRow(float x, float& y, float innerWidth, const char* label, double value, const PanelStyle& style);

}  // namespace game2048
