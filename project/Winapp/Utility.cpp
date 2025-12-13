#include "Utility.h"

////=== UTF-8文字列 -> ワイド文字列への変換===/
//std::wstring Utility::ConvertString(const std::string& str)
//{
//    if (str.empty()) {
//        return std::wstring();
//    }
//
//    auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]),
//        static_cast<int>(str.size()), NULL, 0);
//    if (sizeNeeded == 0) {
//        return std::wstring();
//    }
//    std::wstring result(sizeNeeded, 0);
//    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]),
//        static_cast<int>(str.size()), &result[0], sizeNeeded);
//    return result;
//}
//
////=== ワイド文字列 -> UTF-8文字列への変換 ===
//std::string Utility::ConvertString(const std::wstring& str)
//{
//    if (str.empty()) {
//        return std::string();
//    }
//
//    auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
//        NULL, 0, NULL, NULL);
//    if (sizeNeeded == 0) {
//        return std::string();
//    }
//    std::string result(sizeNeeded, 0);
//    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
//        result.data(), sizeNeeded, NULL, NULL);
//    return result;
//}
//
////=== ログ出力関数（コンソールとデバッグ出力）==//
//void Utility::Log(std::ostream& os, const std::string& message)
//{
//    os << message << std::endl;
//    OutputDebugStringA(message.c_str());
//}

//===エラーハンドリング用の身にダンプ出力関数===///
LONG WINAPI Utility::ExportDump(EXCEPTION_POINTERS* exception)
{
    // 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下ぶ出力
    SYSTEMTIME time;
    GetLocalTime(&time);
    wchar_t filePath[MAX_PATH] = { 0 };
    CreateDirectory(L"./Dumps", nullptr);
    StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
        time.wYear, time.wMonth, time.wDay, time.wHour,
        time.wMinute);
    HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    // processId(このexeのId)とクラッシュ(例外)の発生したthreadIdを取得
    DWORD processId = GetCurrentProcessId();
    DWORD threadId = GetCurrentThreadId();　
    // 設定情報を入力
    MINIDUMP_EXCEPTION_INFORMATION minidumpInformation { 0 };
    minidumpInformation.ThreadId = threadId;
    minidumpInformation.ExceptionPointers = exception;
    minidumpInformation.ClientPointers = TRUE;
    // Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
    MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle,
        MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
    // 他に関連づけられているSEH例外ハンドラがあれば実行。

    return EXCEPTION_EXECUTE_HANDLER;
}
