#include <windows.h>
#include <math.h>

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

void tanOO(float *tanp, float r, float R, float x1, float y1, float x2, float y2) {
  const SIZE_T len = 8;
  SecureZeroMemory(tanp, sizeof(float) * len);
  float dx = x2 - x1;
  float dy = y2 - y1;
  float dd = dx * dx + dy * dy;
  if (dd <= (r - R) * (r - R)) {
    return;
  }
  float d = sqrtf(dd);
  if (!d) return;
  float vx = dx / d;
  float vy = dy / d;
  float c = (r - R) / d;
  float h = sqrtf(max(0.0f, 1.0f - c * c));
  float a[len];
  for (int i = 0; i < len;) {
    int sign = i < 4 ? 1 : -1;
    float nx = vx * c - sign * h * vy;
    float ny = vy * c + sign * h * vx;
    a[i++] = x1 + r * nx;
    a[i++] = y1 + r * ny;
    a[i++] = x2 + R * nx;
    a[i++] = y2 + R * ny;
  }
  tanp[0] = a[0];
  tanp[1] = a[1];
  tanp[2] = a[2];
  tanp[3] = a[3];
  tanp[4] = a[6];
  tanp[5] = a[7];
  tanp[6] = a[4];
  tanp[7] = a[5];
}
