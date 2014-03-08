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
      mUiWidth(0),
      mSelectedCandidateIndex(0),
      mVisible(false) {
  this->Rescan();

  createWindowsClass();
  createWindow();
  msHwndToInstance[mHwnd] = this;
}

windmenu::~windmenu() {
  msHwndToInstance.erase(mHwnd);
  if (mBackgroundScanThread.joinable()) {
    mBackgroundScanThread.join();
  }
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
  mUiWidth = GetSystemMetrics(SM_CXSCREEN);
  //const int height = GetSystemMetrics(SM_CYSCREEN);
  mHwnd = CreateWindowEx(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
      mClassName.c_str(),
      mClassName.c_str(),
      WS_POPUP,
      x, y,
      mUiWidth, 24,
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

    case WM_CREATE: {
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

      // draw a background
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

      if (mCommand.size() > 0) {
        SIZE textsize;
        unsigned x = 5;

        // draw user entered command
        ExtTextOut(hdc, x, 5, ETO_CLIPPED, &ps.rcPaint, mCommand.c_str(), mCommand.size(), NULL);
        GetTextExtentPoint32(hdc, mCommand.c_str(), mCommand.size(), &textsize);
        x += textsize.cx;

        // constant offset before drawing candidates so it does not skip around
        x = 200;

        for (size_t i = 0; i < mCommandCandidates.size(); ++i) {
          if (x > mUiWidth) {
            break;
          }

          const auto& candidate = mCommandCandidates[i];

          bool selected = i == mSelectedCandidateIndex;
          if (selected) {
            SetBkColor(hdc, RGB(131, 148, 150));
            SetTextColor(hdc, RGB(0, 43, 54));
          } else {
            SetBkColor(hdc, RGB(0, 43, 54));
            SetTextColor(hdc, RGB(131, 148, 150));
          }

          std::wstring text = candidate.ExeName;
          if (candidate.ShowDirectory) {
            text = candidate.Directory + L"\\" + text;
          }

          // draw candidate command
          ExtTextOut(hdc, x, 5, ETO_CLIPPED, &ps.rcPaint, text.c_str(), text.size(), NULL);
          GetTextExtentPoint32(hdc, text.c_str(), text.size(), &textsize);
          x += textsize.cx;

          // candidate command space
          x += 12;
        }
      }

      DeleteObject(font);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_KEYDOWN: {
      switch (wParam) {
        case VK_BACK: {
          if (mCommand.size() > 0) {
            mCommand.resize(mCommand.size() - 1, L' ');
            updatePossibleCommands();
            InvalidateRect(hwnd, NULL, TRUE);
          }
          break;
        }
        case VK_RETURN: {
          runCommand();
          ToggleVisibility();
          break;
        }
        case VK_TAB: {
          bool shift_down = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
          int dir = 1;
          if (shift_down) {
            dir = -1;
          }
          size_t new_index = mSelectedCandidateIndex + dir;

          if (new_index >= 0 && new_index < mCommandCandidates.size()) {
            mSelectedCandidateIndex = new_index;
            InvalidateRect(hwnd, NULL, TRUE);
          }

          break;
        }
        default: {
          WCHAR buf[4];
          BYTE keyboard_state[256];
          GetKeyboardState(keyboard_state);
          UINT scan_code = (lParam >> 16) & 0xFF;
          if (ToUnicode(wParam, scan_code, keyboard_state, buf, 4, 0) == 1) {
            mCommand += buf[0];
            updatePossibleCommands();
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

void windmenu::Rescan() {
  if (mBackgroundScanThread.joinable()) {
    mBackgroundScanThread.join();
  }
  mBackgroundScanThread = std::thread(&windmenu::rescan, this);
}

void windmenu::rescan() {
  mExecutablesLock.lock();
  mExecutables.clear();
  mExecutablesLock.unlock();

  addExecutablesInEnvironmentVariable(L"PATH", 0);
  addExecutablesInEnvironmentVariable(L"ProgramW6432", 2);
  addExecutablesInEnvironmentVariable(L"ProgramFiles(x86)", 2);
}

void windmenu::addExecutablesInEnvironmentVariable(std::wstring env, int depth) {
  DWORD ret;
  TCHAR buffer[32767];
  ret = GetEnvironmentVariable(env.c_str(), buffer, 32767);
  if (ret) {
    std::wstring path(buffer);
    std::wistringstream ss(path);
    std::wstring token;
    while (std::getline(ss, token, (wchar_t)';')) {
      addExecutablesInDir(token, depth);
    }
  }
}

void windmenu::addExecutablesInDir(std::wstring dir, int depth) {
  print(dir);

  WIN32_FIND_DATA ffd;
  HANDLE hFind = FindFirstFile((dir + L"\\*").c_str(), &ffd);
  if (hFind == INVALID_HANDLE_VALUE) {
    return;
  }

  do {
    std::wstring filename(ffd.cFileName);

    if (filename == L"." || filename == L"..") {
      continue;
    }

    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if ((depth - 1) >= 0) {
        addExecutablesInDir(dir + L"\\" + ffd.cFileName, depth - 1);
      }
      continue;
    }

    std::wstring lowercase_filename(filename);
    CharLowerBuff(&lowercase_filename[0], lowercase_filename.size());
    if (!EndsWith(filename, L".exe")) {
      continue;
    }

    lowercase_filename.resize(lowercase_filename.size() - 4, L' ');
    mExecutablesLock.lock();
    ExecutableInfo& info = mExecutables[lowercase_filename];
    info.Locations.insert(dir);
    info.ExeName = filename;
    mExecutablesLock.unlock();

  } while(FindNextFile(hFind, &ffd) != 0);
}

void windmenu::updatePossibleCommands() {
  mCommandCandidates.clear();
  mSelectedCandidateIndex = 0;

  if (mCommand.size() == 0) {
    return;
  }

  mExecutablesLock.lock();
  auto end_iter = mExecutables.upper_bound(mCommand);
  for (auto iter = mExecutables.lower_bound(mCommand); iter != mExecutables.end(); ++iter) {
    if (!BeginsWith(iter->first, mCommand)) {
      break;
    }

    const std::wstring& exe_name = iter->second.ExeName;
    const auto locations = iter->second.Locations;

    for (auto location = locations.cbegin(); location != locations.end(); ++location) {
      CandidateInfo candidate;
      if (locations.size() == 1) {
        candidate.ShowDirectory = false;
      } else {
        candidate.ShowDirectory = true;
      }
      candidate.Directory = *location;
      candidate.ExeName = exe_name;
      mCommandCandidates.push_back(candidate);
    }
  }
  mExecutablesLock.unlock();
}

void windmenu::runCommand() {
  if (mCommand.size() == 0) {
    return;
  }

  if (mCommand == L"exit") {
    PostQuitMessage(0);
    return;
  } else if (mCommand == L"rescan") {
    Rescan();
    return;
  }

  if (mCommandCandidates.size() == 0) {
    return;
  }

  const CandidateInfo& candidate = mCommandCandidates[mSelectedCandidateIndex];
  std::wstring dir = candidate.Directory;
  std::wstring exe = candidate.ExeName;
  std::wstring path(dir + L"\\" + exe);

  std::wstring args;

  ShellExecute(
      NULL,
      NULL,
      path.c_str(),
      args.c_str(),
      dir.c_str(),
      SW_SHOWDEFAULT);

  mCommand.clear();
  updatePossibleCommands();
}
