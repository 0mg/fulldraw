#include <windows.h>

LPCTSTR getError() {
  const SIZE_T len = 0x400;
  TCHAR str[len];
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0,
    str, len, NULL);
  return str;
}

void popError(HWND hwnd = NULL) {
  const SIZE_T len = 0x400;
  TCHAR str[len];
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0,
    str, len, NULL);
  MessageBox(hwnd, str, NULL, MB_OK | MB_ICONERROR);
}
