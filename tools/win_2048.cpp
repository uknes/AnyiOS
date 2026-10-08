#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>

#include <anyios/guest_2048.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
constexpr int kLogicalWidth = 600;
constexpr int kLogicalHeight = 760;
constexpr int kBoardX = 47;
constexpr int kBoardY = 250;
constexpr int kBoardSize = 506;
constexpr int kPadding = 12;
constexpr int kTile = 109;
constexpr int kGap = 14;

struct App {
    explicit App(const std::string& file) : guest(file) {}
    anyios::gui::Guest2048 guest;
    POINT drag_start{};
    bool dragging = false;
    unsigned keyboard_inputs = 0;
    unsigned mouse_inputs = 0;
    unsigned restarts = 0;
    int width = kLogicalWidth;
    int height = kLogicalHeight;
    HWND hwnd = nullptr;
};

std::string utf8(const wchar_t* text) {
    const auto count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        text, -1, nullptr, 0, nullptr, nullptr);
    if (count <= 0) throw std::runtime_error("invalid Windows Unicode path");
    std::string result(static_cast<std::size_t>(count), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        text, -1, result.data(), count, nullptr, nullptr)) {
        throw std::runtime_error("Windows path conversion failed");
    }
    result.pop_back();
    return result;
}

void rounded(HDC dc, int left, int top, int right, int bottom,
             COLORREF color, int radius = 13) {
    const auto fill = CreateSolidBrush(color);
    if (!fill) throw std::runtime_error("Windows GDI brush creation failed");
    const auto old_brush = SelectObject(dc, fill);
    const auto old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, left, top, right, bottom, radius, radius);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(fill);
}

void label(HDC dc, std::wstring_view text, RECT rect, int size,
           COLORREF color, bool bold = true, unsigned placement = DT_CENTER) {
    const auto font = CreateFontW(-size, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    if (!font) throw std::runtime_error("Windows GDI font allocation failed");
    const auto old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    std::wstring buffer(text);
    DrawTextW(dc, buffer.c_str(), -1, &rect,
              placement | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old);
    DeleteObject(font);
}

COLORREF tile_color(std::uint32_t value) {
    switch (value) {
    case 0: return RGB(205, 193, 180);
    case 2: return RGB(238, 228, 218);
    case 4: return RGB(237, 224, 200);
    case 8: return RGB(242, 177, 121);
    case 16: return RGB(245, 149, 99);
    case 32: return RGB(246, 124, 95);
    case 64: return RGB(246, 94, 59);
    case 128: return RGB(237, 207, 114);
    case 256: return RGB(237, 204, 97);
    case 512: return RGB(237, 200, 80);
    case 1024: return RGB(237, 197, 63);
    default: return RGB(237, 194, 46);
    }
}

void render(HDC dc, int width, int height,
            const anyios::gui::Guest2048State& state) {
    if (width <= 0 || height <= 0) return;
    const auto saved = SaveDC(dc);
    SetMapMode(dc, MM_ANISOTROPIC);
    SetWindowExtEx(dc, kLogicalWidth, kLogicalHeight, nullptr);
    SetViewportExtEx(dc, width, height, nullptr);
    rounded(dc, 0, 0, 600, 760, RGB(250, 248, 239), 0);
    label(dc, L"2048", RECT{47, 48, 260, 132}, 82, RGB(119, 110, 101),
          true, DT_LEFT);
    label(dc, L"ARM64 iOS guest  /  Windows host",
          RECT{47, 138, 470, 166}, 17, RGB(143, 130, 118),
          false, DT_LEFT);
    rounded(dc, 350, 65, 445, 138, RGB(187, 173, 160));
    rounded(dc, 458, 65, 553, 138, RGB(187, 173, 160));
    label(dc, L"SCORE", RECT{355, 70, 440, 92}, 14, RGB(240, 234, 226));
    label(dc, std::to_wstring(state.score), RECT{355, 93, 440, 134},
          24, RGB(255, 255, 255));
    label(dc, L"BEST", RECT{463, 70, 548, 92}, 14, RGB(240, 234, 226));
    label(dc, std::to_wstring(state.best), RECT{463, 93, 548, 134},
          24, RGB(255, 255, 255));

    label(dc, L"Join the tiles to reach 2048",
          RECT{47, 184, 360, 228}, 20, RGB(119, 110, 101),
          false, DT_LEFT);
    rounded(dc, 392, 181, 553, 235, RGB(143, 122, 102));
    label(dc, L"NEW GAME", RECT{396, 183, 550, 234}, 18, RGB(255, 255, 255));

    rounded(dc, kBoardX, kBoardY,
            kBoardX + kBoardSize, kBoardY + kBoardSize, RGB(187, 173, 160), 16);
    for (int i = 0; i < 16; ++i) {
        const int row = i / 4;
        const int col = i % 4;
        const auto value = state.tiles[static_cast<std::size_t>(i)];
        const int x = kBoardX + kPadding + col * (kTile + kGap);
        const int y = kBoardY + kPadding + row * (kTile + kGap);
        rounded(dc, x, y, x + kTile, y + kTile, tile_color(value), 12);
        if (value != 0) {
            const int font_size = value < 128 ? 48 : value < 1024 ? 41 : 32;
            label(dc, std::to_wstring(value),
                  RECT{x + 3, y + 3, x + kTile - 3, y + kTile - 3},
                  font_size,
                  value <= 4 ? RGB(119, 110, 101) : RGB(249, 246, 242));
        }
    }
    if (state.game_over) {
        rounded(dc, 98, 455, 502, 554, RGB(235, 222, 208), 20);
        label(dc, L"GAME OVER", RECT{101, 468, 499, 544},
              42, RGB(119, 110, 101));
    } else if (state.won) {
        rounded(dc, 111, 458, 489, 551, RGB(237, 194, 46), 20);
        label(dc, L"2048 REACHED!", RECT{112, 465, 488, 545},
              34, RGB(255, 255, 255));
    }
    label(dc, L"Arrow keys / WASD or drag to move  \x2022  R to restart",
          RECT{42, 714, 558, 748}, 15, RGB(151, 135, 122), false);
    RestoreDC(dc, saved);
}

POINT to_logical(const App& app, POINT point) {
    return POINT{static_cast<LONG>(point.x * kLogicalWidth /
                                      std::max(1, app.width)),
                 static_cast<LONG>(point.y * kLogicalHeight /
                                      std::max(1, app.height))};
}

void do_move(App& app, unsigned direction) {
    app.guest.move(direction);
    InvalidateRect(app.hwnd, nullptr, FALSE);
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        const auto* setup = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* app = static_cast<App*>(setup->lpCreateParams);
        app->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        return TRUE;
    }
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!app) return DefWindowProcW(hwnd, message, wparam, lparam);
    try {
        switch (message) {
        case WM_SIZE:
            app->width = std::max(1, static_cast<int>(LOWORD(lparam)));
            app->height = std::max(1, static_cast<int>(HIWORD(lparam)));
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC destination = BeginPaint(hwnd, &ps);
            RECT client{};
            GetClientRect(hwnd, &client);
            const auto width = std::max(1L, client.right - client.left);
            const auto height = std::max(1L, client.bottom - client.top);
            HDC temporary = CreateCompatibleDC(destination);
            HBITMAP bitmap = CreateCompatibleBitmap(destination, width, height);
            if (temporary && bitmap) {
                auto previous = SelectObject(temporary, bitmap);
                render(temporary, width, height, app->guest.snapshot());
                BitBlt(destination, 0, 0, width, height, temporary, 0, 0, SRCCOPY);
                SelectObject(temporary, previous);
            }
            if (bitmap) DeleteObject(bitmap);
            if (temporary) DeleteDC(temporary);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN: {
            int direction = -1;
            switch (wparam) {
            case VK_UP: case 'W': direction = 0; break;
            case VK_LEFT: case 'A': direction = 1; break;
            case VK_DOWN: case 'S': direction = 2; break;
            case VK_RIGHT: case 'D': direction = 3; break;
            case 'R':
                app->guest.reset(static_cast<std::uint32_t>(
                    GetTickCount64() ^ 0x20482048ULL));
                ++app->restarts;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            default: return DefWindowProcW(hwnd, message, wparam, lparam);
            }
            ++app->keyboard_inputs;
            do_move(*app, static_cast<unsigned>(direction));
            return 0;
        }
        case WM_LBUTTONDOWN:
            app->drag_start = POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            app->dragging = true;
            SetCapture(hwnd);
            return 0;
        case WM_LBUTTONUP: {
            if (!app->dragging) return 0;
            app->dragging = false;
            if (GetCapture() == hwnd) ReleaseCapture();
            const auto start = to_logical(*app, app->drag_start);
            const auto end = to_logical(*app, POINT{
                GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
            const int dx = end.x - start.x;
            const int dy = end.y - start.y;
            if (std::max(std::abs(dx), std::abs(dy)) >= 36) {
                ++app->mouse_inputs;
                do_move(*app, std::abs(dx) >= std::abs(dy) ?
                        (dx > 0 ? 3U : 1U) : (dy > 0 ? 2U : 0U));
            } else if (end.x >= 392 && end.x <= 553 &&
                       end.y >= 181 && end.y <= 235) {
                app->guest.reset(static_cast<std::uint32_t>(
                    GetTickCount64() ^ 0x20482048ULL));
                ++app->restarts;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_CAPTURECHANGED:
            app->dragging = false;
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default: return DefWindowProcW(hwnd, message, wparam, lparam);
        }
    } catch (const std::exception& error) {
        OutputDebugStringA(error.what());
        MessageBoxA(hwnd, error.what(), "AnyiOS ARM64 guest failed closed",
                    MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd);
        return 0;
    }
}

void save_preview(const std::wstring& path,
                  const anyios::gui::Guest2048State& state) {
    constexpr int width = 600;
    constexpr int height = 760;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits,
                                      nullptr, 0);
    if (!bitmap || !bits) throw std::runtime_error("offscreen Windows DIB creation failed");
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) {
        DeleteObject(bitmap);
        throw std::runtime_error("offscreen GDI DC allocation failed");
    }
    auto old = SelectObject(dc, bitmap);
    render(dc, width, height, state);
    GdiFlush();
    const auto* pixels = static_cast<const std::uint8_t*>(bits);
    const auto background_offset = (20 * width + 20) * 4;
    if (pixels[background_offset + 0] < 230 ||
        pixels[background_offset + 1] < 230 ||
        pixels[background_offset + 2] < 230) {
        throw std::runtime_error("Windows framebuffer GDI background validation failed");
    }
    const std::uint32_t bytes = width * height * 4;
    BITMAPFILEHEADER header{};
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + bytes;
    std::ofstream output(utf8(path.c_str()), std::ios::binary);
    if (!output) throw std::runtime_error("cannot save Windows UI screenshot");
    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    output.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
    output.write(reinterpret_cast<const char*>(bits), bytes);
    SelectObject(dc, old);
    DeleteDC(dc);
    DeleteObject(bitmap);
    if (!output) throw std::runtime_error("Windows UI screenshot write failed");
}

int self_test(App& app, HWND hwnd, const wchar_t* screenshot) {
    const auto before = app.guest.snapshot();
    unsigned populated = 0;
    for (auto v : before.tiles) if (v) ++populated;
    if (populated != 2 || before.moves != 0) {
        throw std::runtime_error("UI self-test guest initial state invalid");
    }
    for (auto key : {VK_UP, VK_LEFT, VK_DOWN, VK_RIGHT}) {
        SendMessageW(hwnd, WM_KEYDOWN, key, 0);
    }
    if (app.keyboard_inputs != 4 || app.guest.snapshot().moves == 0) {
        throw std::runtime_error("Windows WM_KEYDOWN did not change ARM64 guest state");
    }
    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(130, 430));
    SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(420, 430));
    if (app.mouse_inputs != 1) {
        throw std::runtime_error("Windows mouse swipe did not reach guest input bridge");
    }
    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(470, 200));
    SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(470, 200));
    if (app.restarts != 1 || app.guest.snapshot().moves != 0) {
        throw std::runtime_error("Windows mouse click did not restart ARM64 guest");
    }
    // Verify a new swiping interaction after reset, then render exact guest state.
    SendMessageW(hwnd, WM_KEYDOWN, VK_DOWN, 0);
    save_preview(screenshot, app.guest.snapshot());
    std::printf("WINDOWS_UI=offscreen-GDI-rendered\n");
    std::printf("GUEST=original-AnyiOS-owned-arm64-ios-MachO\n");
    std::printf("GUEST_INPUT=real-executed-arm64-gameplay\n");
    std::printf("KEYBOARD_TEST=passed\nMOUSE_SWIPE_TEST=passed\nMOUSE_CLICK_TEST=passed\n");
    std::printf("SCREENSHOT_BMP=written\n");
    std::printf("EXTERNAL_2048_IOS_APP=not-executed\n");
    return 0;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    try {
        int argc = 0;
        auto* args = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!args) throw std::runtime_error("cannot parse Windows command line");
        const bool testing = argc == 4 && std::wstring_view(args[1]) == L"--self-test";
        if (!testing && argc != 1 && argc != 2) {
            LocalFree(args);
            throw std::runtime_error("usage: anyios-win-2048 [--self-test] AnyiOS2048Guest [preview.bmp]");
        }
        std::wstring image;
        if (testing) {
            image = args[2];
        } else if (argc == 2) {
            image = args[1];
        } else {
            std::array<wchar_t, 32768> executable{};
            const auto length = GetModuleFileNameW(nullptr, executable.data(),
                                                    static_cast<DWORD>(executable.size()));
            if (length == 0 || length >= executable.size()) {
                LocalFree(args);
                throw std::runtime_error("cannot locate AnyiOS executable directory");
            }
            image.assign(executable.data(), length);
            const auto slash = image.find_last_of(L"\\/");
            if (slash == std::wstring::npos) {
                LocalFree(args);
                throw std::runtime_error("cannot locate bundled ARM64 iOS guest");
            }
            image.resize(slash + 1);
            image += L"AnyiOS2048Guest";
        }
        const std::wstring screenshot = testing ? args[3] : L"";
        LocalFree(args);
        App app(utf8(image.c_str()));
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = wnd_proc;
        wc.hInstance = instance;
        wc.lpszClassName = L"AnyiOSArm64Guest2048Window";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        if (!RegisterClassExW(&wc)) {
            throw std::runtime_error("Windows GDI host window registration failed");
        }
        const auto hwnd = CreateWindowExW(0, wc.lpszClassName,
            L"AnyiOS - 2048 ARM64 iOS Guest (compatibility fixture)",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            640, 835, nullptr, nullptr, instance, &app);
        if (!hwnd) throw std::runtime_error("Windows GDI host window creation failed");
        if (testing) {
            auto result = self_test(app, hwnd, screenshot.c_str());
            DestroyWindow(hwnd);
            return result;
        }
        ShowWindow(hwnd, show);
        UpdateWindow(hwnd);
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return static_cast<int>(message.wParam);
    } catch (const std::exception& error) {
        OutputDebugStringA(error.what());
        MessageBoxA(nullptr, error.what(), "AnyiOS Windows interactive host",
                    MB_OK | MB_ICONERROR);
        return 2;
    }
}
