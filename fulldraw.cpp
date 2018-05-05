#define dev
#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <gdiplus.h>
using namespace Gdiplus;
using namespace D2D1;

#include "util.h"
#include "fulldraw.h"

// defs that *.rc never call
#define C_WINDOW_CLASS TEXT("fulldraw_window_class")
#define C_SCWIDTH GetSystemMetrics(SM_CXSCREEN)
#define C_SCHEIGHT GetSystemMetrics(SM_CYSCREEN)
#define C_FGCOLOR Color(255, 0, 0, 0)
#define C_BGCOLOR Color(255, 255, 255, 255)

LPCTSTR C_APPNAME_STR = C_APPNAME;

void __start__() {
  // program will start from here if `gcc -nostartfiles`
  ExitProcess(WinMain(GetModuleHandle(NULL), 0, NULL, 0));
}
void free(void *p) {
  // dummy for operator delete
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

class DCBuffer {
public:
  int width, height;
  HDC dc;
  ARGB bgcolor;
  void init(HWND hwnd, int w, int h, Color color) {
    Bitmap bm(w, h);
    Graphics g(&bm);
    dc = g.GetHDC();
    width = w, height = h;
    bgcolor = color.GetValue();
    cls();
  }
  void cls() {
    Graphics gpctx(dc);
    gpctx.Clear(Color(bgcolor));
  }
  void copyToBitmap(Bitmap *bm) {
    Graphics ctx(bm);
    HDC bmdc = ctx.GetHDC();
    BitBlt(bmdc, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    ctx.ReleaseHDC(bmdc);
  }
  void save(LPWSTR pathname) {
    Bitmap bm(width, height);
    copyToBitmap(&bm);
    // PNG {557CF406-1A04-11D3-9A73-0000F81EF32E}
    CLSID clsid = {0x557CF406, 0x1A04, 0x11D3,
      {0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E}};
    bm.Save(pathname, &clsid, NULL);
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

enum C_CS_TYPE {C_CS_PEN, C_CS_ARROW};
static class tagPenUI {
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
    if (!dwpa.PRS_MAX) return; // avoid div 0
    int length = w * presmax / dwpa.PRS_MAX;
    gpctx.DrawLine(&pen2, w / 2, (h - length) / 2, w / 2, (h + length) / 2);
    gpctx.DrawLine(&pen2, (w - length) / 2, h / 2, (w + length) / 2, h / 2);
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
      ((volatile BYTE *)band)[i] = 0xff; // if 0x00 : must volatile
    }
    drawBMP(hwnd, bxor, w, h, dwpa);
    int x = w / 2, y = h / 2;
    return CreateCursor(GetModuleHandle(NULL), x, y, w, h, band, bxor);
  }
public:
  BOOL setCursor(HWND hwnd, DrawParams &dwpa, C_CS_TYPE type = C_CS_PEN, BOOL redraw = TRUE) {
    HCURSOR cursor = type == C_CS_ARROW ?
      (HCURSOR)LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED) :
      create(hwnd, dwpa);
    HCURSOR old = (HCURSOR)GetClassLongPtr(hwnd, GCLP_HCURSOR);
    SetClassLongPtr(hwnd, GCLP_HCURSOR, (LONG_PTR)cursor);
    DestroyCursor(old);
    if (redraw) SetCursor(cursor);
    return 0;
  }
} PenUI;

// C_CMD_DRAW v2.0
enum C_DR_TYPE {C_DR_LINE, C_DR_DOT};
int drawRender(HWND hwnd, DCBuffer *dcb, Bitmap *bmbg, DrawParams &dwpa, C_DR_TYPE type) {
  // draw line
  int pressure = dwpa.pressure;
  int penmax = dwpa.penmax;
  int presmax = dwpa.presmax;
  int oldx = dwpa.oldx, oldy = dwpa.oldy, x = dwpa.x, y = dwpa.y;
  if (pressure > presmax) pressure = presmax;
  REAL pensize = pressure * (REAL)penmax / presmax;
  Pen pen2(C_FGCOLOR, pensize); // Pen draws 1px line if pensize=0
  pen2.SetStartCap(LineCapRound);
  pen2.SetEndCap(LineCapRound);
  for (int i = 0; i <= 1; i++) {
    Graphics screen(hwnd);
    Graphics buffer(dcb->dc);
    Graphics *gpctx = i ? &buffer : &screen;
    gpctx->SetSmoothingMode(SmoothingModeAntiAlias);
    if (dwpa.eraser) {
      if (gpctx == &screen) {
        TextureBrush brushE(bmbg);
        pen2.SetBrush(&brushE);
      } else {
        pen2.SetColor(dcb->bgcolor);
      }
    }
    if (type == C_DR_DOT) {
      gpctx->DrawLine(&pen2, (REAL)x - 0.1, (REAL)y, (REAL)x, (REAL)y);
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
  ID2D1DCRenderTarget *dcreen;
  HRESULT init(HWND hwnd) {
    HRESULT hr = D2D1CreateFactory(
      D2D1_FACTORY_TYPE_MULTI_THREADED,
      &factory
    );
    getScreen(hwnd);
    return hr;
  }
  void getScreen(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    factory->CreateHwndRenderTarget(
      RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        {DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN},
        0.0f, 0.0f,
        D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE,
        D2D1_FEATURE_LEVEL_DEFAULT),
      HwndRenderTargetProperties(
        hwnd,
        SizeU(rc.right - rc.left, rc.bottom - rc.top),
        D2D1_PRESENT_OPTIONS_NONE
      ),
      &screen
    );
    factory->CreateDCRenderTarget(
      &RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED},
        0.0f, 0.0f,
        D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE,
        D2D1_FEATURE_LEVEL_DEFAULT),
      &dcreen
    );
  }
  void resizeScreen(HWND hwnd) {
    screen->Release();
    getScreen(hwnd);
  }
  /*HRESULT createCanvas() {
    HRESULT hr = screen->CreateCompatibleRenderTarget(&kanv);
    return hr;
  }*/
  void end() {
    screen->Release();
    dcreen->Release();
    factory->Release();
  }
};

// C_CMD_DRAW v3.0 (Direct2D)
int drawRender2D(HWND hwnd, Direct2D &d2o, DrawParams &dwpa, BOOL dot = 0) {
  // draw line
  int pressure = dwpa.pressure;
  int penmax = dwpa.penmax;
  int presmax = dwpa.presmax;
  int oldx = dwpa.oldx, oldy = dwpa.oldy, x = dwpa.x, y = dwpa.y;
  if (pressure > presmax) pressure = presmax;
  REAL pensize = pressure * penmax / presmax;
  if (!pensize) pensize = 1.0f;
  ID2D1DCRenderTarget *darget = d2o.dcreen;
  ID2D1HwndRenderTarget *target = d2o.screen;
  HDC hdc = GetDC(hwnd);
  RECT rc;
  GetClientRect(hwnd, &rc);
  darget->BindDC(hdc, &rc);
  target->BeginDraw();
  D2D1_POINT_2F point1 = {(float)oldx, (float)oldy};
  D2D1_POINT_2F point2 = {(float)x, (float)y};
  if (dot) point1.x = (float)x + 0.1, point1.y = (float)y;
  ID2D1SolidColorBrush *brush;
  target->CreateSolidColorBrush(
    dwpa.eraser ? ColorF(255, 255, 255) : ColorF(0, 0, 0), &brush);
  ID2D1StrokeStyle *style;
  D2D1_STROKE_STYLE_PROPERTIES props = StrokeStyleProperties(
    D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_FLAT,
    D2D1_LINE_JOIN_MITER, 1.0f, D2D1_DASH_STYLE_SOLID, 0.0f);
  d2o.factory->CreateStrokeStyle(props, NULL, 0, &style);
  target->DrawLine(point1, point2, brush, pensize, style);
  target->EndDraw();
  ReleaseDC(hwnd, hdc);
  return 0;
}

LRESULT CALLBACK mainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  static Direct2D d2main;
  static DrawParams dwpa;
  static DCBuffer dcb1, dcb2, dcbg, *dcbA, *dcbB;
  static Bitmap *bmbg; // eraser texture
  static BOOL nodraw = FALSE; // no draw dot on activated window by click
  static BOOL exitmenu = FALSE; // no draw dot on close menu by click outside
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
  if (msgLogOn) tou(txs, chwnd2, 0, mslen), OutputDebugString(MsgStr::get(LOWORD(msg)));
  touf2("[lp:%8x, wp:%8x] msg: 0x%4X, nodraw: %d", lp, wp, msg, nodraw);
  #endif
  switch (msg) {
  case WM_CREATE: {
    #ifdef dev
    chwnd = createDebugWindow(hwnd, TEXT("fdw_dbg"));
    //chwnd2 = createDebugWindow(hwnd, TEXT("fdw_dbg2"));
    //SetWindowPos(chwnd2, HWND_TOP, C_SCWIDTH-300, 0, 300, C_SCHEIGHT, 0);
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
    SetWindowPos(hwnd, HWND_TOP, 0, 80, C_SCWIDTH/1.5, C_SCHEIGHT/1.5, 0);
    //PostMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    dwpa.penmax = 0x9; dwpa.presmax = 0; dwpa.updatePenPres();
    #endif
    // x, y
    dwpa.init();
    // ready bitmap buffer
    d2main.init(hwnd);
    d2main.screen->BeginDraw();
    d2main.screen->Clear(ColorF(ColorF::White));
    d2main.screen->EndDraw();
    dcb1.init(hwnd, C_SCWIDTH, C_SCHEIGHT, C_BGCOLOR);
    dcb2.init(hwnd, dcb1.width, dcb1.height, Color(dcb1.bgcolor));
    dcbg.init(hwnd, C_SCWIDTH, C_SCHEIGHT, C_BGCOLOR);
    dcbA = &dcb1;
    dcbB = &dcb2;
    bmbg = new Bitmap(dcbg.width, dcbg.height);
    // cursor
    PenUI.setCursor(hwnd, dwpa);
    // menu
    HMENU menu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(C_CTXMENU));
    popup = GetSubMenu(menu, 0);
    // post WM_POINTERXXX on mouse move
    EnableMouseInPointer(TRUE);
    return 0;
  }
  case WM_ERASEBKGND: {
    return 1;
  }
  case WM_PAINT: {
    // screen = bg = (eraser = bg + layerB) + layerA
    dcbg.cls();
    // bg += layerB
    BLENDFUNCTION bfB = {AC_SRC_OVER, 0, 0x15, AC_SRC_ALPHA};
    GdiAlphaBlend(
      dcbg.dc, 0, 0, dcbg.width, dcbg.height,
      dcbB->dc, 0, 0, dcbB->width, dcbB->height, bfB);
    StretchBlt(dcbg.dc, dcbg.width, 0, -dcbg.width, dcbg.height,
      dcbg.dc, 0, 0, dcbg.width, dcbg.height, SRCCOPY);
    // eraser = bg
    dcbg.copyToBitmap(bmbg);
    // bg += layerA
    GdiTransparentBlt(dcbg.dc, 0, 0, dcbg.width, dcbg.height, dcbA->dc,
      0, 0, dcbA->width, dcbA->height, Color(dcbA->bgcolor).ToCOLORREF());
    // screen = bg
    PAINTSTRUCT ps;
    HDC odc = BeginPaint(hwnd, &ps);
    BitBlt(odc, 0, 0, C_SCWIDTH, C_SCHEIGHT, dcbg.dc, 0, 0, SRCCOPY);
    EndPaint(hwnd, &ps);
    d2main.screen->BeginDraw();
    d2main.screen->Clear(ColorF(ColorF::White));
    d2main.screen->EndDraw();
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
    if (exitmenu) {
      nodraw = TRUE;
      exitmenu = FALSE;
    }
    if (nodraw) return 0; // no need to movePoint()
    drawRender2D(hwnd, d2main, dwp2, C_DR_DOT);
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
    if (exitmenu) {
      exitmenu = FALSE;
      PenUI.setCursor(hwnd, dwpa);
    }
    if (nodraw) return 0;
    if (dwpa.pressure) {
      drawRender2D(hwnd, d2main, dwpa, C_DR_LINE);
    }
    return 0;
  }
  case WM_POINTERUP: {
    dwpa.pressure = 0;
    nodraw = FALSE;
    PenUI.setCursor(hwnd, dwpa);
    return 0;
  }
  case WM_NCPOINTERUP: { // WM_NCPOINTERUP is not be sent on click titlebar
    nodraw = FALSE;
    PenUI.setCursor(hwnd, dwpa, C_CS_PEN, FALSE);
    return 0;
  }
  case WM_CONTEXTMENU: { // WM_CONTEXTMENU's lp is [screen x,y]
    const int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
    const BOOL rightclick = !(x == -1 && y == -1);
    // choice ctxmenu or sysmenu
    if (rightclick) {
      POINT point = {x, y};
      ScreenToClient(hwnd, &point);
      RECT rect;
      GetClientRect(hwnd, &rect);
      if (!PtInRect(&rect, point)) { // if R-click titlebar
        goto end;
      }
    }
    // ctxmenu position
    POINT point = {x, y};
    if (!rightclick) { // Shift + F10
      point = {dwpa.x, dwpa.y};
      ClientToScreen(hwnd, &point);
    }
    // ctxmenu popup
    CheckMenuItem(popup, C_CMD_ERASER,
      dwpa.eraser ? MFS_CHECKED : MFS_UNCHECKED);
    TrackPopupMenuEx(popup, 0, point.x, point.y, hwnd, NULL);
    return 0;
  }
  case WM_COMMAND: {
    switch (LOWORD(wp)) {
    case C_CMD_REFRESH: {
      nodraw = FALSE;
      PenUI.setCursor(hwnd, dwpa);
      InvalidateRect(hwnd, NULL, FALSE);
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, C_SCWIDTH, C_SCHEIGHT, 0);
      return 0;
    }
    case C_CMD_EXIT: {
      PostMessage(hwnd, WM_CLOSE, 0, 0);
      return 0;
    }
    case C_CMD_FLIP: {
      DCBuffer *tmp = dcbA;
      dcbA = dcbB;
      dcbB = tmp;
      InvalidateRect(hwnd, NULL, FALSE);
      UpdateWindow(hwnd);
      return 0;
    }
    case C_CMD_CLEAR: {
      if (MessageBox(hwnd, TEXT("clear?"),
      C_APPNAME_STR, MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2) == IDOK) {
        d2main.screen->BeginDraw();
        d2main.screen->Clear(ColorF(C_BGCOLOR.GetValue()));
        d2main.screen->EndDraw();
        dcbA->cls();
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
      SecureZeroMemory(&mbpa, sizeof(MSGBOXPARAMS));
      mbpa.cbSize = sizeof(MSGBOXPARAMS);
      mbpa.hwndOwner = hwnd;
      mbpa.hInstance = GetModuleHandle(NULL);
      mbpa.lpszText = vertxt;
      mbpa.lpszCaption = TEXT("version");
      mbpa.dwStyle = MB_USERICON;
      mbpa.lpszIcon = MAKEINTRESOURCE(C_APPICON);
      MessageBoxIndirect(&mbpa);
      return 0;
    }
    case C_CMD_SAVEAS: {
      const SIZE_T pathmax = 0x1000;
      WCHAR pathname[pathmax]; // = L"default.name" // memset
      lstrcpyW(pathname, L"fulldraw.png"); // non-memset
      OPENFILENAMEW ofn;
      SecureZeroMemory(&ofn, sizeof(OPENFILENAME));
      ofn.lStructSize = sizeof(OPENFILENAME);
      ofn.hwndOwner = hwnd;
      ofn.lpstrFilter = L"image/png (*.png)\0*.png\0" L"*.*\0*.*\0\0";
      ofn.lpstrFile = pathname;
      ofn.nMaxFile = pathmax;
      ofn.lpstrDefExt = L"png";
      ofn.Flags = OFN_OVERWRITEPROMPT;
      if (GetSaveFileNameW(&ofn)) {
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateWindow(hwnd);
        dcbg.save(pathname);
      }
      return 0;
    }
    case C_CMD_ERASER: {
      dwpa.eraser = !dwpa.eraser;
      return 0;
    }
    case C_CMD_PEN_DE: {
      dwpa.penmax -= dwpa.PEN_INDE;
      dwpa.updatePenPres();
      PenUI.setCursor(hwnd, dwpa);
      return 0;
    }
    case C_CMD_PEN_IN: {
      dwpa.penmax += dwpa.PEN_INDE;
      dwpa.updatePenPres();
      PenUI.setCursor(hwnd, dwpa);
      return 0;
    }
    case C_CMD_PRS_DE: {
      dwpa.presmax -= dwpa.PRS_INDE;
      dwpa.updatePenPres();
      PenUI.setCursor(hwnd, dwpa);
      return 0;
    }
    case C_CMD_PRS_IN: {
      dwpa.presmax += dwpa.PRS_INDE;
      dwpa.updatePenPres();
      PenUI.setCursor(hwnd, dwpa);
      return 0;
    }
    }
    return 0;
  }
  case WM_KEYDOWN: {
    DWORD alt = ('!' * 0x1000000) * !!(GetKeyState(VK_MENU) & 0x8000);
    DWORD shift = ('+' * 0x10000) * !!(GetKeyState(VK_SHIFT) & 0x8000);
    DWORD ctrl = ('^' * 0x100) * !!(GetKeyState(VK_CONTROL) & 0x8000);
    // '+^K': Shift+Ctrl+K, '^K': Ctrl+K, '+\0K': Shift+K
    switch (alt | shift | ctrl | wp) {
    case VK_ESCAPE: PostMessage(hwnd, WM_COMMAND, C_CMD_EXIT, 0); return 0;
    case VK_DELETE: PostMessage(hwnd, WM_COMMAND, C_CMD_CLEAR, 0); return 0;
    case VK_F5: PostMessage(hwnd, WM_COMMAND, C_CMD_REFRESH, 0); return 0;
    case '^M': {
      PostMessage(hwnd, WM_COMMAND, C_CMD_MINIMIZE, 0);
      return 0;
    }
    case 'E': {
      SendMessage(hwnd, WM_COMMAND, C_CMD_ERASER, 0);
      return 0;
    }
    case 'H': {
      SendMessage(hwnd, WM_COMMAND, C_CMD_FLIP, 0);
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
    case '+^S': {
      SendMessage(hwnd, WM_COMMAND, C_CMD_SAVEAS, 0);
      return 0;
    }
    #ifdef dev
    case 'B': {
      HDC odc = GetDC(hwnd);
      BitBlt(odc, 0, 0, C_SCWIDTH, C_SCHEIGHT, odc, 0, 0, NOTSRCCOPY);
      ReleaseDC(hwnd, odc);
      return 0;
    }
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
      PenUI.setCursor(hwnd, dwpa, C_CS_ARROW, FALSE);
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_LEFT);
      SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    } else {
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST);
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      #ifdef dev
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_LEFT);
      SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      #endif
      if (LOWORD(wp) == WA_ACTIVE) {
        if (!nodraw) PenUI.setCursor(hwnd, dwpa, C_CS_PEN, FALSE);
      }
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
  case WM_EXITMENULOOP: {
    exitmenu = TRUE;
    PenUI.setCursor(hwnd, dwpa, C_CS_ARROW);
    return 0;
  }
  case WM_SIZE: {
    if (d2main.screen != NULL) {
      d2main.resizeScreen(hwnd);
    }
    return 0;
  }
  case WM_CLOSE: {
    #ifdef dev
    if (TRUE) {
    #else
    if (MessageBox(hwnd, TEXT("exit?"),
    C_APPNAME_STR, MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) == IDOK) {
    #endif
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
  SecureZeroMemory(&wc, sizeof(WNDCLASSEX));
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = mainWndProc;
  wc.hInstance = hi;
  wc.hIcon = (HICON)LoadImage(hi, MAKEINTRESOURCE(C_APPICON), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
  wc.hCursor = (HCURSOR)LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = C_WINDOW_CLASS;
  wc.hIconSm = (HICON)LoadImage(hi, MAKEINTRESOURCE(C_APPICON), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
  RegisterClassEx(&wc);

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
  if (hwnd == NULL) { popError(); return 0; }

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
