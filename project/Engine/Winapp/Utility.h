#pragma once
#include <Windows.h>
#include <dbghelp.h>
#include <iostream>
#include <string>
#include <strsafe.h>
#pragma comment(lib, "dbghelp.lib")
class Utility {
public:
    ////=== UTF-8文字列 -> ワイド文字列への変換===/
    //static std::wstring ConvertString(const std::string& str);
    ////=== ワイド文字列 -> UTF-8文字列への変換 ===
    //static std::string ConvertString(const std::wstring& str);
    ////=== ログ出力関数（コンソールとデバッグ出力）==//
    //static void Log(std::ostream& os, const std::string& message);

    static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);
};
