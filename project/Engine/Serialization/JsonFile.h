#pragma once
#include "externals/json.hpp"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace JsonFile {
inline nlohmann::json Load(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (!stream) { throw std::runtime_error("JSONを開けません: " + path.generic_string()); }
    return nlohmann::json::parse(stream);
}

inline void Save(const std::filesystem::path& path, const nlohmann::json& value)
{
    if (path.empty()) { throw std::runtime_error("保存先を指定してください"); }
    if (!path.parent_path().empty()) { std::filesystem::create_directories(path.parent_path()); }
    auto temporaryPath = path;
    temporaryPath += L".tmp";
    const std::string serialized = value.dump(2) + "\n";
    if (serialized.size() > MAXDWORD) { throw std::runtime_error("JSONが大きすぎます"); }
    HANDLE file = CreateFileW(temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { throw std::runtime_error("一時ファイルを作成できません。保存先と.tmpの有無を確認してください"); }
    try {
        DWORD writtenBytes = 0;
        if (!WriteFile(file, serialized.data(), static_cast<DWORD>(serialized.size()), &writtenBytes, nullptr) ||
            writtenBytes != serialized.size() || !FlushFileBuffers(file)) { throw std::runtime_error("JSONの書き込みに失敗しました"); }
        CloseHandle(file);
        file = INVALID_HANDLE_VALUE;
        if (!MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            throw std::runtime_error("JSONを置き換えられません。元のファイルを保持しました");
        }
    } catch (...) {
        if (file != INVALID_HANDLE_VALUE) { CloseHandle(file); }
        DeleteFileW(temporaryPath.c_str());
        throw;
    }
}
}
