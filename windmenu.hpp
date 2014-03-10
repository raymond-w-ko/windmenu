#pragma once

struct ExecutableInfo {
  std::set<std::wstring> Locations;
  std::wstring ExeName;
};

struct CandidateInfo {
  std::wstring Directory;
  std::wstring ExeName;
  bool ShowDirectory;
};

class windmenu {
 public:
  windmenu(HINSTANCE hInstance);
  ~windmenu();

  /***
   * the main event loop
   */
  void Loop();
  /***
   * show or hide the menu
   */
  void ToggleVisibility();

  /***
   * start a background thread to find and build a list of all relevant executables.
   */
  void Rescan();

 protected:
  // init methods
  void createWindowsClass();
  void createWindow();

  // command processing
  void setForeground();
  void updatePossibleCommands();
  void runCommand();

  // background
  void rescan();
  void addExecutablesInEnvironmentVariable(std::wstring env, int depth);
  void addExecutablesInDir(std::wstring dir, int depth);

  // window callbacks
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

  std::thread mBackgroundScanThread;
  std::mutex mExecutablesLock;
  std::map<std::wstring, ExecutableInfo> mExecutables;

  unsigned mUiWidth;

  std::vector<CandidateInfo> mCommandCandidates;
  int mSelectedCandidateIndex;
};
