#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
using namespace Gdiplus;

#include "fulldraw.h"

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

#ifdef dev
HWND chwnd;
TCHAR ss[255];
ULONGLONG nn;
HDC ddcc;
void tou(LPTSTR str, HDC hdc, HWND hwnd, int bottom) {
  Graphics gpctx(hdc);
  SolidBrush brush(0xFFc0c0c0);
  gpctx.FillRectangle(&brush, 0, bottom * 20, C_SCWIDTH, 20);
  TextOut(hdc, 0, bottom * 20, str, lstrlen(str));
  InvalidateRect(hwnd, NULL, TRUE);
}
#define touf(f,...)  wsprintf(ss,TEXT(f),__VA_ARGS__),tou(ss,ddcc,chwnd,0)
#define touf2(f,...) wsprintf(ss,TEXT(f),__VA_ARGS__),tou(ss,ddcc,chwnd,1)
#define touf3(f,...) wsprintf(ss,TEXT(f),__VA_ARGS__),tou(ss,ddcc,chwnd,2)
#define touf4(f,...) wsprintf(ss,TEXT(f),__VA_ARGS__),tou(ss,ddcc,chwnd,3)
#define mboxf(f,...) wsprintf(ss,TEXT(f),__VA_ARGS__),MessageBox(NULL,ss,ss,0)
#endif
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
  BOOL eraser;
  int x, y, oldx, oldy;
  int pressure;
  int penmax, presmax;
  int PEN_MIN, PEN_MAX, PEN_INDE;
  int PRS_MIN, PRS_MAX, PRS_INDE;
  void init() {
    eraser = FALSE;
    x = y = oldx = oldy = 0;
    pressure = 0;
    PEN_INDE = 1 * 2;
    PEN_MIN = 2 * 2;
    PEN_MAX = 25 * 2;
    PRS_MAX = 1023;
    PRS_MIN = 33;
    PRS_INDE = 33;
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

#ifdef dev
LRESULT CALLBACK chproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  static DCBuffer dgdc1;
  switch (msg) {
  case WM_CREATE: {
    dgdc1.init(hwnd);
    ddcc = dgdc1.dc;
    return 0;
  }
  case WM_ERASEBKGND: return 1;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC odc = BeginPaint(hwnd, &ps);
    BitBlt(odc, 0, 0, C_SCWIDTH, C_SCHEIGHT, dgdc1.dc, 0, 0, SRCCOPY);
    EndPaint(hwnd, &ps);
    return 0;
  }
  }
  return DefWindowProc(hwnd, msg, wp, lp);
}
void createDebugWindow(HWND parent) {
  WNDCLASS wc;
  SecureZeroMemory(&wc, sizeof(WNDCLASS));
  wc.lpfnWndProc = chproc;
  wc.lpszClassName = TEXT("fulldraw_debug_winclass");
  if (RegisterClass(&wc) == 0) {
    MessageBox(NULL, TEXT("failed: child win class"), NULL, MB_OK);
    return;
  }
  chwnd = CreateWindow(wc.lpszClassName, C_APPNAME, WS_VISIBLE,
    0, 600, C_SCWIDTH, 120, parent, NULL, NULL, NULL);
}
#endif
LRESULT CALLBACK mainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  static DrawParams dwpa;
  static DCBuffer dcb1;
  static HMENU menu;
  static HMENU popup;
  #ifdef dev
  static UINT mss[16];
  int i = sizeof(mss) / sizeof(UINT);
  for (;i-- > 1;) mss[i] = mss[i - 1];
  mss[i] = msg;
  touf2("[%x,%x] %x > %x > %x > %x > %x > %x > %x > %x > %x > %x > %x > %x > %x > %x > %x > %x", lp, wp, mss[0], mss[1], mss[2], mss[3], mss[4], mss[5], mss[6], mss[7], mss[8], mss[9], mss[10], mss[11], mss[12], mss[13], mss[14], mss[15]);
  touf3("[%d] gdi:%d", GetTickCount(), GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS));
  #endif
  switch (msg) {
  case WM_CREATE: {
    // x, y
    dwpa.init();
    // ready bitmap buffer
    dcb1.init(hwnd);
    // cursor
    cursor.setCursor(hwnd, dwpa);
    // menu
    menu = LoadMenu(GetModuleHandle(NULL), TEXT("C_CTXMENU"));
    popup = GetSubMenu(menu, 0);
    #ifdef dev
    createDebugWindow(hwnd);
    wsprintf(ss, TEXT("fulldraw - %d, %d"), 0, 0);
    SetWindowText(chwnd, ss);
    #endif
    // post WM_POINTERXXX on mouse move
    EnableMouseInPointer(TRUE);
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
  case WM_POINTERDOWN: {
    // WM_RBUTTONDOWN
    POINTER_PEN_INFO penInfo;
    GetPointerPenInfo(GET_POINTERID_WPARAM(wp), &penInfo);
    if (penInfo.pointerInfo.pointerType == PT_MOUSE &&
      IS_POINTER_SECONDBUTTON_WPARAM(wp)) {
      PostMessage(hwnd, WM_CONTEXTMENU, wp, lp);
      return 0;
    }
    // WM_LBUTTONDOWN
    POINT point = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    ScreenToClient(hwnd, &point);
    dwpa.movePoint(point.x, point.y);
    dwpa.pressure = dwpa.PRS_INDE * 3; // for mouse
    SetCapture(hwnd);
    return 0;
  }
  case WM_POINTERUPDATE: {
    // WM_MOUSEMOVE
    POINTER_PEN_INFO penInfo;
    GetPointerPenInfo(GET_POINTERID_WPARAM(wp), &penInfo);
    POINT point = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    ScreenToClient(hwnd, &point);
    dwpa.movePoint(point.x, point.y);
    #ifdef dev
    touf("[%d] prs:%d, penmax:%d, presmax:%d",
      GetTickCount(), dwpa.pressure,
      dwpa.penmax, dwpa.presmax);
    #endif
    if (penInfo.pointerInfo.pointerType == PT_PEN) {
      if (!penInfo.pressure) {
        // Sometimes, Windows doesn't set cursor
        cursor.setCursor(hwnd, dwpa);
        return 0;
      }
      dwpa.pressure = penInfo.pressure;
      dwpa.eraser = !!(penInfo.penFlags & PEN_FLAG_ERASER);
    }
    SendMessage(hwnd, WM_COMMAND, C_CMD_DRAW, 0);
    return 0;
  }
  case WM_POINTERUP: {
    // WM_LBUTTONUP
    dwpa.pressure = 0; // for mouse
    ReleaseCapture();
    return 0;
  }
  case WM_CONTEXTMENU: { // WM_CONTEXTMENU's lp is [screen x,y]
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
      if (pressure > presmax) pressure = presmax;
      pensize = pressure * penmax / presmax;
      if (!pensize) {
        return 0; // DrawLine() draws 1px line if pensize=0
      }
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
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE);
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
    #ifdef dev
    touf("%d", wp);
    #endif
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
      dwpa.pressure = 0;
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_LEFT);
      SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    } else {
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST);
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      #ifdef dev
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_LEFT);
      SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      #endif
    }
    #ifdef dev
    touf("%d,%d", LOWORD(wp), GetTickCount());
    #endif
    return 0;
  }
  case WM_CLOSE: {
    #ifdef dev
    if (TRUE) {
    #else
    if (MessageBox(hwnd, TEXT("exit?"),
    C_APPNAME, MB_OKCANCEL | MB_ICONWARNING) == IDOK) {
    #endif
      dcb1.end();
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
    #ifdef dev
    WS_EX_LEFT,
    wc.lpszClassName, C_APPNAME,
    WS_VISIBLE | WS_SYSMENU | WS_POPUP | WS_OVERLAPPEDWINDOW,
    80, 80,
    C_SCWIDTH/1.5,
    C_SCHEIGHT/1.5,
    #else
    WS_EX_TOPMOST,
    wc.lpszClassName, C_APPNAME,
    WS_VISIBLE | WS_SYSMENU | WS_POPUP,
    0, 0,
    C_SCWIDTH,
    C_SCHEIGHT,
    #endif
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
