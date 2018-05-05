#include <windows.h>

int popError(HWND hwnd, UINT mbstyle = MB_OK) {
  const SIZE_T len = 0x400;
  TCHAR str[len];
  DWORD code = GetLastError();
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, 0, str, len, NULL);
  LPTSTR title;
  if (code != NO_ERROR) {
    title = NULL;
    mbstyle |= MB_ICONERROR;
  } else {
    title = TEXT("OK");
    mbstyle |= MB_ICONINFORMATION;
  }
  return MessageBox(hwnd, str, title, mbstyle);
}
