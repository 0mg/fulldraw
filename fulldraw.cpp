#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
using namespace Gdiplus;

#include "util.h"
#include "kbd.h"
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
  Status save(LPWSTR pathname) {
    Bitmap bm(width, height);
    copyToBitmap(&bm);
    // PNG {557CF406-1A04-11D3-9A73-0000F81EF32E}
    CLSID clsid = {0x557CF406, 0x1A04, 0x11D3,
      {0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E}};
    return bm.Save(pathname, &clsid, NULL);
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
  int pressure, oldpressure;
  BOOL smooth;
  static BOOL eraser;
  static int penmax, presmax;
  static int PEN_MIN, PEN_MAX, PEN_INDE;
  static int PRS_MIN, PRS_MAX, PRS_INDE;
  void init() {
    x = y = oldx = oldy = 0;
    pressure = oldpressure = 0;
    smooth = TRUE;
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
  void updatePressure(int newpressure) {
    oldpressure = pressure;
    pressure = newpressure;
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
int drawRender(HWND hwnd, DCBuffer *dcb, Bitmap *bmbg, DrawParams dwpa, C_DR_TYPE type) {
  // draw line
  int pressure = dwpa.pressure, oldpressure = dwpa.oldpressure;
  int penmax = dwpa.penmax;
  int presmax = dwpa.presmax;
  if (!presmax) presmax = 1; // avoid div 0
  if (pressure > presmax) pressure = presmax;
  if (oldpressure > presmax) oldpressure = presmax;
  REAL pensize = pressure * (REAL)penmax / presmax;
  REAL oldpensize = oldpressure * (REAL)penmax / presmax;
  Pen pen2(C_FGCOLOR, pensize); // Pen draws 1px line if pensize=0
  TextureBrush brushBGImg(bmbg);
  SolidBrush brushFG(C_FGCOLOR);
  SolidBrush brushBG(dcb->bgcolor);
  Brush *brush = &brushFG;
  float x1 = dwpa.oldx, y1 = dwpa.oldy, x2 = dwpa.x, y2 = dwpa.y;
  float r = oldpensize / 2, R = pensize / 2;
  for (int i = 0; i <= 1; i++) {
    Graphics screen(hwnd);
    Graphics buffer(dcb->dc);
    Graphics *gpctx = i ? &buffer : &screen;
    if (dwpa.smooth) gpctx->SetSmoothingMode(SmoothingModeAntiAlias);
    if (dwpa.eraser) {
      if (gpctx == &screen) {
        brush = &brushBGImg;
      } else {
        brush = &brushBG;
      }
    }
    if (type == C_DR_DOT) {
      gpctx->FillEllipse(brush, x2 - R, y2 - R, pensize, pensize);
    } else {
      const SIZE_T len = 4;
      float pts[2 * len]; // (x, y), (x, y), (x, y), (x, y)
      tanOO(pts, r, R, x1, y1, x2, y2);
      gpctx->FillEllipse(brush, x1 - r, y1 - r, oldpensize, oldpensize);
      gpctx->FillPolygon(brush, (PointF *)pts, len, FillModeAlternate);
      gpctx->FillEllipse(brush, x2 - R, y2 - R, pensize, pensize);
    }
  }
  return 0;
}
struct drawRenderThreadArgs {
  HWND hwnd;
  DCBuffer *dcb;
  Bitmap *bmbg;
  DrawParams dwpa;
  C_DR_TYPE type;
};
DWORD WINAPI drawRenderThread(LPVOID argsRAW) {
  drawRenderThreadArgs *args = (drawRenderThreadArgs *)argsRAW;
  drawRender(args->hwnd, args->dcb, args->bmbg, args->dwpa, args->type);
  return 0;
}
int drawRenderAsync(HWND hwnd, DCBuffer *dcb, Bitmap *bmbg, DrawParams &dwpa, C_DR_TYPE type) {
  drawRenderThreadArgs args;
  args.hwnd = hwnd;
  args.dcb = dcb;
  args.bmbg = bmbg;
  args.dwpa = dwpa;
  args.type = type;
  CloseHandle(CreateThread(NULL, 0, drawRenderThread, &args, 0, NULL));
  return 0;
}

void modifyMenu(HMENU menu, WORD lang) {
  setMenuText(menu, C_CMD_EXIT, lang);
  setMenuText(menu, C_CMD_CLEAR, lang);
  setMenuText(menu, C_CMD_REFRESH, lang);
  setMenuText(menu, C_CMD_MINIMIZE, lang);
  setMenuText(menu, C_CMD_ERASER, lang);
  setMenuText(menu, C_CMD_FLIP, lang);
  setMenuText(menu, C_CMD_PEN_DE, lang);
  setMenuText(menu, C_CMD_PEN_IN, lang);
  setMenuText(menu, C_CMD_PRS_DE, lang);
  setMenuText(menu, C_CMD_PRS_IN, lang);
  setMenuText(menu, C_CMD_SMOOTH, lang);
  setMenuText(menu, C_CMD_SAVEAS, lang);
  setMenuText(menu, C_CMD_VERSION, lang);
  setMenuText(menu, C_STR_PEN, lang, 2);
  setMenuText(menu, C_STR_LANG, lang, 9);
  setMenuText(menu, C_CMD_LANG_DEFAULT, lang);
  setMenuText(menu, C_CMD_LANG_JA, lang);
}

LRESULT CALLBACK mainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  static DrawParams dwpa;
  static DCBuffer dcb1, dcb2, dcbg, *dcbA, *dcbB;
  static Bitmap *bmbg; // eraser texture
  static BOOL nodraw = FALSE; // no draw dot on activated window by click
  static BOOL exitmenu = FALSE; // no draw dot on close menu by click outside
  static HMENU popup;
  static WORD langtype = C_LANG_DEFAULT;
  static TCHAR msgtxt[C_MAX_MSGTEXT];
  #ifdef dev
  static BOOL msgLogOn = 0;
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
  touf2("[lp:%8x, wp:%8x] msg: 0x%4X, nodraw: %d", lp, wp, msg, nodraw);
  #endif
  switch (msg) {
  case WM_CREATE: {
    // x, y
    dwpa.init();
    // ready bitmap buffer
    dcb1.init(hwnd, C_SCWIDTH, C_SCHEIGHT, C_BGCOLOR);
    dcb2.init(hwnd, dcb1.width, dcb1.height, Color(dcb1.bgcolor));
    dcbg.init(hwnd, C_SCWIDTH, C_SCHEIGHT, C_BGCOLOR);
    dcbA = &dcb1;
    dcbB = &dcb2;
    bmbg = new Bitmap(dcbg.width, dcbg.height);
    // cursor
    PenUI.setCursor(hwnd, dwpa);
    // keyboard shortcut
    Hotkey.assign(C_CMD_EXIT, VK_ESCAPE, 0);
    Hotkey.assign(C_CMD_CLEAR, VK_DELETE, 0);
    Hotkey.assign(C_CMD_REFRESH, VK_F5, 0);
    Hotkey.assign(C_CMD_MINIMIZE, 'M', C_KBD_CTRL);
    Hotkey.assign(C_CMD_ERASER, 'E', 0);
    Hotkey.assign(C_CMD_FLIP, 'H', 0);
    Hotkey.assign(C_CMD_PEN_DE, VK_DOWN, 0);
    Hotkey.assign(C_CMD_PEN_IN, VK_UP, 0);
    Hotkey.assign(C_CMD_PRS_DE, VK_LEFT, 0);
    Hotkey.assign(C_CMD_PRS_IN, VK_RIGHT, 0);
    Hotkey.assign(C_CMD_SMOOTH, 'S', 0);
    Hotkey.assign(C_CMD_SAVEAS, 'S', C_KBD_CTRL | C_KBD_SHIFT);
    // menu
    HMENU menu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(C_CTXMENU));
    popup = GetSubMenu(menu, 0);
    switch (LANGIDFROMLCID(GetUserDefaultLCID())) {
    case 0x0411: langtype = C_LANG_JA; break;
    default: langtype = C_LANG_DEFAULT; break;
    }
    modifyMenu(popup, langtype);
    #ifdef dev
    chwnd = createDebugWindow(hwnd, TEXT("fdw_dbg"));
    chwnd2 = createDebugWindow(hwnd, TEXT("fdw_dbg2"));
    SetWindowPos(chwnd2, HWND_TOP, C_SCWIDTH-300, 0, 300, C_SCHEIGHT, 0);
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
    SetWindowPos(hwnd, HWND_TOP, 0, 80, C_SCWIDTH/1.5, C_SCHEIGHT/1.5, 0);
    //PostMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    dwpa.penmax=dwpa.PEN_MAX; dwpa.presmax=0; dwpa.updatePenPres();
    #endif
    // post WM_POINTERXXX on mouse move
    EnableMouseInPointer(TRUE);
    return 0;
  }
  case WM_ERASEBKGND: {
    // window.onload: set window to top most if not.
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
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
      dwp2.updatePressure(penInfo.pressure);
      dwp2.eraser = !!(penInfo.penFlags & PEN_FLAG_ERASER);
      break;
    }
    case PT_TOUCH: // same to PT_MOUSE
    case PT_TOUCHPAD: // same to PT_MOUSE
    case PT_MOUSE: {
      if (IS_POINTER_FIRSTBUTTON_WPARAM(wp)) {
        // WM_LBUTTONDOWN
        // trigger of draw line
        dwpa.movePoint(point.x, point.y);
        dwpa.updatePressure(dwpa.PRS_INDE * 3);
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
    drawRender(hwnd, dcbA, bmbg, dwp2, C_DR_DOT);
    return 0;
  }
  case WM_POINTERUPDATE: {
    POINTER_INPUT_TYPE device;
    GetPointerType(GET_POINTERID_WPARAM(wp), &device);
    POINT point = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    ScreenToClient(hwnd, &point);
    dwpa.movePoint(point.x, point.y); // do everytime
    #ifdef dev
    POINTER_PEN_INFO pp; GetPointerPenInfo(GET_POINTERID_WPARAM(wp), &pp);
    #define wout touf("[%d] gdi:%d, prs:%d, penmax:%d, presmax:%d, flags: %d, device: %d, (x:%d y:%d)", GetTickCount(), GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), dwpa.pressure, dwpa.penmax, dwpa.presmax, pp.penFlags, device, dwpa.x, dwpa.y);
    wout
    #endif
    switch (device) {
    case PT_PEN: {
      // WM_PENMOVE
      POINTER_PEN_INFO penInfo;
      GetPointerPenInfo(GET_POINTERID_WPARAM(wp), &penInfo);
      dwpa.updatePressure(penInfo.pressure);
      dwpa.eraser = !!(penInfo.penFlags & PEN_FLAG_ERASER);
      if (!penInfo.pressure) { // need to set cursor on some good time
        goto end; // set cursor
      }
      break;
    }
    case PT_TOUCH: // same to PT_MOUSE
    case PT_TOUCHPAD: // same to PT_MOUSE
    case PT_MOUSE: {
      if (dwpa.pressure) {
        dwpa.updatePressure(dwpa.pressure);
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
      drawRenderAsync(hwnd, dcbA, bmbg, dwpa, C_DR_LINE);
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
    CheckMenuRadioItem(popup, C_CMD_LANG_FIRST, C_CMD_LANG_LAST,
      (langtype >> 12) | C_CMD_LANG_FIRST, MF_BYCOMMAND);
    CheckMenuItem(popup, C_CMD_ERASER,
      dwpa.eraser ? MFS_CHECKED : MFS_UNCHECKED);
    CheckMenuItem(popup, C_CMD_SMOOTH,
      dwpa.smooth ? MFS_CHECKED : MFS_UNCHECKED);
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
      getLocaleString(msgtxt, C_STR_CLEAR_CONFIRM, langtype);
      if (MessageBox(hwnd, msgtxt,
      C_APPNAME_STR, MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2) == IDOK) {
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
      wsprintf(vertxt, TEXT("%s v%d.%d.%d"), C_APPNAME_STR, C_APPVER);
      getLocaleString(msgtxt, C_STR_VERSION_TITLE, langtype);
      MSGBOXPARAMS mbpa;
      SecureZeroMemory(&mbpa, sizeof(MSGBOXPARAMS));
      mbpa.cbSize = sizeof(MSGBOXPARAMS);
      mbpa.hwndOwner = hwnd;
      mbpa.hInstance = GetModuleHandle(NULL);
      mbpa.lpszText = vertxt;
      mbpa.lpszCaption = msgtxt;
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
        while (dcbg.save(pathname) != Ok) {
          if (popError(hwnd, MB_RETRYCANCEL) != IDRETRY) break;
        }
      }
      return 0;
    }
    case C_CMD_LANG_DEFAULT: {
      langtype = C_LANG_DEFAULT;
      HMENU menu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(C_CTXMENU));
      popup = GetSubMenu(menu, 0);
      modifyMenu(popup, langtype);
      return 0;
    }
    case C_CMD_LANG_JA: {
      langtype = C_LANG_JA;
      HMENU menu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(C_CTXMENU));
      popup = GetSubMenu(menu, 0);
      modifyMenu(popup, langtype);
      return 0;
    }
    #ifdef dev
    case C_CMD_LANG_KANA: {
      langtype = C_LANG_KANA;
      HMENU menu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(C_CTXMENU));
      popup = GetSubMenu(menu, 0);
      modifyMenu(popup, langtype);
      return 0;
    }
    #endif
    case C_CMD_ERASER: {
      dwpa.eraser = !dwpa.eraser;
      return 0;
    }
    case C_CMD_SMOOTH: {
      dwpa.smooth = !dwpa.smooth;
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
    BYTE alt = C_KBD_ALT * !!(GetKeyState(VK_MENU) & 0x8000);
    BYTE shift = C_KBD_SHIFT * !!(GetKeyState(VK_SHIFT) & 0x8000);
    BYTE ctrl = C_KBD_CTRL * !!(GetKeyState(VK_CONTROL) & 0x8000);
    char key = wp & 0xFF;
    WORD id = Hotkey.getCmdIdByKeyCombo(key, alt | shift | ctrl);
    if (id) {
      PostMessage(hwnd, WM_COMMAND, id, 0);
    }
    #ifdef dev
    switch (key) {
    case 'B': {
      dcbA->cls();
      InvalidateRect(hwnd, NULL, FALSE);
      int n = GetTickCount() % 1000;
      dwpa.movePoint(30, 50);
      dwpa.movePoint(100, 30);
      dwpa.updatePressure(300);
      dwpa.updatePressure(dwpa.PRS_MAX);
      drawRender(hwnd, dcbA, bmbg, dwpa, C_DR_LINE);
      dwpa.pressure = 0;
      return 0;
    }
    case 'K': msgLogOn ^= 1; return 0;
    default: {
      alt = !!(GetKeyState(VK_MENU) & 0x8000) ? C_KBD_ALT : 0;
      shift = !!(GetKeyState(VK_SHIFT) & 0x8000) ? C_KBD_SHIFT : 0;
      ctrl = !!(GetKeyState(VK_CONTROL) & 0x8000) ? C_KBD_CTRL : 0;
      TCHAR st[0x80];
      strifyKeyCombo(st, key, alt|shift|ctrl);
      touf("str: %s; key: 0x%X(%c);", st, key, key);
    }
    }
    #endif
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
  case WM_CLOSE: {
    getLocaleString(msgtxt, C_STR_EXIT_CONFIRM, langtype);
    if (MessageBox(hwnd, msgtxt,
    C_APPNAME_STR, MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) == IDOK) {
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
  // DPI Scale
  SetProcessDPIAware();

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
    WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX | WS_POPUP,
    0, 0,
    C_SCWIDTH,
    C_SCHEIGHT,
    NULL, NULL, hi, NULL
  );
  // WinMain() must return 0 before msg loop
  if (hwnd == NULL) { popError(NULL); return 0; }

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
