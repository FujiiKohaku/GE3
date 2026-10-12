#pragma once
#include <functional>
#include <utility>

// Edits a detached snapshot. Only validated changes are merged into the owner.
template<class T> class SettingsEdit {
public:
    using CommitFunction = std::function<bool(const T&, const T&)>;
    SettingsEdit(const T& value, CommitFunction commit) : original_(value), value_(value), commit_(std::move(commit)) {}
    SettingsEdit(const SettingsEdit&) = delete;
    SettingsEdit& operator=(const SettingsEdit&) = delete;
    ~SettingsEdit() { Commit(); }
    T& Get() { return value_; }
    bool Commit() { const bool accepted = commit_(original_, value_); if (accepted) original_ = value_; return accepted; }
private:
    T original_;
    T value_;
    CommitFunction commit_;
};
