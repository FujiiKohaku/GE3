#pragma once
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <vector>
#include <cstdint>

// Timestamp polling of explicit source files and their transitive HLSL includes. Never scans DXIL caches.
class EditorSourceWatch {
public:
    void SetRoots(const std::vector<std::filesystem::path>& roots, bool shouldAcknowledge = false)
    {
        std::vector<std::filesystem::path> absoluteRoots;
        for (const auto& root : roots) {
            if (!root.empty()) { absoluteRoots.push_back(std::filesystem::absolute(root).lexically_normal()); }
        }
        if (absoluteRoots == roots_ && !shouldAcknowledge) { return; }
        roots_ = std::move(absoluteRoots);
        RebuildSnapshot(shouldAcknowledge);
    }

    bool Poll(uint64_t nowMs)
    {
        auto current = observed_;
        for (auto& entry : current) { entry.second = ReadStamp(entry.first); }
        if (current != observed_) {
            observed_ = std::move(current);
            changedAtMs_ = nowMs; isPending_ = true;
            return false;
        }
        constexpr uint64_t kSettleTimeMs = 400;
        if (!isPending_ || nowMs < changedAtMs_ || nowMs - changedAtMs_ < kSettleTimeMs) { return false; }
        isPending_ = false;
        RebuildSnapshot(false);
        return true;
    }

private:
    void RebuildSnapshot(bool shouldAcknowledge)
    {
        auto current = ReadSources();
        if (!shouldAcknowledge) {
            for (auto& entry : current) {
                const auto previous = observed_.find(entry.first);
                if (previous != observed_.end()) { entry.second = previous->second; }
            }
        } else { isPending_ = false; }
        observed_ = std::move(current);
    }
    struct FileStamp {
        std::filesystem::file_time_type writeTime {};
        uintmax_t sizeBytes = 0;
        bool isReadable = false;
        bool operator==(const FileStamp&) const = default;
    };
    using Snapshot = std::map<std::filesystem::path, FileStamp>;

    static FileStamp ReadStamp(const std::filesystem::path& path)
    {
        FileStamp stamp;
        std::error_code error;
        stamp.writeTime = std::filesystem::last_write_time(path, error);
        if (error) { return stamp; }
        stamp.sizeBytes = std::filesystem::file_size(path, error);
        stamp.isReadable = !error;
        return stamp;
    }

    static void AddSource(const std::filesystem::path& path, Snapshot& snapshot)
    {
        const auto absolute = std::filesystem::absolute(path).lexically_normal();
        if (snapshot.contains(absolute)) { return; }
        snapshot.emplace(absolute, ReadStamp(absolute));
        if (path.extension() == ".json") { return; }
        std::ifstream stream(absolute);
        std::string line;
        static const std::regex kIncludeExpression(R"(^\s*#\s*include\s*["<]([^">]+)[">])");
        while (std::getline(stream, line)) {
            std::smatch match;
            if (std::regex_search(line, match, kIncludeExpression)) { AddSource(absolute.parent_path() / match[1].str(), snapshot); }
        }
    }

    Snapshot ReadSources() const
    {
        Snapshot snapshot;
        for (const auto& root : roots_) { if (!root.empty()) { AddSource(root, snapshot); } }
        return snapshot;
    }

    std::vector<std::filesystem::path> roots_;
    Snapshot observed_;
    uint64_t changedAtMs_ = 0;
    bool isPending_ = false;
};
