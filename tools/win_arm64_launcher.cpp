#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
std::wstring sibling(std::wstring_view filename) {
    std::array<wchar_t, 32768> buffer{};
    const DWORD count = GetModuleFileNameW(nullptr, buffer.data(),
                                           static_cast<DWORD>(buffer.size()));
    if (count == 0 || count >= buffer.size()) {
        throw std::runtime_error("cannot locate native Windows ARM64 launcher");
    }
    std::wstring result(buffer.data(), count);
    const auto last = result.find_last_of(L"\\/");
    if (last == std::wstring::npos) {
        throw std::runtime_error("ARM64 Windows launcher has no executable folder");
    }
    result.resize(last + 1);
    result += filename;
    return result;
}

std::wstring quote(std::wstring_view argument) {
    std::wstring encoded{L"\""};
    std::size_t slash = 0;
    for (wchar_t c : argument) {
        if (c == L'\\') {
            ++slash;
        } else if (c == L'"') {
            encoded.append(slash * 2 + 1, L'\\');
            encoded += c;
            slash = 0;
        } else {
            encoded.append(slash, L'\\');
            encoded += c;
            slash = 0;
        }
    }
    encoded.append(slash * 2, L'\\');
    encoded += L'"';
    return encoded;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
#if !defined(_M_ARM64)
#error "This Windows launcher must be built as native ARM64"
#endif
    try {
        auto binary = sibling(L"anyios-win-2048.exe");
        const auto attributes = GetFileAttributesW(binary.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            throw std::runtime_error("x64 emulator-backed guest engine not beside native launcher");
        }
        int count = 0;
        wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (!arguments) throw std::runtime_error("cannot parse native ARM64 command line");
        std::wstring command = quote(binary);
        for (int i = 1; i < count; ++i) {
            command += L" ";
            command += quote(arguments[i]);
        }
        LocalFree(arguments);
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        // Windows 11 ARM64 x64 application emulation executes Dynarmic's x64
        // backend. This launcher is native ARM64; the ARM64 iOS application
        // execution is *translated x64*, NOT natively executing on the CPU.
        if (!CreateProcessW(binary.c_str(), command.data(), nullptr, nullptr,
                            FALSE, 0, nullptr, nullptr, &startup, &process)) {
            throw std::runtime_error("cannot launch x64 AnyiOS guest engine under Windows ARM64 emulation");
        }
        CloseHandle(process.hThread);
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exit_status = 1;
        if (!GetExitCodeProcess(process.hProcess, &exit_status)) exit_status = 2;
        CloseHandle(process.hProcess);
        return static_cast<int>(exit_status);
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "AnyiOS Windows ARM64 launcher",
                    MB_OK | MB_ICONERROR);
        return 2;
    }
}
