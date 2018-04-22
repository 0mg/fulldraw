#include <windows.h>

void popError(HWND hwnd = NULL) {
  const SIZE_T len = 0x400;
  TCHAR str[len];
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0,
    str, len, NULL);
  MessageBox(hwnd, str, NULL, MB_OK | MB_ICONERROR);
}

class Array {
public:
  LPVOID data;
  Array(SIZE_T unit, SIZE_T length = 0) {init(unit, length);}
  Array() {}
  int init(SIZE_T unit, SIZE_T length = 0) {
    if (data != NULL && free() != 0) return -1;
    if (allocate(unit * length, 0) == 0) {
      dataUnit = unit;
      dataLength = length;
    }
    return 0;
  }
  int push(LPVOID source) {
    if (sizeof(source) != dataUnit) return -1;
    if (resize(dataUnit * (dataLength + 1))) return -2;
    copy(source, dataUnit * dataLength);
    dataLength++;
    return dataLength;
  }
  int getLength() {
    return dataLength;
  }
  int setLength(SIZE_T newlen) {
    SIZE_T newsize = dataUnit * newlen;
    if (resize(newsize)) {
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
  SIZE_T dataUnit;
  int dataLength;
  int allocate(SIZE_T size, int mode = 0) {
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
  int resize(SIZE_T size) {
    return allocate(size, 1);
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
