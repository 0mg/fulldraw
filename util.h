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
    float nx = vx * c - h * vy;
    float ny = vy * c + h * vx;
    a[i++] = x1 + r * nx;
    a[i++] = y1 + r * ny;
    a[i++] = x2 + R * nx;
    a[i++] = y2 + R * ny;
    h *= -1;
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

class Array {
public:
  LPVOID data;
  Array(SIZE_T unit, SIZE_T length) {init(unit, length);}
  Array() {}
  int init(SIZE_T unit, SIZE_T length) {
    if (data != NULL && free() != 0) return -1;
    if (allocate(unit * length, MODE_ALLOC) == 0) {
      dataUnit = unit;
      dataLength = length;
    }
    return 0;
  }
  int push(LPVOID source) {
    if (sizeof(source) != dataUnit) return -1;
    if (allocate(dataUnit * (dataLength + 1), MODE_REALLOC)) return -2;
    copy(source, dataUnit * dataLength);
    dataLength++;
    return dataLength;
  }
  int getLength() {
    return dataLength;
  }
  int setLength(SIZE_T newlen) {
    SIZE_T newsize = dataUnit * newlen;
    if (allocate(newsize, MODE_REALLOC)) {
      return dataLength = newsize;
    }
    return -1;
  }
  int free() {
    if (data != NULL) {
      HANDLE heap = GetProcessHeap();
      if (heap == NULL) return -1;
      if (HeapFree(heap, 0, data) == 0) return -2;
    }
    data = NULL;
    dataUnit = 0;
    dataLength = 0;
    return 0;
  }
private:
  enum ALLOC_MODE {MODE_ALLOC, MODE_REALLOC};
  SIZE_T dataUnit;
  int dataLength;
  int allocate(SIZE_T size, ALLOC_MODE mode) {
    HANDLE heap = GetProcessHeap();
    if (heap == NULL) return -1;
    LPVOID p;
    if (mode == 0) {
      p = HeapAlloc(heap, HEAP_ZERO_MEMORY, size);
    } else {
      p = HeapReAlloc(heap, HEAP_ZERO_MEMORY, data, size);
    }
    if (p == NULL) return -2;
    data = p;
    return 0;
  }
  int getSize() {
    HANDLE heap = GetProcessHeap();
    if (heap == NULL) return -1;
    DWORD size = HeapSize(heap, 0, data);
    if (size == -1) return -2;
    dataLength = size / dataUnit;
    return size;
  }
  void copy(LPVOID source, int offset) {
    BYTE *p = ((BYTE *)data + offset), *q = (BYTE *)source;
    for (SIZE_T i = 0; i < dataUnit; i++) {
      *p++ = *q++;
    }
  }
};
