#include <windows.h>
#include <windowsx.h>
#include <msgpack.h>
#include <wintab.h>
#define PACKETDATA PK_X | PK_Y | PK_NORMAL_PRESSURE | PK_STATUS
#define PACKETMODE 0
#include <pktdef.h>
#include <gdiplus.h>
using namespace Gdiplus;

#include "fulldraw.h"

#define C_APPNAME TEXT("fulldraw")
#define C_WINDOW_CLASS TEXT("fulldraw_window_class")
#define C_SCWIDTH GetSystemMetrics(SM_CXSCREEN)
#define C_SCHEIGHT GetSystemMetrics(SM_CYSCREEN)
#define C_FGCOLOR Color(255, 0, 0, 0)
#define C_BGCOLOR Color(255, 255, 255, 255)
#define C_QUEUE_SIZE 0

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
  typedef int (API *typeWTPacketsGet)(HCTX, int, LPVOID);
  typedef int (API *typeWTQueueSizeGet)(HCTX);
  typedef BOOL (API *typeWTQueueSizeSet)(HCTX, int);
  typeWTInfoW WTInfoW;
  typeWTOpenW WTOpenW;
  typeWTClose WTClose;
  typeWTPacket WTPacket;
  typeWTQueuePacketsEx WTQueuePacketsEx;
  typeWTPacketsGet WTPacketsGet;
  typeWTQueueSizeGet WTQueueSizeGet;
  typeWTQueueSizeSet WTQueueSizeSet;
  HINSTANCE dll;
  HCTX ctx;
  AXIS *pressure;
  HANDLE heap;
  PACKET *packets;
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
    packets = NULL;
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
    WTPacketsGet = (typeWTPacketsGet)GetProcAddress(dll, "WTPacketsGet");
    WTQueueSizeGet = (typeWTQueueSizeGet)GetProcAddress(dll, "WTQueueSizeGet");
    WTQueueSizeSet = (typeWTQueueSizeSet)GetProcAddress(dll, "WTQueueSizeSet");
    if (
      WTInfoW == NULL ||
      WTOpenW == NULL ||
      WTClose == NULL ||
      WTPacket == NULL ||
      WTQueuePacketsEx == NULL ||
      WTPacketsGet == NULL ||
      WTQueueSizeGet == NULL ||
      WTQueueSizeSet == NULL
    ) {
      end();
    }
    return dll;
  }
  BOOL getPressureMinMax(AXIS *axis) {
    if (dll == NULL) return FALSE;
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
    lcMine.lcOutExtX = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    lcMine.lcOutExtY = -GetSystemMetrics(SM_CYVIRTUALSCREEN);
    ctx = WTOpenW(hwnd, &lcMine, TRUE);
    if (getPressureMinMax(&pressureData)) {
      pressure = &pressureData;
    }
    int defaultSize = WTQueueSizeGet(ctx);
    WTQueueSizeSet(ctx, C_QUEUE_SIZE);
    if (WTQueueSizeGet(ctx) < 1) {
      WTQueueSizeSet(ctx, defaultSize);
    }
    return ctx;
  }
  int getPackets() {
    if (dll == NULL || ctx == NULL) return -1;
    if (packets != NULL) endPackets();
    // heap get
    heap = GetProcessHeap();
    if (heap == NULL) return -2;
    // heap alloc
    int queueSize = WTQueueSizeGet(ctx);
    packets = (PACKET*)HeapAlloc(heap, 0, sizeof(PACKET) * queueSize);
    if (packets == NULL) return -3;
    // heap write
    int count = WTPacketsGet(ctx, queueSize, packets);
    if (count > 0) {
      return count;
    } else {
      endPackets();
      return -4;
    }
  }
  void endPackets() {
    HeapFree(heap, 0, packets);
    packets = NULL;
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
  BOOL eraser;
  int x, y, oldx, oldy;
  int pressure;
  int penmax, presmax;
  int PEN_MIN, PEN_MAX, PEN_INDE;
  int PRS_MIN, PRS_MAX, PRS_INDE;
  void init() {
    drawing = FALSE;
    eraser = FALSE;
    x = y = oldx = oldy = 0;
    pressure = 0;
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
  void movePoint(int newx, int newy) {
    oldx = x;
    oldy = y;
    x = newx;
    y = newy;
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
  static DCBuffer dcb1;
  Wintab32 &wt = wintab32;
  static HMENU menu;
  static HMENU popup;
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
    // menu
    menu = LoadMenu(GetModuleHandle(NULL), TEXT("C_CTXMENU"));
    popup = GetSubMenu(menu, 0);
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
  case WM_LBUTTONDOWN: {
    dwpa.movePoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
    wt.getPackets();
    wt.endPackets();
    dwpa.drawing = TRUE;
    SetCapture(hwnd);
    return 0;
  }
  case WM_MOUSEMOVE: {
    PACKET pkt;
    int count = wt.getPackets();
    if (count > 0) for (int i = 0; i < count; i++) {
      pkt = wt.packets[i];
      dwpa.pressure = pkt.pkNormalPressure;
      dwpa.eraser = !!(pkt.pkStatus & TPS_INVERT);
      POINT point;
      point.x = pkt.pkX;
      point.y = pkt.pkY;
      ScreenToClient(hwnd, &point);
      dwpa.movePoint(point.x, point.y);
      SendMessage(hwnd, WM_COMMAND, C_CMD_DRAW, 0);
    } else {
      dwpa.pressure = dwpa.PRS_INDE * 3;
      dwpa.movePoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
      SendMessage(hwnd, WM_COMMAND, C_CMD_DRAW, 0);
    }
    wt.endPackets();
    return 0;
  }
  case WM_LBUTTONUP: {
    dwpa.drawing = FALSE;
    ReleaseCapture();
    return 0;
  }
  case WM_CONTEXTMENU: {
    dwpa.drawing = FALSE;
    CheckMenuItem(popup, C_CMD_ERASER,
      dwpa.eraser ? MFS_CHECKED : MFS_UNCHECKED);
    TrackPopupMenuEx(popup, 0, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), hwnd, NULL);
    return 0;
  }
  case WM_COMMAND: {
    switch (LOWORD(wp)) {
    case C_CMD_DRAW: {
      // draw line
      int pensize;
      int pressure = dwpa.pressure;
      int penmax = dwpa.penmax;
      int presmax = dwpa.presmax;
      int oldx = dwpa.oldx, oldy = dwpa.oldy, x = dwpa.x, y = dwpa.y;
      if (!dwpa.drawing) {
        return 0;
      }
      if (pressure > presmax) pressure = presmax;
      pensize = pressure * penmax / presmax;
      Pen pen2(C_FGCOLOR, pensize);
      pen2.SetStartCap(LineCapRound);
      pen2.SetEndCap(LineCapRound);
      if (dwpa.eraser) {
        pen2.SetColor(C_BGCOLOR);
      }
      for (int i = 0; i <= 1; i++) {
        Graphics screen(hwnd);
        Graphics buffer(dcb1.dc);
        Graphics *gpctx = i ? &buffer : &screen;
        gpctx->SetSmoothingMode(SmoothingModeAntiAlias);
        gpctx->DrawLine(&pen2, oldx, oldy, x, y);
      }
      return 0;
    }
    case C_CMD_REFRESH: {
      BOOL scs = wt.startMouseMode(hwnd) != NULL;
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE);
      if (!scs) {
        MessageBox(hwnd, TEXT("failed: wintab32.dll"),
          C_APPNAME, MB_OK | MB_ICONSTOP);
      }
      return 0;
    }
    case C_CMD_EXIT: {
      PostMessage(hwnd, WM_CLOSE, 0, 0);
      return 0;
    }
    case C_CMD_CLEAR: {
      if (MessageBox(hwnd, TEXT("clear?"),
      C_APPNAME, MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
        dcb1.cls();
        InvalidateRect(hwnd, NULL, FALSE);
      }
      return 0;
    }
    case C_CMD_MINIMIZE: {
      CloseWindow(hwnd);
      return 0;
    }
    case C_CMD_ERASER: {
      dwpa.eraser = !dwpa.eraser;
      return 0;
    }
    }
    return 0;
  }
  case WM_KEYDOWN: {
    int ctrl = GetAsyncKeyState(VK_CONTROL);
    switch (wp) {
    case VK_ESCAPE: PostMessage(hwnd, WM_COMMAND, C_CMD_EXIT, 0); return 0;
    case VK_DELETE: PostMessage(hwnd, WM_COMMAND, C_CMD_CLEAR, 0); return 0;
    case VK_F5: PostMessage(hwnd, WM_COMMAND, C_CMD_REFRESH, 0); return 0;
    case 'M': {
      if (ctrl) {
        PostMessage(hwnd, WM_COMMAND, C_CMD_MINIMIZE, 0);
      } else {
        break;
      }
      return 0;
    }
    case 'E': {
      SendMessage(hwnd, WM_COMMAND, C_CMD_ERASER, 0);
      return 0;
    }
    case VK_DOWN: {
      dwpa.penmax -= dwpa.PEN_INDE;
      dwpa.updatePenPres();
      cursor.setCursor(hwnd, dwpa);
      return 0;
    }
    case VK_UP: {
      dwpa.penmax += dwpa.PEN_INDE;
      dwpa.updatePenPres();
      cursor.setCursor(hwnd, dwpa);
      return 0;
    }
    case VK_LEFT: {
      dwpa.presmax -= dwpa.PRS_INDE;
      dwpa.updatePenPres();
      cursor.setCursor(hwnd, dwpa);
      return 0;
    }
    case VK_RIGHT: {
      dwpa.presmax += dwpa.PRS_INDE;
      dwpa.updatePenPres();
      cursor.setCursor(hwnd, dwpa);
      return 0;
    }
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
  // WinMain() must return 0 before msg loop
  if (RegisterClassEx(&wc) == 0) return 0;

  // Main Window: Create, Show
  HWND hwnd = CreateWindowEx(
    WS_EX_TOPMOST,
    wc.lpszClassName, C_APPNAME,
    WS_VISIBLE | WS_SYSMENU | WS_POPUP,
    0, 0,
    C_SCWIDTH,
    C_SCHEIGHT,
    NULL, NULL, hi, NULL
  );
  // WinMain() must return 0 before msg loop
  if (hwnd == NULL) return 0;

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
