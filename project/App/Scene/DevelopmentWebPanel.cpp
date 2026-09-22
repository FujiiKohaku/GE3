#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <shellapi.h>

#include "DevelopmentWebPanel.h"

#if defined(ENABLE_DEVELOPMENT_TOOLS)

#include "GamePlayScene.h"
#include "TestScene1.h"
#include "Engine/PostEffect/PostEffectManager.h"
#include "externals/json.hpp"
#include "Engine/Logger/Logger.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string_view>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")

namespace {
constexpr SOCKET kInvalidSocket = INVALID_SOCKET;
constexpr std::size_t kMaxRequestSize = 8192;
constexpr std::size_t kMaxClients = 8;

std::string QueryValue(std::string_view target, std::string_view name)
{
    const std::size_t queryStart = target.find('?');
    if (queryStart == std::string_view::npos) return {};
    target.remove_prefix(queryStart + 1);
    while (!target.empty()) {
        const std::size_t end = target.find('&');
        const std::string_view part = target.substr(0, end);
        const std::size_t equals = part.find('=');
        if (equals != std::string_view::npos && part.substr(0, equals) == name) {
            return std::string(part.substr(equals + 1));
        }
        if (end == std::string_view::npos) break;
        target.remove_prefix(end + 1);
    }
    return {};
}

std::string LoadPage()
{
    wchar_t modulePath[MAX_PATH] {};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    const std::filesystem::path repositoryPage =
        std::filesystem::path(modulePath).parent_path().parent_path()
            .parent_path().parent_path() /
        "project/resources/DevelopmentPanel/index.html";
    for (const std::filesystem::path& path : {
             std::filesystem::path("resources/DevelopmentPanel/index.html"),
             std::filesystem::path("project/resources/DevelopmentPanel/index.html"),
             repositoryPage }) {
        std::ifstream file(path, std::ios::binary);
        if (file) return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }
    return {};
}

void SendResponse(SOCKET socket, int status, std::string_view contentType,
                  std::string_view body)
{
    std::string response = "HTTP/1.1 " + std::to_string(status) +
        (status == 200 ? " OK\r\n" : " Error\r\n");
    response += "Content-Type: " + std::string(contentType) + "\r\n";
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    response += "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n";
    response += "Referrer-Policy: no-referrer\r\nConnection: close\r\n\r\n";
    response.append(body);
    u_long blocking = 0;
    ioctlsocket(socket, FIONBIO, &blocking);
    const DWORD sendTimeoutMs = 100;
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&sendTimeoutMs), sizeof(sendTimeoutMs));
    const char* data = response.data();
    int remaining = static_cast<int>(response.size());
    while (remaining > 0) {
        const int sent = send(socket, data, remaining, 0);
        if (sent <= 0) break;
        data += sent;
        remaining -= sent;
    }
}
}

DevelopmentWebPanel& DevelopmentWebPanel::GetInstance()
{
    static DevelopmentWebPanel panel;
    return panel;
}

void DevelopmentWebPanel::SetScene(GamePlayScene* scene)
{
    if (scene == nullptr && postEffectManager_) {
        postEffectManager_->ClearDevelopmentPassOverrides();
    }
    scene_ = scene;
    if (scene) testScene_ = nullptr;
}

void DevelopmentWebPanel::SetTestScene(TestScene1* scene)
{
    if (scene == nullptr && postEffectManager_) {
        postEffectManager_->ClearDevelopmentPassOverrides();
    }
    testScene_ = scene;
    if (scene) scene_ = nullptr;
}

bool DevelopmentWebPanel::Start()
{
    if (listener_ != static_cast<std::uintptr_t>(kInvalidSocket)) return true;
    WSADATA data {};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
    winsockStarted_ = true;
    const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == kInvalidSocket) { Stop(); return false; }
    listener_ = static_cast<std::uintptr_t>(listener);

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(8765);
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        // Another Development instance may already own the usual port.
        address.sin_port = 0;
        if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            Stop();
            return false;
        }
    }
    if (listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        Stop();
        return false;
    }
    int addressLength = sizeof(address);
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&address), &addressLength) == SOCKET_ERROR) {
        Stop();
        return false;
    }
    port_ = ntohs(address.sin_port);
    u_long nonblocking = 1;
    ioctlsocket(listener, FIONBIO, &nonblocking);

    std::random_device random;
    token_ = std::to_string(random()) + std::to_string(random());
    const std::wstring url = L"http://127.0.0.1:" + std::to_wstring(port_) + L"/";
    Logger::Log("Development panel: http://127.0.0.1:" + std::to_string(port_) + "/");
    Logger::Flush();
    SetWindowTextW(WinApp::GetInstance()->GetHwnd(),
                   (L"KohakuEngine Development | " + url).c_str());
    browserLaunchPending_ = GetEnvironmentVariableW(L"KOH_DEV_PANEL_NO_BROWSER", nullptr, 0) == 0;
    browserLaunchAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    return true;
}

void DevelopmentWebPanel::Stop()
{
    scene_ = nullptr;
    testScene_ = nullptr;
    postEffectManager_ = nullptr;
    browserLaunchPending_ = false;
    for (Client& client : clients_) closesocket(static_cast<SOCKET>(client.socket));
    clients_.clear();
    if (listener_ != static_cast<std::uintptr_t>(kInvalidSocket)) {
        closesocket(static_cast<SOCKET>(listener_));
        listener_ = static_cast<std::uintptr_t>(kInvalidSocket);
    }
    if (winsockStarted_) {
        WSACleanup();
        winsockStarted_ = false;
    }
}

void DevelopmentWebPanel::Poll()
{
    if (listener_ == static_cast<std::uintptr_t>(kInvalidSocket)) return;
    if (browserLaunchPending_ && std::chrono::steady_clock::now() >= browserLaunchAt_) {
        browserLaunchPending_ = false;
        const std::wstring url = L"http://127.0.0.1:" + std::to_wstring(port_) + L"/";
        if (reinterpret_cast<std::intptr_t>(ShellExecuteW(
                WinApp::GetInstance()->GetHwnd(), L"open", url.c_str(),
                nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
            Logger::Warning(std::string("Could not open the Development panel in a browser: ") +
                            "http://127.0.0.1:" + std::to_string(port_) + "/");
        }
    }
    const SOCKET listener = static_cast<SOCKET>(listener_);
    for (int attempt = 0; attempt < 4 && clients_.size() < kMaxClients; ++attempt) {
        const SOCKET accepted = accept(listener, nullptr, nullptr);
        if (accepted == kInvalidSocket) break;
        u_long nonblocking = 1;
        ioctlsocket(accepted, FIONBIO, &nonblocking);
        clients_.push_back({ static_cast<std::uintptr_t>(accepted), {},
                             std::chrono::steady_clock::now() });
    }
    for (std::size_t index = 0; index < clients_.size();) {
        Client& client = clients_[index];
        char buffer[4096];
        const int received = recv(static_cast<SOCKET>(client.socket), buffer, sizeof(buffer), 0);
        bool done = false;
        if (received > 0) {
            client.request.append(buffer, received);
            if (client.request.find("\r\n\r\n") != std::string::npos) {
                Respond(client);
                done = true;
            } else if (client.request.size() > kMaxRequestSize) {
                done = true;
            }
        } else if (received == 0 || WSAGetLastError() != WSAEWOULDBLOCK) {
            done = true;
        }
        if (std::chrono::steady_clock::now() - client.acceptedAt > std::chrono::seconds(2)) {
            done = true;
        }
        if (done) {
            closesocket(static_cast<SOCKET>(client.socket));
            clients_.erase(clients_.begin() + index);
        } else {
            ++index;
        }
    }
}

void DevelopmentWebPanel::Respond(Client& client)
{
    const std::size_t lineEnd = client.request.find("\r\n");
    const std::string_view line(client.request.data(), lineEnd);
    const std::size_t firstSpace = line.find(' ');
    const std::size_t secondSpace = line.find(' ', firstSpace + 1);
    if (firstSpace == std::string_view::npos || secondSpace == std::string_view::npos) {
        SendResponse(static_cast<SOCKET>(client.socket), 400, "text/plain", "Bad request");
        return;
    }
    const std::string_view method = line.substr(0, firstSpace);
    const std::string_view target = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    const SOCKET socket = static_cast<SOCKET>(client.socket);
    if (method == "GET" && target == "/") {
        std::string page = LoadPage();
        if (page.empty()) {
            SendResponse(socket, 404, "text/plain", "Development panel page missing");
            return;
        }
        const std::size_t marker = page.find("__PANEL_TOKEN__");
        if (marker != std::string::npos) page.replace(marker, 15, token_);
        SendResponse(socket, 200, "text/html; charset=utf-8", page);
    } else if (method == "GET" && target == "/api/state") {
        nlohmann::json body = scene_ ? nlohmann::json::parse(scene_->GetDevelopmentStateJson())
                                      : nlohmann::json{{"active", false}};
        if (testScene_) {
            body["testActive"] = true;
            body["test"] = nlohmann::json::parse(testScene_->GetDevelopmentStateJson());
        }
        if (postEffectManager_) {
            body["effects"] = nlohmann::json::parse(postEffectManager_->GetDevelopmentSettingsJson());
        }
        SendResponse(socket, 200, "application/json; charset=utf-8", body.dump());
    } else if (method == "POST" && target.starts_with("/api/action?") &&
               QueryValue(target, "token") == token_) {
        if (scene_ || testScene_) {
            const std::string key = QueryValue(target, "key");
            const std::string value = QueryValue(target, "value");
            if (key.starts_with("effects.") && postEffectManager_) {
                postEffectManager_->ApplyDevelopmentSetting(key.substr(8), value);
            } else if (key.starts_with("test.") && testScene_) {
                testScene_->ApplyDevelopmentAction(key.substr(5), value);
            } else if (scene_) {
                scene_->ApplyDevelopmentAction(key, value);
            }
        }
        SendResponse(socket, 200, "application/json", "{\"ok\":true}");
    } else {
        SendResponse(socket, 404, "text/plain", "Not found");
    }
}

#endif
