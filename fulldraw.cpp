#include <windows.h>
#include <windowsx.h>
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
  ExitProcess(WinMain(GetModuleHandle(NULL), 0, NULL, 0));
}

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
  BOOL end() {
    if (ctx != NULL) {
      if (WTClose(ctx)) ctx = NULL;
      else return FALSE; // failed close
    }
    if (ctx == NULL && dll != NULL) {
      if (FreeLibrary(dll)) dll = NULL;
      else return FALSE; // failed free
    }
    return TRUE;
  }
  HINSTANCE init() {
    if (!end()) return NULL;
    pressure = NULL;
    ctx = NULL;
    dll = LoadLibrary(TEXT("wintab32.dll"));
    if (dll == NULL) {
      return dll;
    }
    WTInfoW = (typeWTInfoW)GetProcAddress(dll, "WTInfoW");
    WTOpenW = (typeWTOpenW)GetProcAddress(dll, "WTOpenW");
    WTClose = (typeWTClose)GetProcAddress(dll, "WTClose");
    WTPacket = (typeWTPacket)GetProcAddress(dll, "WTPacket");
    WTQueuePacketsEx = (typeWTQueuePacketsEx)GetProcAddress(dll, "WTQueuePacketsEx");
    if (
      WTInfoW == NULL ||
      WTOpenW == NULL ||
      WTClose == NULL ||
      WTPacket == NULL ||
      WTQueuePacketsEx == NULL
    ) {
      end();
    }
    return dll;
  }
  BOOL getPressureMinMax(AXIS *axis) {
    if (dll == NULL && init() == NULL) return FALSE;
    if (WTInfoW(WTI_DEVICES, DVC_NPRESSURE, axis) == 0) return FALSE;
    return TRUE;
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
    if (getPressureMinMax(&pressureData)) {
      pressure = &pressureData;
    }
    return ctx;
  }
  BOOL getLastPacket(PACKET &pkt) {
    UINT FAR oldest;
    UINT FAR newest;
    if (dll == NULL || ctx == NULL) return FALSE;
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
  void cls(Color color = C_BGCOLOR) {
    Graphics gpctx(dc);
    gpctx.Clear(color);
  }
  void end() {
    DeleteDC(dc);
  }
};

class DrawParams {
public:
  BOOL drawing;
  int x, y, oldx, oldy;
  int penmax, presmax;
  int PEN_MIN, PEN_MAX, PEN_INDE;
  int PRS_MIN, PRS_MAX, PRS_INDE;
  void init() {
    drawing = FALSE;
    x = y = oldx = oldy = 0;
    PEN_INDE = 1 * 2;
    PEN_MIN = 2 * 2;
    PEN_MAX = 25 * 2;
    if (wintab32.pressure && wintab32.pressure->axMax > 0) {
      int PRS_DVC_MAX = wintab32.pressure->axMax;
      int unit = PRS_DVC_MAX >= 31 ? 31 : PRS_DVC_MAX;
      PRS_MAX = PRS_DVC_MAX;
      PRS_MIN = PRS_DVC_MAX / unit;
      PRS_INDE = PRS_MIN;
    } else {
      PRS_MAX = 1023;
      PRS_MIN = 33;
      PRS_INDE = 33;
    }
    presmax = PRS_INDE * 14;
    penmax = 14;
    updatePenPres();
  }
  BOOL updatePenPres() {
    // penmax
    penmax &= 0xfffe; // for cursor's circle adjustment
    if (penmax < PEN_MIN) penmax = PEN_MIN;
    if (penmax > PEN_MAX) penmax = PEN_MAX;
    // presmax
    if (presmax < PRS_MIN) presmax = PRS_MIN;
    if (presmax > PRS_MAX) presmax = PRS_MAX;
    return TRUE;
  }
};

static class Cursor {
private:
  void drawBMP(HWND hwnd, BYTE *ptr, int w, int h, DrawParams &dwpa) {
    int penmax = dwpa.penmax;
    int presmax = dwpa.presmax;
    // DC
    DCBuffer dcb;
    dcb.init(hwnd, w, h);
    dcb.cls(Color(255, 255, 255, 255));
    Graphics gpctx(dcb.dc);
    Pen pen2(Color(255, 0, 0, 0), 1);
    // penmax circle
    gpctx.DrawEllipse(&pen2,
      (w - penmax) / 2, (h - penmax) / 2, penmax, penmax
    );
    // presmax cross
    int length = w * presmax / dwpa.PRS_MAX;
    gpctx.DrawLine(&pen2, w / 2, (h - length) / 2, w / 2, (h + length) / 2);
    gpctx.DrawLine(&pen2, (w -length) / 2, h / 2, (w + length) / 2, h / 2);
    // convert DC to bits
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w;) {
        *ptr = 0;
        for (int i = 7; i >= 0; i--) {
          *ptr |= (GetPixel(dcb.dc, x++, y) != RGB(255, 255, 255)) << i;
        }
        ptr++;
      }
    }
    dcb.end();
  }
  HCURSOR create(HWND hwnd, DrawParams &dwpa) {
    const int A_BYTE = 8;
    const int w = A_BYTE * 8;
    const int h = w;
    const int sz = (w / A_BYTE) * h;
    BYTE band[sz];
    BYTE bxor[sz];
    for (int i = 0; i < sz; i++) {
      band[i] = 0xff;
    }
    drawBMP(hwnd, bxor, w, h, dwpa);
    int x = w / 2, y = h / 2;
    return CreateCursor(GetModuleHandle(NULL), x, y, w, h, band, bxor);
  }
public:
  BOOL setCursor(HWND hwnd, DrawParams &dwpa) {
    HCURSOR cursor = create(hwnd, dwpa);
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
    // x, y
    dwpa.init();
    // ready bitmap buffer
    dcb1.init(hwnd);
    // cursor
    cursor.setCursor(hwnd, dwpa);
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
    int pensize = 1;
    int pressure = 100;
    int penmax = dwpa.penmax;
    int presmax = dwpa.presmax;
    // init
    int &oldx = dwpa.oldx, &oldy = dwpa.oldy, &x = dwpa.x, &y = dwpa.y;
    oldx = x;
    oldy = y;
    x = GET_X_LPARAM(lp);
    y = GET_Y_LPARAM(lp);
    // wintab
    if (wt.ctx != NULL) {
      PACKET pkt;
      if (wt.getLastPacket(pkt)) {
        pressure = pkt.pkNormalPressure;
        if (pkt.pkCursor == 2) eraser = TRUE;
      }
    }
    if (pressure > presmax) pressure = presmax;
    // draw line
    if (dwpa.drawing) {
      pensize = pressure * penmax / presmax;
      Pen pen2(C_FGCOLOR, pensize);
      pen2.SetStartCap(LineCapRound);
      pen2.SetEndCap(LineCapRound);
      if (eraser) {
        pen2.SetColor(C_BGCOLOR);
        pen2.SetWidth(10);
      }
      HDC odc = GetDC(hwnd);
      for (int i = 0; i <= 1; i++) {
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
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST);
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    return 0;
  }
  case WM_LBUTTONUP: {
    dwpa.drawing = FALSE;
    ReleaseCapture();
    return 0;
  }
  case WM_LBUTTONDOWN: {
    int &oldx = dwpa.oldx, &oldy = dwpa.oldy, &x = dwpa.x, &y = dwpa.y;
    oldx = x;
    oldy = y;
    x = GET_X_LPARAM(lp);
    y = GET_Y_LPARAM(lp);
    dwpa.drawing = TRUE;
    SetCapture(hwnd);
    return 0;
  }
  case WM_RBUTTONUP: {
    dwpa.drawing = FALSE;
    HMENU menu = LoadMenu(GetModuleHandle(NULL), TEXT("C_CTXMENU"));
    HMENU popup = GetSubMenu(menu, 0);
    POINT point;
    point.x = GET_X_LPARAM(lp);
    point.y = GET_Y_LPARAM(lp);
    ClientToScreen(hwnd, &point);
    TrackPopupMenuEx(popup, 0, point.x, point.y, hwnd, NULL);
    return 0;
  }
  case WM_COMMAND: {
    switch (LOWORD(wp)) {
    case 0xAB32: {
      BOOL scs = wt.startMouseMode(hwnd) != NULL;
      if (!scs) {
        MessageBox(hwnd, TEXT("failed: wintab32.dll"),
          C_APPNAME, MB_OK | MB_ICONSTOP);
      }
      return 0;
    }
    case 0xDEAD: {
      PostMessage(hwnd, WM_CLOSE, 0, 0);
      return 0;
    }
    case 0x000C: {
      if (MessageBox(hwnd, TEXT("clear?"),
      C_APPNAME, MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
        dcb1.cls();
        InvalidateRect(hwnd, NULL, FALSE);
      }
      return 0;
    }
    }
    return 0;
  }
  case WM_KEYDOWN: {
    switch (wp) {
    case VK_ESCAPE: PostMessage(hwnd, WM_COMMAND, 0xDEAD, 0); return 0;
    case VK_DELETE: PostMessage(hwnd, WM_COMMAND, 0x000C, 0); return 0;
    case VK_F5: PostMessage(hwnd, WM_COMMAND, 0xAB32, 0); return 0;
    case 83: // s
    case VK_DOWN: { // down
      dwpa.penmax -= dwpa.PEN_INDE;
      dwpa.updatePenPres();
      cursor.setCursor(hwnd, dwpa);
      return 0;
    }
    case 87: // w
    case VK_UP: { // up
      dwpa.penmax += dwpa.PEN_INDE;
      dwpa.updatePenPres();
      cursor.setCursor(hwnd, dwpa);
      return 0;
    }
    case 65: // a
    case VK_LEFT: { // left
      dwpa.presmax -= dwpa.PRS_INDE;
      dwpa.updatePenPres();
      cursor.setCursor(hwnd, dwpa);
      return 0;
    }
    case 68: // d
    case VK_RIGHT: { // right
      dwpa.presmax += dwpa.PRS_INDE;
      dwpa.updatePenPres();
      cursor.setCursor(hwnd, dwpa);
      return 0;
    }
    }
    return 0;
  }
  case WM_CLOSE: {
    if (MessageBox(hwnd, TEXT("exit?"),
    C_APPNAME, MB_OKCANCEL | MB_ICONWARNING) == IDOK) {
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

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cl, int cs) {
  // GDI+
  ULONG_PTR token;
  GdiplusStartupInput input;
  GdiplusStartup(&token, &input, NULL);

  // Main Window: Settings
  WNDCLASSEX wc;
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
  HWND hwnd = CreateWindowEx(
    WS_EX_TOPMOST,
    C_WINDOW_CLASS, C_APPNAME,
    WS_VISIBLE | WS_SYSMENU | WS_POPUP,
    0, 0,
    C_SCWIDTH,
    C_SCHEIGHT,
    NULL, NULL, hi, NULL
  );
  if (hwnd == NULL) return 1;

  // main
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  // finish
  GdiplusShutdown(token);
  return msg.wParam;
}
