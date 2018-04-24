#define dev
#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <gdiplus.h>
using namespace Gdiplus;

#include "fulldraw.h"

// defs that *.rc never call
#define C_WINDOW_CLASS TEXT("fulldraw_window_class")
#define C_SCWIDTH GetSystemMetrics(SM_CXSCREEN)
#define C_SCHEIGHT GetSystemMetrics(SM_CYSCREEN)
#define C_FGCOLOR Color(255, 0, 0, 0)
#define C_BGCOLOR Color(255, 255, 255, 255)
#define C_DR_DOT 1

void __start__() {
  // program will start from here if `gcc -nostartfiles`
  ExitProcess(WinMain(GetModuleHandle(NULL), 0, NULL, 0));
}

#ifdef dev
#include "debug.h"
HWND chwnd, chwnd2;
TCHAR ss[5000];
ULONGLONG nn;
HDC chp1, chp2;
#define touf(f,...)  wsprintf(ss,TEXT(f),__VA_ARGS__),tou(ss,chwnd,0)
#define touf2(f,...) wsprintf(ss,TEXT(f),__VA_ARGS__),tou(ss,chwnd,1)
#define touf3(f,...) wsprintf(ss,TEXT(f),__VA_ARGS__),tou(ss,chwnd,2)
#define touf4(f,...) wsprintf(ss,TEXT(f),__VA_ARGS__),tou(ss,chwnd,3)
#define mboxf(f,...) wsprintf(ss,TEXT(f),__VA_ARGS__),MessageBox(NULL,ss,ss,0)
#endif
struct Buffer {
  void *data;
  Buffer(SIZE_T size) {
    data = NULL;
    HANDLE heap = GetProcessHeap();
    if (heap == NULL) return;
    data = HeapAlloc(heap, HEAP_ZERO_MEMORY, size);
  }
  BOOL copy(void *source, SIZE_T size) {
    char *p = (char *)data, *q = (char *)source;
    for (int i = 0; i < size; i++) {
      *p++ = *q++;
    }
    return TRUE;
  }
  BOOL free() {
    HANDLE heap = GetProcessHeap();
    if (heap == NULL) return FALSE;
    if (HeapFree(heap, 0, data)) {
      data = NULL;
      return TRUE;
    }
    return FALSE;
  }
};

class DCBuffer {
public:
  HDC dc;
  ARGB bgcolor;
  void init(HWND hwnd, int w, int h, Color color) {
    HBITMAP bmp;
    HDC hdc = GetDC(hwnd);
    dc = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, w, h);
    SelectObject(dc, bmp);
    DeleteObject(bmp);
    bgcolor = color.GetValue();
    cls();
    ReleaseDC(hwnd, hdc);
  }
  void cls() {
    Graphics gpctx(dc);
    gpctx.Clear(Color(bgcolor));
  }
  void end() {
    DeleteDC(dc);
  }
};

class DrawParams {
private:
  static BOOL staticsReadied;
  static void initStatics() {
    eraser = FALSE;
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
public:
  int x, y, oldx, oldy;
  int pressure;
  static BOOL eraser;
  static int penmax, presmax;
  static int PEN_MIN, PEN_MAX, PEN_INDE;
  static int PRS_MIN, PRS_MAX, PRS_INDE;
  void init() {
    x = y = oldx = oldy = 0;
    pressure = 0;
    if (!staticsReadied) {
      initStatics();
      staticsReadied = TRUE;
    }
  }
  static BOOL updatePenPres() {
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
BOOL DrawParams::eraser;
int DrawParams::penmax; int DrawParams::presmax;
int DrawParams::PEN_MIN; int DrawParams::PEN_MAX; int DrawParams::PEN_INDE;
int DrawParams::PRS_MIN; int DrawParams::PRS_MAX; int DrawParams::PRS_INDE;
BOOL DrawParams::staticsReadied = FALSE;

static class Cursor {
private:
  void drawBMP(HWND hwnd, BYTE *ptr, int w, int h, DrawParams &dwpa) {
    int penmax = dwpa.penmax;
    int presmax = dwpa.presmax;
    // DC
    Bitmap bmp(w, h);
    Graphics gpctx(&bmp);
    gpctx.Clear(Color(255, 255, 255, 255));
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
          Color color;
          bmp.GetPixel(x++, y, &color);
          *ptr |= (color.GetValue() != 0xFFFFFFFF) << i;
        }
        ptr++;
      }
    }
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

// C_CMD_DRAW v2.0
int drawRenderOrig(HWND hwnd, HDC dc, DrawParams &dwpa, BOOL dot = 0) {
  // draw line
  int pensize;
  int pressure = dwpa.pressure;
  int penmax = dwpa.penmax;
  int presmax = dwpa.presmax;
  int oldx = dwpa.oldx, oldy = dwpa.oldy, x = dwpa.x, y = dwpa.y;
  if (pressure > presmax) pressure = presmax;
  pensize = pressure * penmax / presmax;
  Pen pen2(C_FGCOLOR, pensize); // Pen draws 1px line if pensize=0
  pen2.SetStartCap(LineCapRound);
  pen2.SetEndCap(LineCapRound);
  if (dwpa.eraser) {
    pen2.SetColor(C_BGCOLOR);
  }
  for (int i = 0; i <= 1; i++) {
    Graphics screen(hwnd);
    Graphics buffer(dc);
    Graphics *gpctx = i ? &buffer : &screen;
    gpctx->SetSmoothingMode(SmoothingModeAntiAlias);
    if (dot) {
      gpctx->DrawLine(&pen2, (float)x - 0.1, (float)y, (float)x, (float)y);
    } else {
      gpctx->DrawLine(&pen2, oldx, oldy, x, y);
    }
  }
  return 0;
}

class Direct2D {
public:
  ID2D1Factory *factory;
  ID2D1HwndRenderTarget *screen;
  ID2D1BitmapRenderTarget* kanv;
  HRESULT init(HWND hwnd) {
    HRESULT hr = D2D1CreateFactory(
      D2D1_FACTORY_TYPE_MULTI_THREADED,
      &factory
    );
    getScreen(hwnd);
    return hr;
  }
  HRESULT getScreen(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    HRESULT hr = factory->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(),
      D2D1::HwndRenderTargetProperties(
        hwnd,
        D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)
      ),
      &screen
    );
    return hr;
  }
  HRESULT createCanvas() {
    HRESULT hr = screen->CreateCompatibleRenderTarget(&kanv);
    return hr;
  }
  void end() {
    screen->Release();
    factory->Release();
  }
};

// C_CMD_DRAW v3.0 (Direct2D)
int drawRender(HWND hwnd, Direct2D &d2o, DrawParams &dwpa, BOOL dot = 0) {
  // draw line
  int pensize;
  int pressure = dwpa.pressure;
  int penmax = dwpa.penmax;
  int presmax = dwpa.presmax;
  int oldx = dwpa.oldx, oldy = dwpa.oldy, x = dwpa.x, y = dwpa.y;
  if (pressure > presmax) pressure = presmax;
  pensize = pressure * penmax / presmax;
  ID2D1RenderTarget *target = d2o.screen;
  target->BeginDraw();
  D2D1_POINT_2F point1 = {(float)oldx, (float)oldy};
  D2D1_POINT_2F point2 = {(float)x, (float)y};
  if (dot) point1.x = (float)x + 0.1, point1.y = (float)y;
  ID2D1SolidColorBrush* brush;
  target->CreateSolidColorBrush(
    dwpa.eraser ? D2D1::ColorF(255, 255, 255) : D2D1::ColorF(0, 0, 0), &brush);
  target->DrawLine(point1, point2, brush, pensize, NULL);
  target->EndDraw();
  return 0;
}

LRESULT CALLBACK mainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  static Direct2D d2main;
  static DrawParams dwpa;
  static DCBuffer dcb1;
  static BOOL nodraw = FALSE; // no draw dot on activated window by click
  static HMENU menu;
  static HMENU popup;
  #ifdef dev
  static BOOL msgLogOn = 1;
  const SIZE_T mslen = 50; static LPARAM mss[mslen];
  const SIZE_T txlen = mslen * 100; TCHAR txs[txlen];
  SecureZeroMemory(txs, sizeof(TCHAR) * txlen);
  for (int i = mslen; i-- > 0;) {
    int mgs = msg == WM_KEYUP && wp == 'J' ? 0xABCD : msg;
    mss[i] = i > 0 ? mss[i - 1] : MAKELPARAM(mgs & 0xFFFF, GetTickCount() & 0xFFFF);
  }
  for (int i = 0; i < mslen; i++) {
    wsprintf(txs, TEXT("%s%3d|%4X %s\n"), txs, HIWORD(mss[i])&0xFF, LOWORD(mss[i]), MsgStr::get(LOWORD(mss[i])));
  }
  if (msgLogOn) tou(txs, chwnd2, 0, mslen);
  touf2("[lp:%8x, wp:%8x] msg: 0x%4X", lp, wp, msg);
  touf3("nodraw: %d", nodraw);
  #endif
  switch (msg) {
  case WM_CREATE: {
    #ifdef dev
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
    SetWindowPos(hwnd, HWND_TOP, 80, 80, C_SCWIDTH/1.5, C_SCHEIGHT/1.5, 0);
    chwnd = createDebugWindow(hwnd, TEXT("fdw_dbg"));
    //chwnd2 = createDebugWindow(hwnd, TEXT("fdw_dbg2"));
    //SetWindowPos(chwnd2, HWND_TOP, C_SCWIDTH-300, 0, 300, C_SCHEIGHT, 0);
    #endif
    // x, y
    dwpa.init();
    // ready bitmap buffer
    dcb1.init(hwnd, C_SCWIDTH, C_SCHEIGHT, C_BGCOLOR);
    d2main.init(hwnd);
    d2main.createCanvas();
    d2main.screen->BeginDraw();
    d2main.screen->Clear(D2D1::ColorF(D2D1::ColorF::White));
    d2main.screen->EndDraw();
    // cursor
    cursor.setCursor(hwnd, dwpa);
    // menu
    menu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(C_CTXMENU));
    popup = GetSubMenu(menu, 0);
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
    POINTER_INPUT_TYPE device;
    GetPointerType(GET_POINTERID_WPARAM(wp), &device);
    POINT point = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    ScreenToClient(hwnd, &point);
    DrawParams dwp2 = dwpa; // is for only draw dot
    switch (device) {
    case PT_PEN: {
      // WM_PEN_TAP
      // DO NOT dwpa.movePoint(x,y) on pen tap. (invalid joint lines bug)
      POINTER_PEN_INFO penInfo;
      GetPointerPenInfo(GET_POINTERID_WPARAM(wp), &penInfo);
      // draw dot
      dwp2.movePoint(point.x, point.y);
      dwp2.pressure = penInfo.pressure;
      dwp2.eraser = !!(penInfo.penFlags & PEN_FLAG_ERASER);
      break;
    }
    case PT_TOUCHPAD: // same to PT_MOUSE
    case PT_MOUSE: {
      if (IS_POINTER_FIRSTBUTTON_WPARAM(wp)) {
        // WM_LBUTTONDOWN
        // trigger of draw line
        dwpa.movePoint(point.x, point.y);
        dwpa.pressure = dwpa.PRS_INDE * 3;
        // draw dot
        dwp2 = dwpa;
      } else {
        // WM_RBUTTONDOWN_OR_OTHER_BUTTONDOWN
        goto end; // default action (e.g. pop context menu up)
      }
      break;
    }
    }
    if (nodraw) return 0; // no need to movePoint()
    drawRender(hwnd, d2main, dwp2, C_DR_DOT);
    return 0;
  }
  case WM_POINTERUPDATE: {
    POINTER_INPUT_TYPE device;
    GetPointerType(GET_POINTERID_WPARAM(wp), &device);
    POINT point = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    ScreenToClient(hwnd, &point);
    dwpa.movePoint(point.x, point.y); // do everytime
    #ifdef dev
    POINTER_INPUT_TYPE dv; GetPointerType(GET_POINTERID_WPARAM(wp), &dv); POINTER_PEN_INFO pp; GetPointerPenInfo(GET_POINTERID_WPARAM(wp), &pp);
    #define wout touf("[%d] gdi:%d, prs:%d, penmax:%d, presmax:%d, flags: %d, device: %d, (x:%d y:%d)", GetTickCount(), GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), dwpa.pressure, dwpa.penmax, dwpa.presmax, pp.penFlags, dv, dwpa.x, dwpa.y);
    wout
    #endif
    switch (device) {
    case PT_PEN: {
      // WM_PENMOVE
      POINTER_PEN_INFO penInfo;
      GetPointerPenInfo(GET_POINTERID_WPARAM(wp), &penInfo);
      dwpa.pressure = penInfo.pressure;
      dwpa.eraser = !!(penInfo.penFlags & PEN_FLAG_ERASER);
      if (!penInfo.pressure) { // need to set cursor on some good time
        goto end; // set cursor
      }
      break;
    }
    }
    #ifdef dev
    wout
    #endif
    if (nodraw) return 0;
    if (dwpa.pressure) {
      drawRender(hwnd, d2main, dwpa);
    }
    return 0;
  }
  case WM_POINTERUP: {
    dwpa.pressure = 0;
    nodraw = FALSE;
    return 0;
  }
  case WM_NCPOINTERUP: {
    nodraw = FALSE;
    return 0;
  }
  case WM_CONTEXTMENU: { // WM_CONTEXTMENU's lp is [screen x,y]
    POINT point = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    ScreenToClient(hwnd, &point);
    RECT rect;
    GetClientRect(hwnd, &rect);
    if (!PtInRect(&rect, point)) {
      goto end;
    }
    CheckMenuItem(popup, C_CMD_ERASER,
      dwpa.eraser ? MFS_CHECKED : MFS_UNCHECKED);
    TrackPopupMenuEx(popup, 0, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), hwnd, NULL);
    return 0;
  }
  case WM_COMMAND: {
    switch (LOWORD(wp)) {
    case C_CMD_REFRESH: {
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, C_SCWIDTH, C_SCHEIGHT, 0);
      return 0;
    }
    case C_CMD_EXIT: {
      PostMessage(hwnd, WM_CLOSE, 0, 0);
      return 0;
    }
    case C_CMD_CLEAR: {
      if (MessageBox(hwnd, TEXT("clear?"),
      C_APPNAME_STR, MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
        dcb1.cls();
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateWindow(hwnd);
      }
      return 0;
    }
    case C_CMD_MINIMIZE: {
      CloseWindow(hwnd);
      return 0;
    }
    case C_CMD_VERSION: {
      TCHAR vertxt[100];
      wsprintf(vertxt, TEXT("%s v%d.%d.%d.%d"), C_APPNAME_STR, C_APPVER);
      MSGBOXPARAMS mbpa;
      mbpa.cbSize = sizeof(MSGBOXPARAMS);
      mbpa.hwndOwner = hwnd;
      mbpa.hInstance = GetModuleHandle(NULL);
      mbpa.lpszText = vertxt;
      mbpa.lpszCaption = TEXT("version");
      mbpa.dwStyle = MB_USERICON;
      mbpa.lpszIcon = MAKEINTRESOURCE(C_APPICON);
      mbpa.dwContextHelpId = 0;
      mbpa.lpfnMsgBoxCallback = NULL;
      mbpa.dwLanguageId = LANG_JAPANESE;
      MessageBoxIndirect(&mbpa);
      return 0;
    }
    case C_CMD_ERASER: {
      dwpa.eraser = !dwpa.eraser;
      return 0;
    }
    case C_CMD_PEN_DE: {
      dwpa.penmax -= dwpa.PEN_INDE;
      dwpa.updatePenPres();
      cursor.setCursor(hwnd, dwpa);
      return 0;
    }
    case C_CMD_PEN_IN: {
      dwpa.penmax += dwpa.PEN_INDE;
      dwpa.updatePenPres();
      cursor.setCursor(hwnd, dwpa);
      return 0;
    }
    case C_CMD_PRS_DE: {
      dwpa.presmax -= dwpa.PRS_INDE;
      dwpa.updatePenPres();
      cursor.setCursor(hwnd, dwpa);
      return 0;
    }
    case C_CMD_PRS_IN: {
      dwpa.presmax += dwpa.PRS_INDE;
      dwpa.updatePenPres();
      cursor.setCursor(hwnd, dwpa);
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
      SendMessage(hwnd, WM_COMMAND, C_CMD_PEN_DE, 0);
      return 0;
    }
    case VK_UP: {
      SendMessage(hwnd, WM_COMMAND, C_CMD_PEN_IN, 0);
      return 0;
    }
    case VK_LEFT: {
      SendMessage(hwnd, WM_COMMAND, C_CMD_PRS_DE, 0);
      return 0;
    }
    case VK_RIGHT: {
      SendMessage(hwnd, WM_COMMAND, C_CMD_PRS_IN, 0);
      return 0;
    }
    #ifdef dev
    case 'K': msgLogOn ^= 1; return 0;
    default: {
      touf("key: %d(%c)", wp, wp);
    }
    #endif
    }
    return 0;
  }
  case WM_ACTIVATE: {
    if (LOWORD(wp) == WA_INACTIVE) {
      dwpa.pressure = 0; // on Windows start menu
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
    touf("[%d] WM_ACTIVATE: wp %d", GetTickCount(), LOWORD(wp));
    #endif
    return 0;
  }
  case WM_MOUSEACTIVATE: {
    nodraw = TRUE;
    return MA_ACTIVATE;
  }
  case WM_POINTERACTIVATE: {
    nodraw = TRUE;
    return PA_ACTIVATE;
  }
  case WM_CLOSE: {
    #ifdef dev
    if (TRUE) {
    #else
    if (MessageBox(hwnd, TEXT("exit?"),
    C_APPNAME_STR, MB_OKCANCEL | MB_ICONWARNING) == IDOK) {
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
  end:
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
  wc.hIcon = (HICON)LoadImage(hi, MAKEINTRESOURCE(C_APPICON), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
  wc.hCursor = (HCURSOR)LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszMenuName = NULL;
  wc.lpszClassName = C_WINDOW_CLASS;
  wc.hIconSm = (HICON)LoadImage(hi, MAKEINTRESOURCE(C_APPICON), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
  // WinMain() must return 0 before msg loop
  if (RegisterClassEx(&wc) == 0) return 0;

  // Main Window: Create, Show
  HWND hwnd = CreateWindowEx(
    WS_EX_TOPMOST,
    wc.lpszClassName, C_APPNAME_STR,
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
