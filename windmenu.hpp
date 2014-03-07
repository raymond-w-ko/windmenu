#pragma once

class windmenu {
 public:
  windmenu(HINSTANCE hInstance);
  ~windmenu();

  void Loop();
  void ToggleVisibility();

 protected:
  void createWindowsClass();
  void createWindow();

  static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    windmenu* dmenu = (windmenu*) GetWindowLong(hwnd, GWLP_USERDATA);
    if (!dmenu) {
      return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return dmenu->windowProc(uMsg, wParam, lParam);
  }
  LRESULT windowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

  static std::unordered_map<HWND, windmenu*> msHwndToInstance;

  HINSTANCE mHinstance;
  std::wstring mClassName;
  HWND mHwnd;
  HHOOK mHook;
  bool mVisible;

  std::wstring mCommand;
};
