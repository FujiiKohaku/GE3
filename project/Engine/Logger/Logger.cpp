#include "Logger.h"
#include <Windows.h>
#include <iostream>
#include <filesystem>
#include <ctime>
#include <sstream>
#include <iomanip>
std::ofstream Logger::logFile_;

void Logger::Initialize()
{
    std::filesystem::create_directory("logs");

    std::string time = GetTimeString();

    for (uint32_t index = 0; index < time.size(); index++) {
        if (time[index] == ':') {
            time[index] = '-';
        }

        if (time[index] == ' ') {
            time[index] = '_';
        }
    }

    std::string filePath = "logs/EngineLog_" + time + ".txt";

    logFile_.open(filePath, std::ios::out);
}

void Logger::Finalize()
{
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void Logger::Log(const std::string& message)
{
    std::string time = GetTimeString();
    std::string text = "[" + time + "] [Log] " + message;

    std::cout << text << std::endl;
    OutputDebugStringA((text + "\n").c_str());

    if (logFile_.is_open()) {
        logFile_ << text << std::endl;
    }
}

void Logger::Error(const std::string& message)
{
    std::string time = GetTimeString();
    std::string text = "[" + time + "] [Error] " + message;

    std::cout << text << std::endl;
    OutputDebugStringA((text + "\n").c_str());

    if (logFile_.is_open()) {
        logFile_ << text << std::endl;
    }
}
std::string Logger::GetTimeString()
{
    std::time_t now = std::time(nullptr);

    std::tm localTime {};
    localtime_s(&localTime, &now);

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");

    return oss.str();
}