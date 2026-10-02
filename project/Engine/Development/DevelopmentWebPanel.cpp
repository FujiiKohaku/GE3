#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <shellapi.h>

#include "Engine/Development/DevelopmentWebPanel.h"

#if defined(ENABLE_DEVELOPMENT_TOOLS)

#include "externals/json.hpp"
#include "Engine/Logger/Logger.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string_view>
#include <stdexcept>
#include <unordered_map>
#include <cmath>
#include <cstdlib>
#include "Engine/Winapp/WinApp.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")

namespace {
constexpr SOCKET kInvalidSocket = INVALID_SOCKET;
constexpr std::size_t kMaxRequestSize = 8192;
constexpr std::size_t kMaxClients = 8;
constexpr int kMaxAcceptsPerFrame = 4;
constexpr std::size_t kSendChunkBytes = 16384;
constexpr auto kClientTimeoutSeconds = std::chrono::seconds(2);

int HexDigit(char character)
{
    if (character >= '0' && character <= '9') { return character - '0'; }
    if (character >= 'a' && character <= 'f') { return character - 'a' + 10; }
    if (character >= 'A' && character <= 'F') { return character - 'A' + 10; }
    return -1;
}

std::string DecodeQuery(std::string_view encoded)
{
    std::string decoded;
    decoded.reserve(encoded.size());
    for (std::size_t index = 0; index < encoded.size(); ++index) {
        const char character = encoded[index];
        if (character == '%') {
            if (index + 2 >= encoded.size()) { throw std::invalid_argument("Incomplete URL escape"); }
            const int highDigit = HexDigit(encoded[index + 1]);
            const int lowDigit = HexDigit(encoded[index + 2]);
            if (highDigit < 0 || lowDigit < 0) { throw std::invalid_argument("Invalid URL escape"); }
            const char decodedCharacter = static_cast<char>(highDigit * 16 + lowDigit);
            if (decodedCharacter == '\0') { throw std::invalid_argument("Null URL character"); }
            decoded += decodedCharacter;
            index += 2;
        } else if (character == '+') {
            decoded += ' ';
        } else {
            decoded += character;
        }
    }
    return decoded;
}

std::unordered_map<std::string, std::string> ParseQuery(std::string_view target)
{
    std::unordered_map<std::string, std::string> query;
    const std::size_t queryStart = target.find('?');
    if (queryStart == std::string_view::npos) { return query; }
    target.remove_prefix(queryStart + 1);
    while (!target.empty()) {
        const std::size_t separator = target.find('&');
        const std::string_view part = target.substr(0, separator);
        const std::size_t equals = part.find('=');
        if (equals == std::string_view::npos) { throw std::invalid_argument("Missing query value"); }
        const std::string key = DecodeQuery(part.substr(0, equals));
        const std::string value = DecodeQuery(part.substr(equals + 1));
        if (!query.emplace(key, value).second) { throw std::invalid_argument("Duplicate query key"); }
        if (separator == std::string_view::npos) { break; }
        target.remove_prefix(separator + 1);
    }
    return query;
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

}

DevelopmentWebPanel& DevelopmentWebPanel::GetInstance()
{
    static DevelopmentWebPanel panel;
    return panel;
}

void DevelopmentWebPanel::NotifySceneChanged()
{
    for (const Registration& registration : registrations_) {
        if (!registration.isScene) { registration.source->OnSceneChanged(); }
    }
}

void DevelopmentWebPanel::BeginScene(const void* owner)
{
    if (owner == nullptr || sceneOwner_ == owner) { return; }
    for (auto iterator = registrations_.begin(); iterator != registrations_.end();) {
        if (iterator->isScene) { iterator = registrations_.erase(iterator); }
        else { ++iterator; }
    }
    sceneOwner_ = owner;
    NotifySceneChanged();
}

bool DevelopmentWebPanel::UnregisterOwner(const void* owner)
{
    if (owner == nullptr) { return false; }
    bool isRemoved = false;
    for (auto iterator = registrations_.begin(); iterator != registrations_.end();) {
        if (iterator->source->GetOwner() == owner) {
            iterator = registrations_.erase(iterator);
            isRemoved = true;
        } else { ++iterator; }
    }
    if (sceneOwner_ == owner) {
        sceneOwner_ = nullptr;
        NotifySceneChanged();
    }
    return isRemoved;
}

bool DevelopmentWebPanel::AddSource(const std::string& id, const std::string& label,
    bool isScene, std::unique_ptr<DevelopmentPanelSource> source)
{
    if (id.empty() || id.find('/') != std::string::npos || source == nullptr) { return false; }
    for (const Registration& registration : registrations_) {
        if (registration.id == id && (!isScene || !registration.isScene ||
                registration.source->GetOwner() == source->GetOwner())) { return false; }
    }
    if (isScene) { BeginScene(source->GetOwner()); }
    registrations_.push_back({id, label, isScene, std::move(source)});
    return true;
}

nlohmann::json DevelopmentWebPanel::GetState() const
{
    nlohmann::json groups = nlohmann::json::array();
    for (const Registration& registration : registrations_) {
        groups.push_back({{"id", registration.id}, {"label", registration.label},
            {"controls", registration.source->GetControls()}, {"values", registration.source->GetState()}});
    }
    return {{"groups", std::move(groups)}};
}

bool DevelopmentWebPanel::ApplyAction(const std::string& key, const std::string& value)
{
    const std::size_t separator = key.find('/');
    if (separator == std::string::npos) { return false; }
    const std::string sourceId = key.substr(0, separator);
    const std::string itemKey = key.substr(separator + 1);
    for (const Registration& registration : registrations_) {
        if (registration.id != sourceId) { continue; }
        const nlohmann::json controls = registration.source->GetControls();
        for (const auto& control : controls) {
            if (control.at("key") != itemKey) { continue; }
            const std::string type = control.at("type");
            if (type == "action") { return registration.source->Execute(itemKey); }
            if (type == "bool") {
                if (value == "true") { return registration.source->SetBool(itemKey, true); }
                if (value == "false") { return registration.source->SetBool(itemKey, false); }
                return false;
            }
            char* numberEnd = nullptr;
            const double number = std::strtod(value.c_str(), &numberEnd);
            if (numberEnd == value.c_str() || *numberEnd != '\0' || !std::isfinite(number)) { return false; }
            if (type == "select") {
                bool isOptionFound = false;
                for (const auto& option : control.at("options")) {
                    if (option.at("value").get<double>() == number) { isOptionFound = true; break; }
                }
                if (!isOptionFound) { return false; }
            } else if (type == "number") {
                const double minimum = control.at("minimum");
                const double maximum = control.at("maximum");
                if (!std::isfinite(minimum) || !std::isfinite(maximum) || number < minimum || number > maximum) { return false; }
            } else { return false; }
            return registration.source->SetNumber(itemKey, number);
        }
        return false;
    }
    return false;
}

std::wstring DevelopmentWebPanel::GetUrl() const
{
    return L"http://127.0.0.1:" + std::to_wstring(port_) + L"/";
}

void DevelopmentWebPanel::QueueResponse(Client& client, int status,
    const std::string& contentType, const std::string& body)
{
    std::string reason = "Error";
    if (status == 200) { reason = "OK"; }
    client.response = "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n";
    client.response += "Content-Type: " + contentType + "\r\n";
    client.response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    client.response += "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n";
    client.response += "Referrer-Policy: no-referrer\r\nConnection: close\r\n\r\n";
    client.response += body;
    client.sentBytes = 0;
    client.request.clear();
}

bool DevelopmentWebPanel::Start()
{
    if (listener_ != static_cast<std::uintptr_t>(kInvalidSocket)) return true;
    WSADATA wsaData {};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
    isWinsockStarted_ = true;
    const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == kInvalidSocket) { StopTransport(); return false; }
    listener_ = static_cast<std::uintptr_t>(listener);

    sockaddr_in address {};
    address.sin_family = AF_INET;
    // ゲームの設定を変更できるため、他のPCからの接続は受け付けません。
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(8765);
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        // 別の開発用ゲームが通常のポートを使用中なら、空いているポートを使います。
        address.sin_port = 0;
        if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            StopTransport();
            return false;
        }
    }
    if (listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        StopTransport();
        return false;
    }
    int addressLength = sizeof(address);
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&address), &addressLength) == SOCKET_ERROR) {
        StopTransport();
        return false;
    }
    port_ = ntohs(address.sin_port);
    u_long nonblocking = 1;
    if (ioctlsocket(listener, FIONBIO, &nonblocking) == SOCKET_ERROR) { StopTransport(); return false; }

    try {
        std::random_device random;
        token_ = std::to_string(random()) + std::to_string(random());
        page_ = LoadPage();
        if (page_.empty()) { StopTransport(); return false; }
        const std::size_t marker = page_.find("__PANEL_TOKEN__");
        if (marker == std::string::npos) { StopTransport(); return false; }
        page_.replace(marker, std::string_view("__PANEL_TOKEN__").size(), token_);
    } catch (const std::exception& error) {
        Logger::Warning(std::string("Development panel startup failed: ") + error.what());
        StopTransport();
        return false;
    }
    const std::wstring url = GetUrl();
    Logger::Log("Development panel: http://127.0.0.1:" + std::to_string(port_) + "/");
    Logger::Flush();
    SetWindowTextW(WinApp::GetInstance()->GetHwnd(),
                   (L"KohakuEngine Development | " + url).c_str());
    // 自動テストなどでは環境変数でブラウザの自動起動を抑制できます。
    isBrowserLaunchPending_ = GetEnvironmentVariableW(L"KOH_DEV_PANEL_NO_BROWSER", nullptr, 0) == 0;
    browserLaunchAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    return true;
}

void DevelopmentWebPanel::Stop()
{
    StopTransport();
    registrations_.clear();
    sceneOwner_ = nullptr;
}

void DevelopmentWebPanel::StopTransport()
{
    isBrowserLaunchPending_ = false;
    page_.clear();
    token_.clear();
    for (Client& client : clients_) closesocket(static_cast<SOCKET>(client.socket));
    clients_.clear();
    if (listener_ != static_cast<std::uintptr_t>(kInvalidSocket)) {
        closesocket(static_cast<SOCKET>(listener_));
        listener_ = static_cast<std::uintptr_t>(kInvalidSocket);
    }
    if (isWinsockStarted_) {
        WSACleanup();
        isWinsockStarted_ = false;
    }
}

void DevelopmentWebPanel::Poll()
{
    if (listener_ == static_cast<std::uintptr_t>(kInvalidSocket)) return;
    if (isBrowserLaunchPending_ && std::chrono::steady_clock::now() >= browserLaunchAt_) {
        isBrowserLaunchPending_ = false;
        const std::wstring url = GetUrl();
        if (reinterpret_cast<std::intptr_t>(ShellExecuteW(
                WinApp::GetInstance()->GetHwnd(), L"open", url.c_str(),
                nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
            Logger::Warning(std::string("Could not open the Development panel in a browser: ") +
                            "http://127.0.0.1:" + std::to_string(port_) + "/");
        }
    }
    const SOCKET listener = static_cast<SOCKET>(listener_);
    for (int attempt = 0; attempt < kMaxAcceptsPerFrame && clients_.size() < kMaxClients; ++attempt) {
        const SOCKET accepted = accept(listener, nullptr, nullptr);
        if (accepted == kInvalidSocket) { break; }
        u_long nonblocking = 1;
        if (ioctlsocket(accepted, FIONBIO, &nonblocking) == SOCKET_ERROR) {
            closesocket(accepted);
            continue;
        }
        Client client;
        client.socket = static_cast<std::uintptr_t>(accepted);
        client.acceptedAt = std::chrono::steady_clock::now();
        clients_.push_back(std::move(client));
    }
    for (std::size_t index = 0; index < clients_.size();) {
        Client& client = clients_[index];
        const SOCKET socket = static_cast<SOCKET>(client.socket);
        bool shouldClose = std::chrono::steady_clock::now() - client.acceptedAt > kClientTimeoutSeconds;
        if (!shouldClose && client.response.empty()) {
            char buffer[4096];
            const int received = recv(socket, buffer, sizeof(buffer), 0);
            if (received > 0) {
                client.request.append(buffer, received);
                if (client.request.size() > kMaxRequestSize) {
                    QueueResponse(client, 413, "text/plain", "Request too large");
                } else if (client.request.find("\r\n\r\n") != std::string::npos) {
                    try { Respond(client); }
                    catch (const std::invalid_argument& error) {
                        QueueResponse(client, 400, "text/plain", error.what());
                    } catch (const std::exception& error) {
                        Logger::Warning(std::string("Development panel request failed: ") + error.what());
                        QueueResponse(client, 500, "text/plain", "Internal server error");
                    }
                }
            } else if (received == 0 || WSAGetLastError() != WSAEWOULDBLOCK) {
                shouldClose = true;
            }
        }
        // 送信待ちでは停止せず、接続ごとに1フレーム1回だけ送信を進めます。
        if (!shouldClose && !client.response.empty()) {
            const std::size_t remainingBytes = client.response.size() - client.sentBytes;
            const int sendBytes = static_cast<int>((std::min)(remainingBytes, kSendChunkBytes));
            const int sentBytes = send(socket, client.response.data() + client.sentBytes, sendBytes, 0);
            if (sentBytes > 0) {
                client.sentBytes += static_cast<std::size_t>(sentBytes);
                shouldClose = client.sentBytes == client.response.size();
            } else if (sentBytes == 0 || WSAGetLastError() != WSAEWOULDBLOCK) {
                shouldClose = true;
            }
        }
        if (shouldClose) {
            closesocket(socket);
            clients_.erase(clients_.begin() + index);
        } else { ++index; }
    }
}

void DevelopmentWebPanel::Respond(Client& client)
{
    const std::size_t lineEnd = client.request.find("\r\n");
    if (lineEnd == std::string::npos) { throw std::invalid_argument("Missing request line"); }
    const std::string_view line(client.request.data(), lineEnd);
    const std::size_t firstSpace = line.find(' ');
    if (firstSpace == std::string_view::npos) { throw std::invalid_argument("Invalid request line"); }
    const std::size_t secondSpace = line.find(' ', firstSpace + 1);
    if (firstSpace == 0 || secondSpace == std::string_view::npos || secondSpace <= firstSpace + 1) {
        throw std::invalid_argument("Invalid request line");
    }
    const std::string_view version = line.substr(secondSpace + 1);
    if (version != "HTTP/1.1" && version != "HTTP/1.0") { throw std::invalid_argument("Invalid HTTP version"); }
    const std::string_view method = line.substr(0, firstSpace);
    const std::string_view target = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    if (method == "GET" && target == "/") {
        QueueResponse(client, 200, "text/html; charset=utf-8", page_);
    } else if (method == "GET" && target == "/api/state") {
        QueueResponse(client, 200, "application/json", GetState().dump());
    } else if (method == "POST" && target.starts_with("/api/action?")) {
        const auto query = ParseQuery(target);
        const auto token = query.find("token");
        if (token == query.end() || token->second != token_) {
            QueueResponse(client, 403, "application/json", "{\"ok\":false,\"error\":\"Invalid token\"}");
            return;
        }
        const auto key = query.find("key");
        const auto value = query.find("value");
        if (key == query.end() || key->second.empty() || value == query.end()) {
            throw std::invalid_argument("Missing action key or value");
        }
        const bool isApplied = ApplyAction(key->second, value->second);
        if (isApplied) { QueueResponse(client, 200, "application/json", "{\"ok\":true}"); }
        else { QueueResponse(client, 400, "application/json", "{\"ok\":false,\"error\":\"Unsupported or invalid action\"}"); }
    } else { QueueResponse(client, 404, "text/plain", "Not found"); }
}

#endif
