#pragma once
#if defined(ENABLE_DEVELOPMENT_TOOLS)
#include "externals/json.hpp"
#include <cmath>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// 非所有の登録です。対象の破棄前にUnregisterOwnerを呼び出してください。
class DevelopmentPanelSource {
public:
    virtual ~DevelopmentPanelSource() = default;
    virtual const void* GetOwner() const = 0;
    virtual nlohmann::json GetState() const = 0;
    virtual nlohmann::json GetControls() const = 0;
    virtual bool SetBool(const std::string& key, bool value) = 0;
    virtual bool SetNumber(const std::string& key, double value) = 0;
    virtual bool Execute(const std::string& key) = 0;
    virtual void OnSceneChanged() {}
};

template<class T>
class DevelopmentMemberSource final : public DevelopmentPanelSource {
public:
    using Getter = nlohmann::json (T::*)() const;
    using BoolSetter = bool (T::*)(const std::string&, bool);
    using NumberSetter = bool (T::*)(const std::string&, double);
    using Command = bool (T::*)(const std::string&);
    DevelopmentMemberSource(T* owner, Getter stateGetter, Getter controlsGetter,
        BoolSetter boolSetter, NumberSetter numberSetter, Command command,
        void (T::*sceneChanged)())
        : owner_(owner), stateGetter_(stateGetter), controlsGetter_(controlsGetter),
          boolSetter_(boolSetter), numberSetter_(numberSetter), command_(command), sceneChanged_(sceneChanged) {}
    const void* GetOwner() const override { return owner_; }
    nlohmann::json GetState() const override { return (owner_->*stateGetter_)(); }
    nlohmann::json GetControls() const override { return (owner_->*controlsGetter_)(); }
    bool SetBool(const std::string& key, bool value) override {
        if (boolSetter_ == nullptr) { return false; }
        return (owner_->*boolSetter_)(key, value);
    }
    bool SetNumber(const std::string& key, double value) override {
        if (numberSetter_ == nullptr) { return false; }
        return (owner_->*numberSetter_)(key, value);
    }
    bool Execute(const std::string& key) override {
        if (command_ == nullptr) { return false; }
        return (owner_->*command_)(key);
    }
    void OnSceneChanged() override {
        if (sceneChanged_ != nullptr) { (owner_->*sceneChanged_)(); }
    }
private:
    T* owner_;
    Getter stateGetter_;
    Getter controlsGetter_;
    BoolSetter boolSetter_;
    NumberSetter numberSetter_;
    Command command_;
    void (T::*sceneChanged_)();
};

template<class T>
class DevelopmentFloatSource final : public DevelopmentPanelSource {
public:
    DevelopmentFloatSource(T* owner, std::string key, std::string label, float minimum, float maximum,
        float (T::*getter)() const, bool (T::*setter)(float))
        : owner_(owner), key_(std::move(key)), label_(std::move(label)), minimum_(minimum), maximum_(maximum), getter_(getter), setter_(setter) {}
    const void* GetOwner() const override { return owner_; }
    nlohmann::json GetState() const override { return {{key_, (owner_->*getter_)()}}; }
    nlohmann::json GetControls() const override {
        return nlohmann::json::array({{{"key",key_},{"label",label_},{"type","number"},
            {"minimum",minimum_},{"maximum",maximum_},{"step",0.01}}});
    }
    bool SetBool(const std::string&, bool) override { return false; }
    bool SetNumber(const std::string& key, double value) override {
        if (key != key_) { return false; }
        return (owner_->*setter_)(static_cast<float>(value));
    }
    bool Execute(const std::string&) override { return false; }
private:
    T* owner_;
    std::string key_;
    std::string label_;
    float minimum_;
    float maximum_;
    float (T::*getter_)() const;
    bool (T::*setter_)(float);
};

template<class T>
class DevelopmentBoolSource final : public DevelopmentPanelSource {
public:
    DevelopmentBoolSource(T* owner, std::string key, std::string label,
        bool (T::*getter)() const, bool (T::*setter)(bool))
        : owner_(owner), key_(std::move(key)), label_(std::move(label)), getter_(getter), setter_(setter) {}
    const void* GetOwner() const override { return owner_; }
    nlohmann::json GetState() const override { return {{key_, (owner_->*getter_)()}}; }
    nlohmann::json GetControls() const override {
        return nlohmann::json::array({{{"key",key_},{"label",label_},{"type","bool"}}});
    }
    bool SetBool(const std::string& key, bool value) override {
        if (key != key_) { return false; }
        return (owner_->*setter_)(value);
    }
    bool SetNumber(const std::string&, double) override { return false; }
    bool Execute(const std::string&) override { return false; }
private:
    T* owner_;
    std::string key_;
    std::string label_;
    bool (T::*getter_)() const;
    bool (T::*setter_)(bool);
};

// 通信・入力検証・画面生成を担当します。登録と操作はゲームのスレッドで行います。
class DevelopmentWebPanel {
public:
    static DevelopmentWebPanel& GetInstance();
    bool Start();
    void Stop();
    void Poll();
    void BeginScene(const void* owner);
    bool UnregisterOwner(const void* owner);
    template<class T>
    bool RegisterSource(T* owner, const std::string& id, const std::string& label, bool isScene,
        nlohmann::json (T::*stateGetter)() const, nlohmann::json (T::*controlsGetter)() const,
        bool (T::*boolSetter)(const std::string&, bool), bool (T::*numberSetter)(const std::string&, double),
        bool (T::*command)(const std::string&), void (T::*sceneChanged)() = nullptr) {
        if (owner == nullptr || stateGetter == nullptr || controlsGetter == nullptr) { return false; }
        return AddSource(id, label, isScene, std::make_unique<DevelopmentMemberSource<T>>(
            owner, stateGetter, controlsGetter, boolSetter, numberSetter, command, sceneChanged));
    }
    template<class T>
    bool RegisterFloat(T* owner, const std::string& id, const std::string& label, float minimum, float maximum,
        float (T::*getter)() const, bool (T::*setter)(float)) {
        if (owner == nullptr || getter == nullptr || setter == nullptr ||
            !std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum) { return false; }
        return AddSource(id, label, true, std::make_unique<DevelopmentFloatSource<T>>(
            owner, id, label, minimum, maximum, getter, setter));
    }
    template<class T>
    bool RegisterBool(T* owner, const std::string& id, const std::string& label,
        bool (T::*getter)() const, bool (T::*setter)(bool)) {
        if (owner == nullptr || getter == nullptr || setter == nullptr) { return false; }
        return AddSource(id, label, true, std::make_unique<DevelopmentBoolSource<T>>(owner,id,label,getter,setter));
    }
    void SetLegacyUiVisible(bool isVisible) { isLegacyUiVisible_ = isVisible; }
    bool IsLegacyUiVisible() const { return isLegacyUiVisible_; }
private:
    friend class DevelopmentWebPanelTests;
    struct Client {
        std::uintptr_t socket = ~std::uintptr_t {0};
        std::string request;
        std::string response;
        std::size_t sentBytes = 0;
        std::chrono::steady_clock::time_point acceptedAt;
    };
    struct Registration {
        std::string id;
        std::string label;
        bool isScene = false;
        std::unique_ptr<DevelopmentPanelSource> source;
    };
    bool AddSource(const std::string& id, const std::string& label, bool isScene,
        std::unique_ptr<DevelopmentPanelSource> source);
    nlohmann::json GetState() const;
    bool ApplyAction(const std::string& key, const std::string& value);
    void NotifySceneChanged();
    void StopTransport();
    void Respond(Client& client);
    void QueueResponse(Client& client, int status, const std::string& contentType, const std::string& body);
    std::wstring GetUrl() const;
    std::uintptr_t listener_ = ~std::uintptr_t {0};
    std::vector<Client> clients_;
    std::vector<Registration> registrations_;
    const void* sceneOwner_ = nullptr;
    std::uint16_t port_ = 0;
    std::string token_;
    std::string page_;
    bool isWinsockStarted_ = false;
    bool isLegacyUiVisible_ = false;
    bool isBrowserLaunchPending_ = false;
    std::chrono::steady_clock::time_point browserLaunchAt_ {};
};
#endif
