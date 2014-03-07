#include <StableHeaders.hpp>
#include "windmenu.hpp"

int CALLBACK WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine,
                     int nCmdShow) {
  windmenu dmenu(hInstance);
  dmenu.Loop();
  return 0;
}
