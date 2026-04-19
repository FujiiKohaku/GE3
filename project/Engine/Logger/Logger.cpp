#include "Logger.h"
#include <Windows.h>
#include <iostream>

void Logger::Log(const std::string& message)
{
    std::cout << message << std::endl;
    OutputDebugStringA((message + "\n").c_str());
}
