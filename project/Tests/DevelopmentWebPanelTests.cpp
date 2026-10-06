#include <winsock2.h>
#include <ws2tcpip.h>
#include "Engine/Development/DevelopmentWebPanel.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

void Require(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}

class PanelTestOwner {
public:
    float GetSpeed() const { return speed_; }
    bool SetSpeed(float speed) {
        if (speed == 9) { throw std::runtime_error("Setter failure for regression test"); }
        speed_ = speed; ++setterCalls_; return true;
    }
    bool IsEnabled() const { return isEnabled_; }
    bool SetEnabled(bool isEnabled) { isEnabled_ = isEnabled; ++setterCalls_; return true; }
    nlohmann::json GetState() const { return {{"value",speed_},{"enabled",isEnabled_}}; }
    nlohmann::json GetControls() const {
        return nlohmann::json::array({
            {{"key","value"},{"label","速度"},{"type","number"},{"minimum",0},{"maximum",10},{"step",0.01}},
            {{"key","enabled"},{"label","有効"},{"type","bool"}},
            {{"key","reset"},{"label","戻す"},{"type","action"}}
        });
    }
    bool SetBool(const std::string& key, bool isEnabled) {
        if (key != "enabled") { return false; }
        return SetEnabled(isEnabled);
    }
    bool SetNumber(const std::string& key, double value) {
        if (key != "value") { return false; }
        return SetSpeed(static_cast<float>(value));
    }
    bool Execute(const std::string& key) {
        if (key != "reset") { return false; }
        return SetSpeed(0);
    }
    void OnSceneChanged() { ++sceneChangedCount_; }
    int GetSetterCalls() const { return setterCalls_; }
    int GetSceneChangedCount() const { return sceneChangedCount_; }
private:
    float speed_ = 0;
    bool isEnabled_ = false;
    int setterCalls_ = 0;
    int sceneChangedCount_ = 0;
};

class DevelopmentWebPanelTests {
public:
    static std::string Request(DevelopmentWebPanel& panel, const std::string& request)
    {
        SOCKET connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Require(connection != INVALID_SOCKET, "Client socket failed");
        sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(panel.port_);
        if (connect(connection, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            closesocket(connection);
            throw std::runtime_error("Client connection failed");
        }
        const int sent = send(connection, request.data(), static_cast<int>(request.size()), 0);
        Require(sent == static_cast<int>(request.size()), "Request send failed");
        u_long nonblocking = 1;
        Require(ioctlsocket(connection, FIONBIO, &nonblocking) != SOCKET_ERROR, "Client nonblocking failed");
        std::string response;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline) {
            panel.Poll();
            char buffer[32768];
            const int received = recv(connection, buffer, sizeof(buffer), 0);
            if (received > 0) { response.append(buffer, received); }
            else if (received == 0) { closesocket(connection); return response; }
            else if (WSAGetLastError() != WSAEWOULDBLOCK) { break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        closesocket(connection);
        throw std::runtime_error("HTTP response timeout");
    }

    static void TestSlowReceiver(DevelopmentWebPanel& panel)
    {
        SOCKET slowReceiver = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Require(slowReceiver != INVALID_SOCKET, "Slow receiver socket failed");
        sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(panel.port_);
        Require(connect(slowReceiver, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != SOCKET_ERROR,
            "Slow receiver connection failed");
        panel.Poll();
        Require(panel.clients_.size() == 1, "Slow receiver was not accepted");
        int sendBufferBytes = 1024;
        const SOCKET serverSocket = static_cast<SOCKET>(panel.clients_[0].socket);
        Require(setsockopt(serverSocket, SOL_SOCKET, SO_SNDBUF,
            reinterpret_cast<const char*>(&sendBufferBytes), sizeof(sendBufferBytes)) != SOCKET_ERROR,
            "Small send buffer failed");
        panel.QueueResponse(panel.clients_[0], 200, "text/plain", std::string(4 * 1024 * 1024, 'x'));
        long long maxPollMicroseconds = 0;
        bool hasBackpressure = false;
        for (int iteration = 0; iteration < 300 && !panel.clients_.empty(); ++iteration) {
            const std::size_t previousBytes = panel.clients_[0].sentBytes;
            const auto begin = std::chrono::steady_clock::now();
            panel.Poll();
            const long long elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - begin).count();
            maxPollMicroseconds = (std::max)(maxPollMicroseconds, elapsed);
            if (!panel.clients_.empty() && panel.clients_[0].sentBytes == previousBytes) {
                hasBackpressure = true;
            }
        }
        Require(hasBackpressure, "Test did not reach send backpressure");
        Require(maxPollMicroseconds < 100000, "Network send blocked the frame");
        std::cout << "Slow receiver: max Poll " << maxPollMicroseconds << " us\n";
        closesocket(slowReceiver);
        panel.Stop();
        Require(panel.Start(), "Restart after slow receiver failed");
    }

    static void Run()
    {
        SetEnvironmentVariableW(L"KOH_DEV_PANEL_NO_BROWSER", L"1");
        DevelopmentWebPanel& panel = DevelopmentWebPanel::GetInstance();
        Require(panel.Start(), "Panel startup failed");
        Require(panel.Start(), "Repeated start failed");
        Require(Request(panel, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n").find("200 OK") != std::string::npos, "Page response failed");
        Require(Request(panel, "GET /api/state HTTP/1.1\r\n\r\n").find("\"groups\":[]") != std::string::npos, "Empty registry failed");
        Require(Request(panel, "invalid\r\n\r\n").find("400 Error") != std::string::npos, "Malformed request accepted");
        const std::string oversized = "GET / HTTP/1.1\r\nX-Large: " + std::string(9000, 'x') + "\r\n\r\n";
        Require(Request(panel, oversized).find("413 Error") != std::string::npos, "Oversized header accepted");
        TestSlowReceiver(panel);
        const std::string token = panel.token_;
        Require(Request(panel, "POST /api/action?token=wrong&key=custom/value&value=1 HTTP/1.1\r\n\r\n").find("403 Error") != std::string::npos, "Invalid token accepted");
        PanelTestOwner oldOwner;
        PanelTestOwner newOwner;
        PanelTestOwner globalOwner;
        Require(panel.RegisterSource(&globalOwner, "global", "共通", false,
            &PanelTestOwner::GetState,&PanelTestOwner::GetControls,&PanelTestOwner::SetBool,
            &PanelTestOwner::SetNumber,&PanelTestOwner::Execute,&PanelTestOwner::OnSceneChanged), "Global registration failed");
        Require(panel.RegisterSource(&oldOwner, "custom", "旧シーン", true,
            &PanelTestOwner::GetState,&PanelTestOwner::GetControls,&PanelTestOwner::SetBool,
            &PanelTestOwner::SetNumber,&PanelTestOwner::Execute), "Old registration failed");
        Require(panel.RegisterSource(&newOwner, "custom", "新シーン", true,
            &PanelTestOwner::GetState,&PanelTestOwner::GetControls,&PanelTestOwner::SetBool,
            &PanelTestOwner::SetNumber,&PanelTestOwner::Execute), "New registration failed");
        Require(!panel.UnregisterOwner(&oldOwner), "Old owner removed new registration");
        Require(panel.sceneOwner_ == &newOwner && panel.registrations_.size() == 2, "Scene replacement lost registrations");
        Require(globalOwner.GetSceneChangedCount() == 2, "Scene change hook failed");
        const std::string prefix = "POST /api/action?token=" + token + "&key=";
        Require(Request(panel, prefix + "custom%2Fvalue&value=1e%2B0 HTTP/1.1\r\n\r\n").find("200 OK") != std::string::npos, "Encoded number failed");
        Require(newOwner.GetSpeed() == 1 && newOwner.GetSetterCalls() == 1, "Registered setter was not called");
        Require(Request(panel, prefix + "custom/enabled&value=true HTTP/1.1\r\n\r\n").find("200 OK") != std::string::npos && newOwner.IsEnabled(), "Bool setter failed");
        Require(Request(panel, prefix + "custom/unknown&value=1 HTTP/1.1\r\n\r\n").find("400 Error") != std::string::npos, "Unknown action accepted");
        Require(Request(panel, prefix + "custom/value&value=%GG HTTP/1.1\r\n\r\n").find("400 Error") != std::string::npos, "Malformed escape accepted");
        Require(Request(panel, prefix + "custom/value&value=nan HTTP/1.1\r\n\r\n").find("400 Error") != std::string::npos, "NaN accepted");
        Require(Request(panel, prefix + "custom/value&value=11 HTTP/1.1\r\n\r\n").find("400 Error") != std::string::npos, "Range violation accepted");
        Require(Request(panel, prefix + "custom/enabled&value=invalid HTTP/1.1\r\n\r\n").find("400 Error") != std::string::npos, "Invalid bool accepted");
        Require(Request(panel, prefix + "custom/value&value=9 HTTP/1.1\r\n\r\n").find("500 Error") != std::string::npos, "Setter exception escaped request handling");
        Require(Request(panel, prefix + "custom/reset&value= HTTP/1.1\r\n\r\n").find("200 OK") != std::string::npos && newOwner.GetSpeed() == 0, "Command setter failed");
        Require(panel.UnregisterOwner(&newOwner), "Current owner detach failed");
        Require(panel.registrations_.size() == 1, "Global registration was removed");
        Require(Request(panel, prefix + "custom/value&value=1 HTTP/1.1\r\n\r\n").find("400 Error") != std::string::npos, "Detached setter was called");
        PanelTestOwner directOwner;
        Require(panel.RegisterFloat(&directOwner,"speed","速度",0,10,&PanelTestOwner::GetSpeed,&PanelTestOwner::SetSpeed), "Direct float registration failed");
        Require(panel.RegisterBool(&directOwner,"enabled","有効",&PanelTestOwner::IsEnabled,&PanelTestOwner::SetEnabled), "Direct bool registration failed");
        Require(!panel.RegisterFloat(&directOwner,"bad","不正",10,0,&PanelTestOwner::GetSpeed,&PanelTestOwner::SetSpeed), "Invalid registration range accepted");
        Require(Request(panel, prefix + "speed/speed&value=2.5 HTTP/1.1\r\n\r\n").find("200 OK") != std::string::npos && directOwner.GetSpeed() == 2.5f, "Direct setter failed");
        Require(panel.GetState()["groups"].size() == 3, "Schema output failed");
        panel.UnregisterOwner(&directOwner);
        panel.UnregisterOwner(&globalOwner);
        panel.Stop();
        Require(panel.Start(), "Panel restart failed");
        panel.Stop();
        std::cout << "DevelopmentWebPanel tests passed: real HTTP, size limit, malformed requests, token, decoded values, typed setters, metadata, scene ownership, persistent groups, restart\n";
    }
};

int main()
{
    try { DevelopmentWebPanelTests::Run(); }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        DevelopmentWebPanel::GetInstance().Stop();
        return 1;
    }
    return 0;
}
