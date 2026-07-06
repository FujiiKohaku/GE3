#include "Profiler.h"

#ifdef _DEBUG
#include "CPUProfiler.h"
#include "GPUProfiler.h"
#include "MemoryProfiler.h"
#include "PerformanceCounter.h"
#include "BootProfiler.h"
#include "TimelineProfiler.h"

#include "externals/imgui/imgui.h"
#include "Engine/DirectXCommon/DirectXCommon.h"

std::unique_ptr<Profiler> Profiler::instance_ = nullptr;

Profiler* Profiler::GetInstance()
{
    if (!instance_) {
        instance_.reset(new Profiler());
    }
    return instance_.get();
}

void Profiler::FinalizeInstance()
{
    if (instance_) {
        instance_->Finalize();
        instance_.reset();
    }
}

void Profiler::Initialize()
{
    cpuProfiler_ = std::make_unique<CPUProfiler>();
    gpuProfiler_ = std::make_unique<GPUProfiler>();
    memoryProfiler_ = std::make_unique<MemoryProfiler>();
    performanceCounter_ = std::make_unique<PerformanceCounter>();
    bootProfiler_ = std::make_unique<BootProfiler>();
    timelineProfiler_ = std::make_unique<TimelineProfiler>();

    cpuProfiler_->Initialize();
    gpuProfiler_->Initialize();
    memoryProfiler_->Initialize();
    performanceCounter_->Initialize();
    bootProfiler_->Initialize();
    timelineProfiler_->Initialize();

    stats_.Reset();
}

void Profiler::Finalize()
{
    gpuProfiler_->Finalize();
}

void Profiler::BeginFrame()
{
    timelineProfiler_->BeginFrame();
    cpuProfiler_->Reset();
    stats_.Reset();
}

void Profiler::EndFrame()
{
    // GPUプロファイルの読み取りとクエリの解決
    ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandList();
    if (commandList) {
        gpuProfiler_->Resolve(commandList);
    }
    gpuProfiler_->Readback();

    timelineProfiler_->EndFrame();
}

void Profiler::Update()
{
    performanceCounter_->Update();
    memoryProfiler_->Update();
}

void Profiler::DrawImGui()
{
    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Engine Profiler", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("ProfilerTabBar")) {
        // ------------------ Performance Tab ------------------
        if (ImGui::BeginTabItem("Performance")) {
            ImGui::Text("=== Frame Rate ===");
            ImGui::Text("FPS: %.2f", performanceCounter_->GetFPS());
            ImGui::Text("Frame Time: %.2f ms", performanceCounter_->GetFrameTimeMs());
            ImGui::Separator();
            ImGui::Text("=== Statistics ===");
            ImGui::Text("Average FPS: %.2f", performanceCounter_->GetAverageFPS());
            ImGui::Text("Min FPS: %.2f", performanceCounter_->GetMinFPS());
            ImGui::Text("Max FPS: %.2f", performanceCounter_->GetMaxFPS());
            ImGui::Separator();
            ImGui::Text("=== Resource Usage ===");
            ImGui::Text("CPU Usage: %.1f %%", performanceCounter_->GetCPUUsage());
            ImGui::Text("GPU Usage: %.1f %%", performanceCounter_->GetGPUUsage());
            ImGui::Text("Delta Time: %.4f s", performanceCounter_->GetDeltaTime());
            ImGui::Text("Frame Count: %llu", performanceCounter_->GetFrameCount());

            // 簡易グラフ表示
            static float fpsHistory[100] = {};
            static int offset = 0;
            fpsHistory[offset] = performanceCounter_->GetFPS();
            offset = (offset + 1) % 100;
            ImGui::PlotLines("FPS History", fpsHistory, 100, offset, nullptr, 0.0f, 120.0f, ImVec2(0, 80));

            ImGui::EndTabItem();
        }

        // ------------------ CPU Tab ------------------
        if (ImGui::BeginTabItem("CPU")) {
            ImGui::Text("=== CPU Section Times ===");
            for (const auto& pair : cpuProfiler_->GetTimes()) {
                ImGui::Text("%-15s: %.3f ms", pair.first.c_str(), pair.second);
            }
            ImGui::EndTabItem();
        }

        // ------------------ GPU Tab ------------------
        if (ImGui::BeginTabItem("GPU")) {
            ImGui::Text("=== GPU Section Times ===");
            for (const auto& pair : gpuProfiler_->GetTimes()) {
                ImGui::Text("%-15s: %.3f ms", pair.first.c_str(), pair.second);
            }
            ImGui::EndTabItem();
        }

        // ------------------ Memory Tab ------------------
        if (ImGui::BeginTabItem("Memory")) {
            double cpuMB = static_cast<double>(memoryProfiler_->GetCPUMemoryBytes()) / (1024.0 * 1024.0);
            double gpuMB = static_cast<double>(memoryProfiler_->GetGPUMemoryBytes()) / (1024.0 * 1024.0);
            double uploadMB = static_cast<double>(memoryProfiler_->GetUploadHeapBytes()) / (1024.0 * 1024.0);
            double defaultMB = static_cast<double>(memoryProfiler_->GetDefaultHeapBytes()) / (1024.0 * 1024.0);
            double texMB = static_cast<double>(memoryProfiler_->GetTextureMemoryBytes()) / (1024.0 * 1024.0);
            double modelMB = static_cast<double>(memoryProfiler_->GetModelMemoryBytes()) / (1024.0 * 1024.0);
            double audioMB = static_cast<double>(memoryProfiler_->GetAudioMemoryBytes()) / (1024.0 * 1024.0);

            ImGui::Text("=== General Heap ===");
            ImGui::Text("CPU Memory: %.2f MB", cpuMB);
            ImGui::Text("GPU Memory: %.2f MB", gpuMB);
            ImGui::Separator();
            ImGui::Text("=== Heap Types ===");
            ImGui::Text("Default Heap: %.2f MB", defaultMB);
            ImGui::Text("Upload Heap: %.2f MB", uploadMB);
            ImGui::Separator();
            ImGui::Text("=== Resources ===");
            ImGui::Text("Texture Memory: %.2f MB", texMB);
            ImGui::Text("Model Memory:   %.2f MB", modelMB);
            ImGui::Text("Audio Memory:   %.2f MB", audioMB);
            ImGui::Separator();
            ImGui::Text("=== Descriptor Allocation ===");
            ImGui::Text("SRV Count: %u", memoryProfiler_->GetSrvCount());
            ImGui::Text("Descriptor Heap Usage: %.1f %%", memoryProfiler_->GetDescriptorHeapUsageRate() * 100.0f);

            ImGui::EndTabItem();
        }

        // ------------------ Boot Tab ------------------
        if (ImGui::BeginTabItem("Boot")) {
            ImGui::Text("=== Engine Initialization Times ===");
            for (const auto& pair : bootProfiler_->GetBootTimes()) {
                ImGui::Text("%-20s: %.2f ms", pair.first.c_str(), pair.second);
            }
            ImGui::Separator();
            ImGui::Text("Total Boot Time: %.2f ms", bootProfiler_->GetTotalBootTimeMs());
            ImGui::EndTabItem();
        }

        // ------------------ Timeline Tab ------------------
        if (ImGui::BeginTabItem("Timeline")) {
            timelineProfiler_->DrawTimelineUI();
            ImGui::EndTabItem();
        }

        // ------------------ Statistics Tab ------------------
        if (ImGui::BeginTabItem("Statistics")) {
            ImGui::Text("=== Renderer Frame Stats ===");
            ImGui::Text("Draw Calls:      %u", stats_.drawCalls);
            ImGui::Text("Triangles:       %u", stats_.triangles);
            ImGui::Text("Vertices:        %u", stats_.vertices);
            ImGui::Text("Indices:         %u", stats_.indices);
            ImGui::Separator();
            ImGui::Text("=== Loaded Asset Counts ===");
            ImGui::Text("Objects:         %u", stats_.objects);
            ImGui::Text("Meshes:          %u", stats_.meshes);
            ImGui::Text("Textures:        %u", stats_.textures);
            ImGui::Text("Materials:       %u", stats_.materials);
            ImGui::Text("Particles:       %u", stats_.particles);
            ImGui::Text("Lights:          %u", stats_.lights);
            ImGui::Text("Audio Clips:     %u", stats_.audios);
            ImGui::Text("Scenes:          %u", stats_.scenes);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}
#endif
