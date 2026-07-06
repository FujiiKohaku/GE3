#include "TimelineProfiler.h"

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#include <algorithm>

void TimelineProfiler::Initialize()
{
    history_.clear();
    history_.resize(kMaxFrames);
    selectedFrameIndex_ = 0;
    pendingEvents_.clear();
    currentFrame_.events.clear();
}

void TimelineProfiler::BeginFrame()
{
    frameStart_ = std::chrono::high_resolution_clock::now();
    currentFrame_.events.clear();
    currentFrame_.totalTimeMs = 0.0f;
    pendingEvents_.clear();
}

void TimelineProfiler::EndFrame()
{
    auto now = std::chrono::high_resolution_clock::now();
    float totalTime = std::chrono::duration<float, std::milli>(now - frameStart_).count();
    currentFrame_.totalTimeMs = totalTime;

    // 履歴に追加 (リングバッファ)
    history_.erase(history_.begin());
    history_.push_back(currentFrame_);

    if (selectedFrameIndex_ >= kMaxFrames) {
        selectedFrameIndex_ = kMaxFrames - 1;
    }
}

void TimelineProfiler::BeginEvent(const std::string& name)
{
    PendingEvent pe;
    pe.name = name;
    pe.start = std::chrono::high_resolution_clock::now();
    pendingEvents_.push_back(pe);
}

void TimelineProfiler::EndEvent(const std::string& name)
{
    auto now = std::chrono::high_resolution_clock::now();
    for (auto it = pendingEvents_.rbegin(); it != pendingEvents_.rend(); ++it) {
        if (it->name == name) {
            Event e;
            e.name = name;
            e.startTimeMs = std::chrono::duration<float, std::milli>(it->start - frameStart_).count();
            e.endTimeMs = std::chrono::duration<float, std::milli>(now - frameStart_).count();
            
            currentFrame_.events.push_back(e);
            
            // 一致したものを削除
            auto distance = std::distance(pendingEvents_.rbegin(), it);
            pendingEvents_.erase(pendingEvents_.end() - 1 - distance);
            break;
        }
    }
}

static ImU32 GetEventColor(const std::string& name)
{
    size_t hash = std::hash<std::string>{}(name);
    uint8_t r = 100 + (hash & 0xFF) % 150;
    uint8_t g = 100 + ((hash >> 8) & 0xFF) % 150;
    uint8_t b = 100 + ((hash >> 16) & 0xFF) % 150;
    return IM_COL32(r, g, b, 255);
}

void TimelineProfiler::DrawTimelineUI()
{
    const size_t latestFrameIdx = kMaxFrames - 1;
    FrameData& frame = history_[latestFrameIdx];

    ImGui::Text("Frame Duration: %.2f ms (60 FPS Target: 16.67 ms)", frame.totalTimeMs);

    ImVec2 size = ImGui::GetContentRegionAvail();
    size.y = std::max(size.y, 250.0f);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + size.x, canvasPos.y + size.y), IM_COL32(30, 30, 30, 255));
    drawList->AddRect(canvasPos, ImVec2(canvasPos.x + size.x, canvasPos.y + size.y), IM_COL32(80, 80, 80, 255));

    float maxTimeScale = std::max(16.67f, frame.totalTimeMs);
    float widthPerMs = size.x / maxTimeScale;

    // グリッド線（5ms刻み）の描画
    for (float t = 5.0f; t < maxTimeScale; t += 5.0f) {
        float gridX = canvasPos.x + (t * widthPerMs);
        drawList->AddLine(ImVec2(gridX, canvasPos.y), ImVec2(gridX, canvasPos.y + size.y), IM_COL32(60, 60, 60, 255));
        
        char text[16];
        sprintf_s(text, "%.0fms", t);
        drawList->AddText(ImVec2(gridX + 2.0f, canvasPos.y + 2.0f), IM_COL32(150, 150, 150, 255), text);
    }

    float rowHeight = 30.0f;
    float startY = canvasPos.y + 25.0f;

    std::vector<std::string> rowNames;
    auto GetRowIndex = [&](const std::string& name) {
        for (size_t i = 0; i < rowNames.size(); ++i) {
            if (rowNames[i] == name) return i;
        }
        rowNames.push_back(name);
        return rowNames.size() - 1;
    };

    for (const auto& e : frame.events) {
        size_t row = GetRowIndex(e.name);
        
        float x1 = canvasPos.x + (e.startTimeMs * widthPerMs);
        float x2 = canvasPos.x + (e.endTimeMs * widthPerMs);
        float y1 = startY + (row * (rowHeight + 8.0f));
        float y2 = y1 + rowHeight;

        x1 = std::max(x1, canvasPos.x);
        x2 = std::min(x2, canvasPos.x + size.x);

        if (x2 > x1) {
            ImU32 color = GetEventColor(e.name);
            drawList->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), color, 6.0f);
            drawList->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(255, 255, 255, 120), 6.0f);

            if (x2 - x1 > 30.0f) {
                char text[128];
                sprintf_s(text, "%s (%.2fms)", e.name.c_str(), e.endTimeMs - e.startTimeMs);
                
                ImVec2 textSize = ImGui::CalcTextSize(text);
                float textX = x1 + 8.0f;
                float textY = y1 + (rowHeight - textSize.y) * 0.5f;
                
                if (textX + textSize.x < x2) {
                    drawList->AddText(ImVec2(textX, textY), IM_COL32(0, 0, 0, 255), text);
                }
            }
        }
    }

    ImGui::Dummy(size);
}
#endif
