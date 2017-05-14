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
  #ifdef dev
  MessageBox(NULL, str, str, MB_OK);
  #endif
}
void tou(HWND hwnd, HDC hdc, LPTSTR str) {
  #ifdef dev
  RECT rect;
  rect.left = rect.top = 0;
  rect.right = C_SCWIDTH;
  rect.bottom = 20;
  FillRect(hdc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
  TextOut(hdc, 0, 0, str, lstrlen(str));
  InvalidateRect(hwnd, NULL, FALSE);
  #endif
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
      }
    }
    // timer
    //SetTimer(hwnd, 0, 10, NULL);
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
    UINT pensize = 3;
    UINT pressure = 1;
    UINT penmin = 0;
    UINT penmax = 10;
    UINT presmax = 300;
    #ifdef dev
    pensize = 8;
    #endif
    // init
    oldx = x;
    oldy = y;
    x = LOWORD(lp);
    y = HIWORD(lp);
    wsprintf(ss, TEXT("%d"), GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS));
    tou(hwnd, hdc, ss);
    // wintab
    if (wtctx != NULL) {
      // wintab packets handler
      UINT FAR oldest;
      UINT FAR newest;
      PACKET pkt;
      // get all queues' oldest to newest
      if (wt.WTQueuePacketsEx(wtctx, &oldest, &newest)) {
        // get newest queue
        if (wt.WTPacket(wtctx, newest, &pkt)) {
          pressure = pkt.pkNormalPressure;
          pensize = pressure / (presmax / penmax);
          if (pkt.pkCursor == 2) eraser = TRUE;
          wsprintf(ss, TEXT("%d, %d, %d, %d, %d, %d, %d"), pkt.pkX, pkt.pkY, pkt.pkNormalPressure, pkt.pkCursor, pensize, newest, GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS));
          tou(hwnd, hdc, ss);
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
      }
      //BitBlt(hdc, 0, 0, C_SCWIDTH, C_SCHEIGHT, adc, 0, 0, SRCCOPY);
      // finish
      InvalidateRect(hwnd, NULL, FALSE);
    }
    return 0;
  }
  case WM_ACTIVATE: {
    if (LOWORD(wp) == WA_INACTIVE) {
      drawing = FALSE;
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_LEFT);
      SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    } else {
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST);
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    wsprintf(ss, TEXT("%d,%d"), LOWORD(wp), GetTickCount());
    tou(hwnd, hdc, ss);
    return 0;
  }
  case WM_LBUTTONUP: {
    /*
    HBRUSH brush;
    brush = CreateSolidBrush(RGB(255, 255, 255));
    RECT rect;
    rect.left = 0, rect.top = 0;
    rect.right = C_SCWIDTH, rect.bottom = C_SCHEIGHT;
    FillRect(adc, &rect, brush);
    DeleteObject(brush);
    //*/
    drawing = FALSE;
    ReleaseCapture();
    return 0;
  }
  case WM_LBUTTONDOWN: {
    drawing = TRUE;
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
    WS_EX_TOPMOST,
    C_WINDOW_CLASS, C_APPNAME,
    #ifdef dev
    WS_VISIBLE | WS_SYSMENU | WS_POPUP | WS_OVERLAPPEDWINDOW,
    80, 80,
    C_SCWIDTH/1.5,
    C_SCHEIGHT/1.5,
    #else
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
