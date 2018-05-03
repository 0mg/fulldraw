#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
using namespace Gdiplus;

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
  // use this dummy with keep safety
  // must keep what `call _free` is not exist in *.asm (cl.exe /Fa)
}

class DCBuffer {
public:
  int width, height;
  HDC dc;
  ARGB bgcolor;
  Bitmap *bmp;
  void init(HWND hwnd, int w, int h, Color color) {
    Bitmap bm(w, h);
    Graphics g(&bm);
    dc = g.GetHDC();
    width = w, height = h;
    bgcolor = color.GetValue();
    cls();
    bmp = new Bitmap(width, height);
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
int drawRender(HWND hwnd, DCBuffer *dcb, DCBuffer *dceraser, DrawParams &dwpa, C_DR_TYPE type) {
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
  if (dwpa.eraser) {
    pen2.SetColor(C_BGCOLOR);
  }
  for (int i = 0; i <= 1; i++) {
    Graphics screen(hwnd);
    Graphics buffer(dcb->dc);
    Graphics *gpctx = i ? &buffer : &screen;
    gpctx->SetSmoothingMode(SmoothingModeAntiAlias);
    if (dwpa.eraser) {
      if (gpctx == &screen) {
        TextureBrush brushE(dceraser->bmp);
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

LRESULT CALLBACK mainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  static DrawParams dwpa;
  static DCBuffer dcb1, dcb2, dcbg, dceraser, *dcbA, *dcbB;
  static BOOL nodraw = FALSE; // no draw dot on activated window by click
  static BOOL exitmenu = FALSE; // no draw dot on close menu by click outside
  static HMENU popup;
  switch (msg) {
  case WM_CREATE: {
    // x, y
    dwpa.init();
    // ready bitmap buffer
    dcb1.init(hwnd, C_SCWIDTH, C_SCHEIGHT, C_BGCOLOR);
    dcb2.init(hwnd, dcb1.width, dcb1.height, Color(dcb1.bgcolor));
    dcbg.init(hwnd, C_SCWIDTH, C_SCHEIGHT, C_BGCOLOR);
    dceraser.init(hwnd, C_SCWIDTH, C_SCHEIGHT, C_BGCOLOR);
    dcbA = &dcb1;
    dcbB = &dcb2;
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
    // background += layerB + layerA
    dcbg.cls();
    BLENDFUNCTION bfB = {AC_SRC_OVER, 0, 0x15, AC_SRC_ALPHA};
    GdiAlphaBlend(
      dcbg.dc, 0, 0, dcbg.width, dcbg.height,
      dcbB->dc, 0, 0, dcbB->width, dcbB->height, bfB);
    StretchBlt(dcbg.dc, dcbg.width, 0, -dcbg.width, dcbg.height,
      dcbg.dc, 0, 0, dcbg.width, dcbg.height, SRCCOPY);
    // eraser = bg
    dceraser.cls();
    BitBlt(dceraser.dc, 0, 0, dceraser.width, dceraser.height,
      dcbg.dc, 0, 0, SRCCOPY);
    dceraser.copyToBitmap(dceraser.bmp);
    // bg += layerA
    GdiTransparentBlt(dcbg.dc, 0, 0, dcbg.width, dcbg.height, dcbA->dc,
      0, 0, dcbA->width, dcbA->height, Color(dcbA->bgcolor).ToCOLORREF());
    // update screen
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
    drawRender(hwnd, dcbA, &dceraser, dwp2, C_DR_DOT);
    return 0;
  }
  case WM_POINTERUPDATE: {
    POINTER_INPUT_TYPE device;
    GetPointerType(GET_POINTERID_WPARAM(wp), &device);
    POINT point = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    ScreenToClient(hwnd, &point);
    dwpa.movePoint(point.x, point.y); // do everytime
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
    if (exitmenu) {
      exitmenu = FALSE;
      PenUI.setCursor(hwnd, dwpa);
    }
    if (nodraw) return 0;
    if (dwpa.pressure) {
      drawRender(hwnd, dcbA, &dceraser, dwpa, C_DR_LINE);
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
      if (LOWORD(wp) == WA_ACTIVE) {
        if (!nodraw) PenUI.setCursor(hwnd, dwpa, C_CS_PEN, FALSE);
      }
    }
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
    if (MessageBox(hwnd, TEXT("exit?"),
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
