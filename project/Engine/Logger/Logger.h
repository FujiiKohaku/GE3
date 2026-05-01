#pragma once
#include <fstream>
#include <string>

class Logger {
public:
    static void Initialize();
    static void Finalize();

    static void Log(const std::string& message);
    static void Error(const std::string& message);
    static std::string GetTimeString();

private:
    static std::ofstream logFile_;
};