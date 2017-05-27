#include <windows.h>
#include <msgpack.h>
#include <wintab.h>
#define PACKETDATA PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE | PK_CURSOR
#define PACKETMODE PK_BUTTONS
#include <pktdef.h>
#include <gdiplus.h>
using namespace Gdiplus;

#define C_APPNAME TEXT("fulldraw")
#define C_WINDOW_CLASS TEXT("fulldraw_window_class")
#define C_SCWIDTH GetSystemMetrics(SM_CXSCREEN)
#define C_SCHEIGHT GetSystemMetrics(SM_CYSCREEN)
#define C_FGCOLOR Color(255, 0, 0, 0)
#define C_BGCOLOR Color(255, 255, 255, 255)

void __start__() {
  // program will start from here if `gcc -nostartfiles`
  ExitProcess(WinMain(GetModuleHandle(NULL), 0, "", 0));
}

#ifdef dev
LONG nn;
TCHAR ss[255];
void tou(LPTSTR str, HDC hdc, HWND hwnd) {
  Graphics gpctx(hdc);
  Pen pen(C_BGCOLOR, 40);
  gpctx.DrawLine(&pen, 0, 0, C_SCWIDTH, 0);
  TextOut(hdc, 0, 0, str, lstrlen(str));
  InvalidateRect(hwnd, NULL, FALSE);
}
#define touf(f,...)  wsprintf(ss,TEXT(f),__VA_ARGS__),tou(ss,dcb1.dc,hwnd)
#define mboxf(f,...) wsprintf(ss,TEXT(f),__VA_ARGS__),MessageBox(NULL,ss,ss,MB_OK)
#endif

static class Wintab32 {
private:
  AXIS pressureData;
public:
  typedef UINT (API *typeWTInfoW)(UINT, UINT, LPVOID);
  typedef HCTX (API *typeWTOpenW)(HWND, LPLOGCONTEXTW, BOOL);
  typedef BOOL (API *typeWTClose)(HCTX);
  typedef BOOL (API *typeWTPacket)(HCTX, UINT, LPVOID);
  typedef BOOL (API *typeWTQueuePacketsEx)(HCTX, UINT FAR *, UINT FAR *);
  typeWTInfoW WTInfoW;
  typeWTOpenW WTOpenW;
  typeWTClose WTClose;
  typeWTPacket WTPacket;
  typeWTQueuePacketsEx WTQueuePacketsEx;
  HINSTANCE dll;
  HCTX ctx;
  AXIS *pressure;
  void end() {
    WTClose(ctx);
    FreeLibrary(dll);
  }
  HINSTANCE init() {
    pressure = NULL;
    ctx = NULL;
    dll = LoadLibrary(TEXT("wintab32.dll"));
    if (dll == NULL) {
      return dll;
    }
    this->WTInfoW = (typeWTInfoW)GetProcAddress(dll, "WTInfoW");
    this->WTOpenW = (typeWTOpenW)GetProcAddress(dll, "WTOpenW");
    this->WTClose = (typeWTClose)GetProcAddress(dll, "WTClose");
    this->WTPacket = (typeWTPacket)GetProcAddress(dll, "WTPacket");
    this->WTQueuePacketsEx = (typeWTQueuePacketsEx)GetProcAddress(dll, "WTQueuePacketsEx");
    return dll;
  }
  BOOL getPressureMinMax(AXIS *axis) {
    if (dll == NULL && init() == NULL) return FALSE;
    if (WTInfoW(WTI_DEVICES, DVC_NPRESSURE, axis) == 0) return FALSE;
    return TRUE;
  }
  HCTX startMouseMode(HWND hwnd) {
    if (dll != NULL || ctx != NULL) end();
    if (init() == NULL) return NULL;
    LOGCONTEXTW lcMine;
    if (WTInfoW(WTI_DEFSYSCTX, 0, &lcMine) == 0) return NULL;
    lcMine.lcMsgBase = WT_DEFBASE;
    lcMine.lcPktData = PACKETDATA;
    lcMine.lcPktMode = PACKETMODE;
    lcMine.lcMoveMask = PACKETDATA;
    lcMine.lcBtnUpMask = lcMine.lcBtnDnMask;
    lcMine.lcOutOrgX = 0;
    lcMine.lcOutOrgY = 0;
    lcMine.lcOutExtX = GetSystemMetrics(SM_CXSCREEN);
    lcMine.lcOutExtY = -GetSystemMetrics(SM_CYSCREEN);
    ctx = WTOpenW(hwnd, &lcMine, TRUE);
    if (getPressureMinMax(&pressureData)) {
      pressure = &pressureData;
    }
    return ctx;
  }
  BOOL getLastPacket(PACKET &pkt) {
    UINT FAR oldest;
    UINT FAR newest;
    // get que oldest and newest from all queues
    if (!WTQueuePacketsEx(ctx, &oldest, &newest)) return FALSE;
    // get newest queue and flush all queues
    if (!WTPacket(ctx, newest, &pkt)) return FALSE;
    return TRUE;
  }
} wintab32;

class DCBuffer {
public:
  HDC dc;
  void init(HWND hwnd, int w = C_SCWIDTH, int h = C_SCHEIGHT) {
    HBITMAP bmp;
    HDC hdc = GetDC(hwnd);
    dc = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, w, h);
    SelectObject(dc, bmp);
    DeleteObject(bmp);
    cls();
    ReleaseDC(hwnd, hdc);
  }
  void cls() {
    Graphics gpctx(dc);
    gpctx.Clear(C_BGCOLOR);
  }
  void end() {
    DeleteDC(dc);
  }
};

class DrawParams {
public:
  BOOL drawing;
  INT16 x, y, oldx, oldy;
  UINT penmax, presmax;
  void init() {
    drawing = FALSE;
    x = y = oldx = oldy = 0;
    if (wintab32.pressure) {
      presmax = wintab32.pressure->axMax / 3;
      penmax = presmax / 30;
    } else {
      presmax = 300;
      penmax = presmax / 30;
    }
  }
};

static class Cursor {
private:
  void drawBMP(HWND hwnd, char *ptr, int w, int h, UINT penmax) {
    DCBuffer dcb;
    dcb.init(hwnd, w, h);
    Graphics gpctx(dcb.dc);
    Pen pen2(Color(255, 0, 0, 0), 1);
    gpctx.DrawEllipse(&pen2,
      (INT)((w / 2) - (penmax / 2)),
      (INT)((h / 2) - (penmax / 2)),
      penmax, penmax
    );
    gpctx.DrawLine(&pen2, w / 2, 0, w / 2, h);
    gpctx.DrawLine(&pen2, 0, h / 2, w, h / 2);
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w;) {
        #define pixel (GetPixel(dcb.dc, x++, y) != RGB(255, 255, 255))
        *ptr = pixel << 7 | pixel << 6 | pixel << 5 | pixel << 4 |
               pixel << 3 | pixel << 2 | pixel << 1 | pixel << 0;
        ptr++;
      }
    }
    dcb.end();
  }
  HCURSOR create(HWND hwnd, UINT penmax) {
    #define w 64
    #define h w
    #define ONE_BYTE 8
    int x = w / 2, y = h / 2;
    char and[(w / ONE_BYTE) * h];
    char xor[(w / ONE_BYTE) * h];
    drawBMP(hwnd, xor, w, h, penmax);
    for (int i = 0; i < h; i++) {
      for (int k = 0; k < (w / ONE_BYTE); k++) {
        int idx = (i * (w / ONE_BYTE)) + k;
        and[idx] = 0xff;
      }
    }
    return CreateCursor(GetModuleHandle(NULL), x, y, w, h, and, xor);
  }
public:
  BOOL setCursor(HWND hwnd, UINT penmax) {
    HCURSOR cursor = create(hwnd, penmax);
    HCURSOR old = (HCURSOR)GetClassLongPtr(hwnd, GCLP_HCURSOR);
    SetClassLongPtr(hwnd, GCL_HCURSOR, (LONG)cursor);
    SetCursor(cursor);
    DestroyCursor(old);
    return 0;
  }
} cursor;

LRESULT CALLBACK mainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  static DrawParams dwpa;
  // GDI+
  static DCBuffer dcb1;
  // Wintab
  Wintab32 &wt = wintab32;
  switch (msg) {
  case WM_CREATE: {
    // load wintab32.dll and open context
    wt.startMouseMode(hwnd);
    #ifdef dev
    wsprintf(ss, TEXT("fulldraw - %d, %d"), wt.dll, wt.ctx);
    SetWindowText(hwnd, ss);
    #endif
    // x, y
    dwpa.init();
    // ready bitmap buffer
    dcb1.init(hwnd);
    // cursor
    cursor.setCursor(hwnd, dwpa.penmax);
    return 0;
  }
  case WM_ERASEBKGND: {
    return 1;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC odc = BeginPaint(hwnd, &ps);
    BitBlt(odc, 0, 0, C_SCWIDTH, C_SCHEIGHT, dcb1.dc, 0, 0, SRCCOPY);
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_MOUSEMOVE: {
    BOOL eraser = FALSE;
    UINT pensize = 1;
    UINT pressure = 100;
    UINT &penmax = dwpa.penmax;
    UINT &presmax = dwpa.presmax;
    // init
    INT16 &oldx = dwpa.oldx;
    INT16 &oldy = dwpa.oldy;
    INT16 &x = dwpa.x;
    INT16 &y = dwpa.y;
    oldx = x;
    oldy = y;
    x = LOWORD(lp);
    y = HIWORD(lp);
    #ifdef dev
    touf("%d", GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS));
    #endif
    // wintab
    if (wt.ctx != NULL) {
      PACKET pkt;
      if (wt.getLastPacket(pkt)) {
        pressure = pkt.pkNormalPressure;
        if (pressure > presmax) pressure = presmax;
        if (pkt.pkCursor == 2) eraser = TRUE;
        #ifdef dev
        touf("%d, %d, %d, %d, %d, gdi:%d, %d, %d",
          pkt.pkX, pkt.pkY, pkt.pkNormalPressure, pkt.pkCursor, pensize,
          GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), penmax, presmax);
        #endif
      }
    }
    // draw line
    if (dwpa.drawing) {
      pensize = pressure / (presmax / penmax);
      Pen pen2(C_FGCOLOR, pensize);
      pen2.SetStartCap(LineCapRound);
      pen2.SetEndCap(LineCapRound);
      if (eraser) {
        pen2.SetColor(C_BGCOLOR);
        pen2.SetWidth(10);
      }
      HDC odc = GetDC(hwnd);
      for (UINT i = 0; i <= 1; i++) {
        Graphics gpctx(i ? dcb1.dc : odc);
        gpctx.SetSmoothingMode(SmoothingModeAntiAlias);
        gpctx.DrawLine(&pen2, oldx, oldy, x, y);
      }
      ReleaseDC(hwnd, odc);
    }
    return 0;
  }
  case WM_ACTIVATE: {
    if (LOWORD(wp) == WA_INACTIVE) {
      dwpa.drawing = FALSE;
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_LEFT);
      SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    } else {
      #ifndef dev
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST);
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      #endif
    }
    #ifdef dev
    touf("%d,%d", LOWORD(wp), GetTickCount());
    #endif
    return 0;
  }
  case WM_LBUTTONUP: {
    dwpa.drawing = FALSE;
    ReleaseCapture();
    return 0;
  }
  case WM_LBUTTONDOWN: {
    INT16 &oldx = dwpa.oldx;
    INT16 &oldy = dwpa.oldy;
    INT16 &x = dwpa.x;
    INT16 &y = dwpa.y;
    oldx = x;
    oldy = y;
    x = LOWORD(lp);
    y = HIWORD(lp);
    dwpa.drawing = TRUE;
    SetCapture(hwnd);
    return 0;
  }
  case WM_RBUTTONUP: {
    dwpa.drawing = FALSE;
    HMENU menu = LoadMenu(GetModuleHandle(NULL), TEXT("C_CTXMENU"));
    HMENU popup = GetSubMenu(menu, 0);
    POINT point;
    point.x = LOWORD(lp);
    point.y = HIWORD(lp);
    ClientToScreen(hwnd, &point);
    TrackPopupMenuEx(popup, 0, point.x, point.y, hwnd, NULL);
    return 0;
  }
  case WM_COMMAND: {
    switch (LOWORD(wp)) {
    case 0xDEAD: {
      PostMessage(hwnd, WM_CLOSE, 0, 0);
      return 0;
    }
    case 0x000C: {
      if (MessageBox(hwnd, TEXT("clear?"), C_APPNAME, MB_OKCANCEL) == IDOK) {
        dcb1.cls();
        InvalidateRect(hwnd, NULL, FALSE);
      }
      return 0;
    }
    }
    return 0;
  }
  case WM_KEYDOWN: {
    #ifdef dev
    touf("%d", wp);
    #endif
    switch (wp) {
    case VK_ESCAPE: PostMessage(hwnd, WM_COMMAND, 0xDEAD, 0); return 0;
    case VK_DELETE: PostMessage(hwnd, WM_COMMAND, 0x000C, 0); return 0;
    case 65: // a
    case 37: { // left
      UINT min = 0;
      dwpa.presmax -= 5;
      if (dwpa.presmax < min) {
        dwpa.presmax = min;
      }
      return 0;
    }
    case 68: // d
    case 39: { // right
      UINT max = wt.pressure ? wt.pressure->axMax : 1023;
      dwpa.presmax += 5;
      if (dwpa.presmax > max) {
        dwpa.presmax = max;
      }
      return 0;
    }
    case 83: // s
    case 40: { // down
      if (dwpa.penmax > 1) dwpa.penmax--;
      cursor.setCursor(hwnd, dwpa.penmax);
      return 0;
    }
    case 87: // w
    case 38: { // up
      if (dwpa.penmax < 63) dwpa.penmax++;
      cursor.setCursor(hwnd, dwpa.penmax);
      return 0;
    }
    }
    return 0;
  }
  case WM_CLOSE: {
    #ifdef dev
    if (TRUE) {
    #else
    if (MessageBox(hwnd, TEXT("exit?"), C_APPNAME, MB_OKCANCEL) == IDOK) {
    #endif
      dcb1.end();
      wt.end();
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
  ULONG_PTR gdiToken;
  GdiplusStartupInput gdiSI;

  // GDI+
  GdiplusStartup(&gdiToken, &gdiSI, NULL);

  // Main Window: Settings
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = mainWndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hi;
  wc.hIcon = (HICON)LoadImage(hi, TEXT("C_APPICON"), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
  wc.hCursor = (HCURSOR)LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
  wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
  wc.lpszMenuName = NULL;
  wc.lpszClassName = C_WINDOW_CLASS;
  wc.hIconSm = (HICON)LoadImage(hi, TEXT("C_APPICON"), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
  if (RegisterClassEx(&wc) == 0) return 1;

  // Main Window: Create, Show
  hwnd = CreateWindowEx(
    #ifdef dev
    WS_EX_LEFT,
    C_WINDOW_CLASS, C_APPNAME,
    WS_VISIBLE | WS_SYSMENU | WS_POPUP | WS_OVERLAPPEDWINDOW,
    80, 80,
    C_SCWIDTH/1.5,
    C_SCHEIGHT/1.5,
    #else
    WS_EX_TOPMOST,
    C_WINDOW_CLASS, C_APPNAME,
    WS_VISIBLE | WS_SYSMENU | WS_POPUP,
    0, 0,
    C_SCWIDTH,
    C_SCHEIGHT,
    #endif
    NULL, NULL, hi, NULL
  );
  if (hwnd == NULL) return 1;

  // main
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  // finish
  GdiplusShutdown(gdiToken);
  return msg.wParam;
}
