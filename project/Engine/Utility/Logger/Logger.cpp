#include "Logger.h"
#include <Windows.h>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

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
    Write("Log", message);
}

void Logger::Warning(const std::string& message)
{
    Write("Warning", message);
}

void Logger::Error(const std::string& message)
{
    Write("Error", message);
}

void Logger::Write(const std::string& level, const std::string& message)
{
    std::string time = GetTimeString();
    std::string text = "[" + time + "] [" + level + "] " + message;

    std::cout << text << std::endl;
    OutputDebugStringA((text + "\n").c_str());

    if (logFile_.is_open()) {
        logFile_ << text << std::endl;
        logFile_.flush();
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