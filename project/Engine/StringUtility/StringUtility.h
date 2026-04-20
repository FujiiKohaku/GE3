#pragma once
#include <Windows.h>
#include <string>

class StringUtility {
public:
    // string → wstring
    static std::wstring ConvertString(const std::string& str);

    // wstring → string
    static std::string ConvertString(const std::wstring& str);
};
