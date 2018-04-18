#include <windows.h>
// common defs (used by *.cpp and *.rc)
#define C_APPNAME TEXT("fulldraw")
#define C_APPICON 0xA1C0
#define C_APPVER 0,5,7,100
#define C_CTXMENU 0xC07E
#define C_CMD_VERSION 0x5E75
#define C_CMD_REFRESH 0xAB32
#define C_CMD_CLEAR 0x000C
#define C_CMD_MINIMIZE 0x2130
#define C_CMD_SAVEAS 0x5AA5
#define C_CMD_EXIT 0xDEAD
#define C_CMD_DRAW 0xD7A3
#define C_CMD_ERASER 0x37A5
#define C_CMD_PEN_IN 0xAE01
#define C_CMD_PEN_DE 0xAE0F
#define C_CMD_PRS_IN 0xA901
#define C_CMD_PRS_DE 0xA90F

class Array {
private:
  SIZE_T dataUnit;
  int dataLength;
  int allocate(SIZE_T size, int mode) {
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
public:
  LPVOID data;
  int init(SIZE_T size) {
    if (data != NULL && free() != 0) return -1;
    if (allocate(0, 0) == 0) {
      dataUnit = size;
      dataLength = 0;
    }
    return 0;
  }
  int push(LPVOID source, SIZE_T size) {
    if (size != dataUnit) return -1;
    if (resize(dataUnit * (dataLength + 1))) return -2;
    copy(source, dataUnit * dataLength);
    dataLength++;
    return 0;
  }
  int getLength() {
    return dataLength;
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
};
