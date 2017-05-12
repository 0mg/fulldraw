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

void __start__() {
  // program will start from here if `gcc -nostartfiles`
  ExitProcess(WinMain(GetModuleHandle(NULL), 0, "", 0));
}

LONG nn;
TCHAR ss[255];
void mbox(LPTSTR str) {
  MessageBox(NULL, str, str, MB_OK);
}
void tou(HWND hwnd, HDC hdc, LPTSTR str) {
  RECT rect;
  rect.left = rect.top = 0;
  rect.right = C_SCWIDTH;
  rect.bottom = 20;
  FillRect(hdc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
  TextOut(hdc, 0, 0, str, lstrlen(str));
  InvalidateRect(hwnd, NULL, FALSE);
}

typedef UINT (API *typeWTInfoW)(UINT, UINT, LPVOID);
typedef HCTX (API *typeWTOpenW)(HWND, LPLOGCONTEXTW, BOOL);
typedef BOOL (API *typeWTClose)(HCTX);
typedef BOOL (API *typeWTPacket)(HCTX, UINT, LPVOID);
typedef BOOL (API *typeWTQueuePacketsEx)(HCTX, UINT FAR *, UINT FAR *);
typedef int (API *typeWTDataGet)(HCTX, UINT, UINT, int, LPVOID, LPINT);
typedef	int (API *typeWTQueueSizeGet)(HCTX);

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
  static HDC adc;
  // GDI+
  static ULONG_PTR gdiToken;
  // Wintab
  static WintabFunctions wt;
  static HINSTANCE wintab32dll;
  static HCTX wtctx;
  switch (msg) {
  case WM_CREATE: {
    // init vars
    drawing = FALSE;
    // load Wintab32.dll
    wintab32dll = LoadWintab32(&wt);
    // init wintab
    if (wintab32dll != NULL) {
      LOGCONTEXTW lcMine;
      if (wt.WTInfoW(WTI_DEFSYSCTX, 0, &lcMine) != 0) {
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
        mbox(TEXT("Error when open WTInfo"));
      }
    }
    // timer
    //SetTimer(hwnd, 0, 10, NULL);
    // GDI+
    GdiplusStartupInput gdiSI;
    GdiplusStartup(&gdiToken, &gdiSI, NULL);
    // ready BMP and paint Background
    RECT rect;
    HBITMAP bmp;
    HDC odc = GetDC(hwnd);
    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    // hdc
    hdc = CreateCompatibleDC(odc);
    bmp = CreateCompatibleBitmap(odc, C_SCWIDTH, C_SCHEIGHT);
    rect.left = 0;
    rect.top = 0;
    rect.right = C_SCWIDTH;
    rect.bottom = C_SCHEIGHT;
    SelectObject(hdc, bmp);
    FillRect(hdc, &rect, brush);
    // adc
    adc = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, C_SCWIDTH, C_SCHEIGHT);
    SelectObject(adc, bmp);
    FillRect(adc, &rect, brush);
    // finish
    DeleteObject(brush);
    DeleteObject(bmp);
    ReleaseDC(hwnd, odc);
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
    BitBlt(odc, 0, 0, C_SCWIDTH, C_SCHEIGHT, hdc, 0, 0, SRCCOPY);
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_MOUSEMOVE: {
    BOOL eraser = FALSE;
    UINT pensize = 9;
    UINT pressure;
    UINT presmax = 400;
    // init
    oldx = x;
    oldy = y;
    x = LOWORD(lp);
    y = HIWORD(lp);
    if (wp & MK_LBUTTON) drawing = TRUE;
    // wintab
    if (wtctx != NULL) {
      // wintab packets handler
      UINT FAR oldest;
      UINT FAR newest;
      PACKET pkt;
      // get all queues' oldest to newest
      if (wt.WTQueuePacketsEx(wtctx, &oldest, &newest)) {
        wsprintf(ss, TEXT("%d"), newest);
        tou(hwnd, hdc, ss);
        // get newest queue
        if (wt.WTPacket(wtctx, newest, &pkt)) {
          wsprintf(ss, TEXT("%d, %d, %d, %d"), pkt.pkX, pkt.pkY, pkt.pkNormalPressure, pkt.pkCursor);
          tou(hwnd, hdc, ss);
          pressure = pkt.pkNormalPressure;
          pensize = pressure / 30;
          if (pkt.pkCursor == 2) eraser = TRUE;
        }
      }
    }
    if (drawing) {
      {
        Pen pen2(Color(255, 0, 0, 0), pensize);
        if (eraser) {
          pen2.SetColor(Color(255, 255, 255, 255));
          pen2.SetWidth(10);
        }
        pen2.SetStartCap(LineCapRound);
        pen2.SetEndCap(LineCapRound);
        //pen2.SetLineJoin(LineJoinRound);
        Graphics gpctx(hdc);
        gpctx.SetSmoothingMode(SmoothingModeAntiAlias);
        gpctx.DrawLine(&pen2, oldx, oldy, x, y);
        GraphicsPath pathes;
        pathes.AddLine(oldx, oldy, x, y);
        //gpctx.DrawPath(&pen2, &pathes);
      }
      //BitBlt(hdc, 0, 0, C_SCWIDTH, C_SCHEIGHT, adc, 0, 0, SRCAND);
      // finish
      wsprintf(ss, TEXT("%d"),
        GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS));
      tou(hwnd, hdc, ss);
      InvalidateRect(hwnd, NULL, FALSE);
    }
    // mouse capture
    if (wp & MK_LBUTTON) SetCapture(hwnd);
    else ReleaseCapture();
    return 0;
  }
  case WM_LBUTTONUP: {
    /*HBRUSH brush;
    brush = CreateSolidBrush(RGB(255, 255, 255));
    RECT rect;
    rect.left = 0, rect.top = 0;
    rect.right = C_SCWIDTH, rect.bottom = C_SCHEIGHT;
    FillRect(adc, &rect, brush);
    DeleteObject(brush);*/
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
      DeleteDC(hdc);
      GdiplusShutdown(gdiToken);
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
  wc.hIcon = (HICON)LoadImage(NULL, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
  wc.hCursor = (HCURSOR)LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
  wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
  wc.lpszMenuName = NULL;
  wc.lpszClassName = C_WINDOW_CLASS;
  wc.hIconSm = (HICON)LoadImage(NULL, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
  if (RegisterClassEx(&wc) == 0) return 1;

  // Main Window: Create, Show
  hwnd = CreateWindowEx(
    0*WS_EX_TOPMOST,
    C_WINDOW_CLASS, C_APPNAME,
    WS_VISIBLE | WS_SYSMENU | WS_POPUP | WS_OVERLAPPEDWINDOW,
    0, 80,
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
