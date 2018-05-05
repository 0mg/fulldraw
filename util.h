#include <windows.h>

LPCTSTR getError() {
  const SIZE_T len = 0x400;
  TCHAR str[len];
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0,
    str, len, NULL);
  return str;
}

int popError(HWND hwnd, UINT mbstyle = MB_OK) {
  const SIZE_T len = 0x400;
  TCHAR str[len];
  DWORD code = GetLastError();
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, 0, str, len, NULL);
  LPTSTR title;
  if (code != 0) {
    title = NULL;
    mbstyle |= MB_ICONERROR;
  } else {
    title = TEXT("OK");
    mbstyle |= MB_ICONINFORMATION;
  }
  return MessageBox(hwnd, str, title, mbstyle);
}
