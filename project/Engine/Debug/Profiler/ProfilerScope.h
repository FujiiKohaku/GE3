#pragma once
#include <string>

#ifdef _DEBUG
class ProfilerScope {
public:
    explicit ProfilerScope(const std::string& name);
    ~ProfilerScope();
private:
    std::string name_;
};
#else
class ProfilerScope {
public:
    explicit ProfilerScope(const std::string&) {}
    ~ProfilerScope() = default;
};
#endif
