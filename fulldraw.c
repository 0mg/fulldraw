#include <windows.h>
#include <msgpack.h>
#include <wintab.h>
#define PACKETDATA PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE | PK_CURSOR
#define PACKETMODE PK_BUTTONS
#include <pktdef.h>

#define C_APPNAME TEXT("fulldraw")
#define C_WINDOW_CLASS TEXT("fulldraw_window_class")
#define C_SCWIDTH GetSystemMetrics(SM_CXSCREEN)
#define C_SCHEIGHT GetSystemMetrics(SM_CYSCREEN)

LONG nn;
TCHAR ss[255];
void mbox(LPTSTR str) {
  MessageBox(NULL, str, str, MB_OK);
}
void mboxn(LONG n) {
  TCHAR str[99];
  wsprintf(str, TEXT("%d"), n);
  MessageBox(NULL, str, str, MB_OK);
}
void tou(HWND hwnd, HDC hdc, LPTSTR str) {
  RECT rect;
  rect.left = rect.top = 0;
  rect.right = 1000;
  rect.bottom = 30;
  FillRect(hdc, &rect, GetStockObject(WHITE_BRUSH));
  TextOut(hdc, 0, 0, str, lstrlen(str));
  InvalidateRect(hwnd, NULL, FALSE);
}
void toun(HWND hwnd, HDC hdc, int n) {
  TCHAR str[255];
  wsprintf(str, TEXT("%d            "), n);
  TextOut(hdc, 0, 0, str, lstrlen(str));
  InvalidateRect(hwnd, NULL, FALSE);
}

void __start__() {
  // program will start from here if `gcc -nostartfiles`
  ExitProcess(WinMain(GetModuleHandle(NULL), 0, "", 0));
}

typedef UINT (*typeWTInfoW)(UINT, UINT, LPVOID);
typedef HCTX (*typeWTOpenW)(HWND, LPLOGCONTEXTW, BOOL);
typedef BOOL (*typeWTClose)(HCTX);
typedef BOOL (*typeWTPacket)(HCTX, UINT, LPVOID);
typedef BOOL (*typeWTQueuePacketsEx)(HCTX, UINT FAR *, UINT FAR *);
typedef int (*typeWTDataGet)(HCTX, UINT, UINT, int, LPVOID, LPINT);
typedef	int (*typeWTQueueSizeGet)(HCTX);

typedef struct {
  typeWTInfoW WTInfoW;
  typeWTOpenW WTOpenW;
  typeWTClose WTClose;
  typeWTPacket WTPacket;
  typeWTQueuePacketsEx WTQueuePacketsEx;
  typeWTDataGet WTDataGet;
  typeWTQueueSizeGet WTQueueSizeGet;
} WintabFunctions;

HINSTANCE LoadWintab32(WintabFunctions *wt) {
  HINSTANCE dll;
  dll = LoadLibrary(TEXT("wintab32.dll"));
  if (dll == NULL) {
    return NULL;
  }
  wt->WTInfoW = (typeWTInfoW)GetProcAddress(dll, "WTInfoW");
  wt->WTOpenW = (typeWTOpenW)GetProcAddress(dll, "WTOpenW");
  wt->WTClose = (typeWTClose)GetProcAddress(dll, "WTClose");
  wt->WTPacket = (typeWTPacket)GetProcAddress(dll, "WTPacket");
  wt->WTQueuePacketsEx = (typeWTQueuePacketsEx)GetProcAddress(dll, "WTQueuePacketsEx");
  wt->WTDataGet = (typeWTDataGet)GetProcAddress(dll, "WTDataGet");
  wt->WTQueueSizeGet = (typeWTQueueSizeGet)GetProcAddress(dll, "WTQueueSizeGet");
  return dll;
}

LRESULT CALLBACK mainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  static BOOL drawing;
  static INT16 x, y, oldx, oldy;
  static HDC hdc;
  static HBITMAP bmp;
  // Wintab
  static WintabFunctions wt;
  static HINSTANCE wintab32dll;
  static LOGCONTEXT lcMine;
  static HCTX wtctx;
  // debug
  static UINT nums[17];
  static UINT idx;
  static TCHAR str[255];
  if (!(msg & (WM_PAINT))) {
    nums[idx++ % 17] = msg;
  }
  {
    wvsprintf(str, TEXT("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u                  "), nums);
    //TextOut(hdc, 0, 0, str, lstrlen(str));
    //InvalidateRect(hwnd, NULL, FALSE);
  }
  switch (msg) {
  case WM_CREATE: {
    // init vars
    drawing = FALSE;
    // load Wintab32.dll
    wintab32dll = LoadWintab32(&wt);
    // init wintab
    if (wintab32dll != NULL) {
      if (wt.WTInfoW(WTI_DEFCONTEXT, 0, &lcMine) != 0) {
        lcMine.lcOptions |= CXO_MESSAGES;
        lcMine.lcMsgBase = WT_DEFBASE;
        lcMine.lcPktData = PACKETDATA;
        lcMine.lcPktMode = PACKETMODE;
        lcMine.lcMoveMask = PACKETDATA;
        lcMine.lcBtnUpMask = lcMine.lcBtnDnMask;
        lcMine.lcOutOrgX = 0;
        lcMine.lcOutOrgY = 0;
        lcMine.lcOutExtX = GetSystemMetrics(SM_CXSCREEN);
        lcMine.lcOutExtY = -GetSystemMetrics(SM_CYSCREEN);
        wtctx = wt.WTOpenW(hwnd, &lcMine, TRUE);
        //wt.WTClose(wtctx);
      } else {
        wtctx = NULL;
      }
    }
    // timer
    //SetTimer(hwnd, 0, 10, NULL);
    // ready BMP and paint Background
    RECT rect;
    HDC odc = GetDC(hwnd);
    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    hdc = CreateCompatibleDC(odc);
    bmp = CreateCompatibleBitmap(odc, C_SCWIDTH, C_SCHEIGHT);
    rect.left = 0;
    rect.top = 0;
    rect.right = C_SCWIDTH;
    rect.bottom = C_SCHEIGHT;
    SelectObject(hdc, bmp);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
    ReleaseDC(hwnd, odc);
    return 0;
  }
  case WM_MOUSELEAVE: {
    ReleaseCapture();
    return 0;
  }
  case WT_PACKET: {
    // wintab
    if (wtctx != NULL) {
      // wintab packets handler
      UINT FAR oldest;
      UINT FAR newest;
      PACKET pkt;
      // get all queues' oldest to newest
      wt.WTQueuePacketsEx(wtctx, &oldest, &newest);
      // get newest queue
      wt.WTPacket(wtctx, newest, &pkt);
      wsprintf(ss, TEXT("%d, %d, %d, %d"), pkt.pkX, pkt.pkY, pkt.pkNormalPressure, pkt.pkCursor);
      
      tou(hwnd, hdc, ss);
    }
    return 0;
  }
  case WM_TIMER: {
    return 0;
  }
  case WM_ERASEBKGND: {
    return 1;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC odc = BeginPaint(hwnd, &ps);
    BitBlt(odc, 0, 0, C_SCWIDTH, C_SCHEIGHT, hdc, 0, 0, SRCCOPY);
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_MOUSEMOVE: {
    // init
    HPEN pen;
    oldx = x;
    oldy = y;
    x = LOWORD(lp);
    y = HIWORD(lp);
    if (wp & MK_LBUTTON) drawing = TRUE;
    if (drawing) {
      pen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
      SelectObject(hdc, bmp);
      SelectObject(hdc, pen);
      MoveToEx(hdc, oldx, oldy, NULL);
      LineTo(hdc, x, y);
      DeleteObject(pen);
      InvalidateRect(hwnd, NULL, FALSE);
    }
    // mouse capture
    if (wp & MK_LBUTTON) SetCapture(hwnd);
    else ReleaseCapture();
    DeleteObject(pen);
    return 0;
  }
  case WM_LBUTTONUP: {
    drawing = FALSE;
    ReleaseCapture();
    return 1;
  }
  case WM_LBUTTONDOWN: {
    drawing = TRUE;
    return 1;
  }
  case WM_RBUTTONUP: {
    PostMessage(hwnd, WM_CLOSE, 0, 0);
    return 0;
  }
  case WM_CHAR: {
    switch (wp) {
    case VK_ESCAPE: {
      PostMessage(hwnd, WM_CLOSE, 0, 0);
      return 0;
    }
    }
    return 0;
  }
  case WM_CLOSE: {
    if (1||MessageBox(hwnd, TEXT("exit?"), C_APPNAME, MB_OKCANCEL) == IDOK) {
      DeleteObject(bmp);
      DeleteDC(hdc);
      wt.WTClose(wtctx);
      FreeLibrary(wintab32dll);
      DestroyWindow(hwnd);
    }
    return 0;
  }
  case WM_DESTROY: {
    PostQuitMessage(0);
    break;
  }
  }
  return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cl, int cs){
  MSG msg;
  WNDCLASSEX wc;
  HWND hwnd;

  // Main Window: Settings
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = mainWndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hi;
  wc.hIcon = LoadImage(NULL, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
  wc.hCursor = LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
  wc.hbrBackground = GetStockObject(LTGRAY_BRUSH);
  wc.lpszMenuName = NULL;
  wc.lpszClassName = C_WINDOW_CLASS;
  wc.hIconSm = LoadImage(NULL, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
  if (RegisterClassEx(&wc) == 0) return 1;

  // Main Window: Create, Show
  hwnd = CreateWindowEx(
    0*WS_EX_TOPMOST,
    C_WINDOW_CLASS, C_APPNAME,
    WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU,// | WS_POPUP,
    0, 0,
    C_SCWIDTH/2,
    C_SCHEIGHT/2,
    NULL, NULL, hi, NULL
  );
  if (hwnd == NULL) return 1;

  // main
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return msg.wParam;
}
