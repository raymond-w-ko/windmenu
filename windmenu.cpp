#include "StableHeaders.hpp"
#include "windmenu.hpp"

std::unordered_map<HWND, windmenu*> windmenu::msHwndToInstance;
#define WM_DELAYED_SET_FOCUS WM_USER+1

LRESULT CALLBACK windmenu::LowLevelKeyboardProc(
    int nCode,
    WPARAM wParam,
    LPARAM lParam) {
  if (nCode < 0) {
    return CallNextHookEx(NULL, nCode, wParam, lParam);
  }

  const KBDLLHOOKSTRUCT* key_info = (const KBDLLHOOKSTRUCT*)lParam;

  static bool eat_next_alt = false;

  if (key_info->vkCode == 'D' &&
      (GetAsyncKeyState(VK_MENU) & 0x8000) != 0) {
    if (wParam == WM_SYSKEYDOWN) {
      eat_next_alt = true;
      return 1;
    } else if (wParam == WM_SYSKEYUP) {
      return 1;
    }
  }

  if (eat_next_alt && key_info->vkCode == VK_LMENU) {
    eat_next_alt = false;
    for (auto iter : windmenu::msHwndToInstance) {
      windmenu* dmenu = iter.second;
      dmenu->ToggleVisibility();
    }
  }

  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

windmenu::windmenu(HINSTANCE hInstance)
    : mHinstance(hInstance),
      mClassName(L"windmenu"),
      mHwnd(NULL),
      mHook(NULL),
      mVisible(false) {
  createWindowsClass();
  createWindow();
  msHwndToInstance[mHwnd] = this;
}

windmenu::~windmenu() {
  msHwndToInstance.erase(mHwnd);
}

void windmenu::createWindowsClass() {
  WNDCLASSEX wnd_class;
  wnd_class.cbSize = sizeof(wnd_class);
  wnd_class.style = 0;
  wnd_class.lpfnWndProc = &windmenu::WindowProc;
  wnd_class.cbClsExtra = 0;
  wnd_class.cbWndExtra = 0;
  wnd_class.hInstance = mHinstance;
  // TODO: use specific resources to replace placeholder generic ones
  wnd_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wnd_class.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
  wnd_class.hCursor = LoadCursor(NULL, IDC_ARROW);
  wnd_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wnd_class.lpszMenuName = NULL;
  wnd_class.lpszClassName = mClassName.c_str();

  RegisterClassEx(&wnd_class);
}

void windmenu::createWindow() {
  const int x = 0;
  const int y = 0;
  const int width = GetSystemMetrics(SM_CXSCREEN);
  //const int height = GetSystemMetrics(SM_CYSCREEN);
  mHwnd = CreateWindowEx(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
      mClassName.c_str(),
      mClassName.c_str(),
      WS_POPUP,
      x, y,
      width, 24,
      NULL,
      NULL,
      mHinstance,
      NULL);
  if (mHwnd == NULL) {
    throw std::exception();
  }
  SetWindowLongPtr(mHwnd, GWLP_USERDATA, (LONG_PTR)this);

  mHook = SetWindowsHookEx(WH_KEYBOARD_LL, windmenu::LowLevelKeyboardProc, mHinstance, 0);
}

void windmenu::Loop() {
  MSG msg = {};
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

LRESULT windmenu::windowProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  const HWND hwnd = mHwnd;

  switch(uMsg) {
    case WM_CLOSE: {
      return 0;
    }
    case WM_DESTROY: {
      UnhookWindowsHookEx(mHook);
      mHook = NULL;

      PostQuitMessage(0);
      return 0;
    }

    case WM_DELAYED_SET_FOCUS: {
      SetForegroundWindow(hwnd);
      SetFocus(hwnd);
      SetActiveWindow(hwnd);
      return 0;

    }

    case WM_ERASEBKGND: {
      return 1;
    }
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);

      // draw a black background
      HBRUSH background_brush = CreateSolidBrush(RGB(0, 43, 54));
      FillRect(hdc, &ps.rcPaint, background_brush);
      DeleteObject(background_brush);

      // draw current command
      HFONT font = CreateFont(0, 0, 0, 0, 0,
                              FALSE, FALSE, FALSE,
                              ANSI_CHARSET,
                              OUT_RASTER_PRECIS,
                              CLIP_DEFAULT_PRECIS,
                              NONANTIALIASED_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE,
                              L"fixed613");

      SelectObject(hdc, font);
      SetBkColor(hdc, RGB(0, 43, 54));
      SetTextColor(hdc, RGB(131, 148, 150));
      ExtTextOut(hdc, 0, 0, ETO_CLIPPED, &ps.rcPaint, mCommand.c_str(), mCommand.size(), NULL);

      DeleteObject(font);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_KEYDOWN: {
      switch (wParam) {
        case VK_BACK: {
          break;
        }
        case VK_RETURN: {
          ToggleVisibility();
          break;
        }
        default: {
          WCHAR buf[4];
          BYTE keyboard_state[256];
          GetKeyboardState(keyboard_state);
          UINT scan_code = (lParam >> 16) & 0xFF;
          if (ToUnicode(wParam, scan_code, keyboard_state, buf, 4, 0) == 1) {
            mCommand += buf[0];
            InvalidateRect(hwnd, NULL, TRUE);
          }

          break;
        }
      }
      return 0;
    }
    default:
      break;
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void windmenu::ToggleVisibility() {
  mVisible = !mVisible;
  ShowWindow(mHwnd, mVisible ? SW_SHOW : SW_HIDE);
  if (mVisible) {
    PostMessage(mHwnd, WM_DELAYED_SET_FOCUS, 0, 0);
  }
}
