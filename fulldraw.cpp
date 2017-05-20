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

LONG nn;
TCHAR ss[255];
void mbox(LPTSTR str) {
  #ifdef dev
  MessageBox(NULL, str, str, MB_OK);
  #endif
}
void tou(HWND hwnd, HDC hdc, LPTSTR str) {
  #ifdef dev
  Graphics gpctx(hdc);
  SolidBrush *brush = new SolidBrush(C_BGCOLOR);
  gpctx.FillRectangle(brush, 0, 0, C_SCWIDTH, 20);
  delete brush;
  TextOut(hdc, 0, 0, str, lstrlen(str));
  InvalidateRect(hwnd, NULL, FALSE);
  #endif
}

class Wintab32 {
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
  Wintab32() {
    dll = NULL;
    ctx = NULL;
  }
  void end() {
    WTClose(ctx);
    FreeLibrary(dll);
  }
  HINSTANCE init() {
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
  HCTX startMouseMode(HWND hwnd) {
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
};

class DCBuffer {
public:
  HDC dc;
  HBITMAP bmp;
  DCBuffer(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    dc = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, C_SCWIDTH, C_SCHEIGHT);
    SelectObject(dc, bmp);
    cls();
    ReleaseDC(hwnd, hdc);
  }
  void cls() {
    Graphics gpctx(dc);
    gpctx.Clear(C_BGCOLOR);
  }
  void end() {
    DeleteObject(bmp);
    DeleteDC(dc);
  }
};

class DrawParams {
public:
  BOOL drawing;
  INT16 x, y, oldx, oldy;
  DrawParams() {
    drawing = FALSE;
    x = y = oldx = oldy = 0;
  }
};

LRESULT CALLBACK mainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  static DrawParams *dwpa;
  // GDI+
  static DCBuffer *dcb1;
  static DCBuffer *dcb2;
  static GraphicsPath *path;
  // Wintab
  static Wintab32 *wt;
  switch (msg) {
  case WM_CREATE: {
    // x, y
    dwpa = new DrawParams;
    // load wintab32.dll and open context
    wt = new Wintab32;
    wt->startMouseMode(hwnd);
    #ifdef dev
    wsprintf(ss, TEXT("fulldraw - %d, %d"), wt->dll, wt->ctx);
    SetWindowText(hwnd, ss);
    #endif
    // ready bitmap buffer
    dcb1 = new DCBuffer(hwnd);
    dcb2 = new DCBuffer(hwnd);
    return 0;
  }
  case WM_MOUSELEAVE: {
    ReleaseCapture();
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
    BitBlt(odc, 0, 0, C_SCWIDTH, C_SCHEIGHT, dcb1->dc, 0, 0, SRCCOPY);
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_MOUSEMOVE: {
    BOOL eraser = FALSE;
    UINT pensize = 1;
    UINT pressure = 100;
    UINT penmin = 0;
    UINT penmax = 10;
    UINT presmax = 300;
    // init
    INT16 &oldx = dwpa->oldx;
    INT16 &oldy = dwpa->oldy;
    INT16 &x = dwpa->x;
    INT16 &y = dwpa->y;
    oldx = x;
    oldy = y;
    x = LOWORD(lp);
    y = HIWORD(lp);
    // debug
    wsprintf(ss, TEXT("%d"),
      GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS));
    tou(hwnd, dcb1->dc, ss);
    // wintab
    if (wt->ctx != NULL) {
      PACKET pkt;
      if (wt->getLastPacket(pkt)) {
        pressure = pkt.pkNormalPressure;
        if (pkt.pkCursor == 2) eraser = TRUE;
        // debug
        wsprintf(ss, TEXT("%d, %d, %d, %d, %d, %d"),
          pkt.pkX, pkt.pkY, pkt.pkNormalPressure, pkt.pkCursor, pensize,
          GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS));
        tou(hwnd, dcb1->dc, ss);
      }
    }
    // draw line
    if (dwpa->drawing) {
      pensize = pressure / (presmax / penmax);
      Pen pen2(C_FGCOLOR, pensize);
      pen2.SetStartCap(LineCapRound);
      pen2.SetEndCap(LineCapRound);
      if (eraser) {
        pen2.SetColor(C_BGCOLOR);
        pen2.SetWidth(10);
      }
      Graphics gpctx(dcb1->dc);
      gpctx.SetSmoothingMode(SmoothingModeAntiAlias);
      gpctx.DrawLine(&pen2, oldx, oldy, x, y);
      /* experimental
      Graphics gpct2(dcb2->dc);
      dcb2->cls();
      Color col(Color(122, 0 ,100, 0));
      Pen pen3(col, pensize);
      pen3.SetStartCap(LineCapRound);
      pen3.SetEndCap(LineCapRound);
      pen3.SetLineJoin(LineJoinRound);
      path->AddLine(Point(oldx, oldy), Point(x, y));
      gpct2.DrawPath(&pen3, path);
      BitBlt(dcb1->dc, 10, 0, C_SCWIDTH, C_SCHEIGHT, dcb2->dc, 0, 0, SRCAND);
      InvalidateRect(hwnd, NULL, FALSE);
      //*/
    }
    return 0;
  }
  case WM_ACTIVATE: {
    if (LOWORD(wp) == WA_INACTIVE) {
      dwpa->drawing = FALSE;
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_LEFT);
      SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    } else {
      #ifndef dev
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST);
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      #endif
    }
    wsprintf(ss, TEXT("%d,%d"), LOWORD(wp), GetTickCount());
    tou(hwnd, dcb1->dc, ss);
    return 0;
  }
  case WM_LBUTTONUP: {
    delete path;
    dwpa->drawing = FALSE;
    ReleaseCapture();
    return 0;
  }
  case WM_LBUTTONDOWN: {
    path = new GraphicsPath();
    dwpa->drawing = TRUE;
    SetCapture(hwnd);
    return 0;
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
    #ifdef dev
    if (TRUE) {
    #else
    if (MessageBox(hwnd, TEXT("exit?"), C_APPNAME, MB_OKCANCEL) == IDOK) {
    #endif
      dcb1->end();
      dcb2->end();
      wt->end();
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
  wc.hIcon = (HICON)LoadImage(hi, TEXT("APPICON"), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
  wc.hCursor = (HCURSOR)LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
  wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
  wc.lpszMenuName = NULL;
  wc.lpszClassName = C_WINDOW_CLASS;
  wc.hIconSm = (HICON)LoadImage(hi, TEXT("APPICON"), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
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
