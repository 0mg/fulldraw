#include <windows.h>
#include "fulldraw.h"

typedef struct tagHOTKEYDATA {
  WORD id;
  char key;
  BYTE mod;
} HOTKEYDATA;

static class tagHotkey {
private:
  HOTKEYDATA hotkeylist[99];
  int entries = 0;
public:
  void assign(WORD id, char key, BYTE mod) {
    if (entries >= sizeof(hotkeylist) / sizeof(HOTKEYDATA)) {
      SetLastError(ERROR_VOLMGR_MAXIMUM_REGISTERED_USERS);
      popError(NULL);
      return;
    }
    HOTKEYDATA *tgt = &hotkeylist[entries++];
    tgt->id = id;
    tgt->key = key;
    tgt->mod = mod;
  }
  WORD getCmdIdByKeyCombo(char key, BYTE mod) {
    for (int i = 0; i < entries; i++) {
      HOTKEYDATA *m = &hotkeylist[i];
      if (m->key == key && m->mod == mod) {
        return m->id;
      }
    }
    return 0;
  }
  void getHOTKEYDATA(HOTKEYDATA *srcdst) {
    for (int i = 0; i < entries; i++) {
      HOTKEYDATA *m = &hotkeylist[i];
      if (m->id == srcdst->id) {
        *srcdst = *m;
        return;
      }
    }
  }
} Hotkey;

enum C_KBD_MOD {
  C_KBD_CTRL = 1,
  C_KBD_SHIFT = 2,
  C_KBD_ALT = 4
};
void strifyKeyCombo(LPTSTR dst, char key, BYTE mod) {
  LPTSTR alt = mod & C_KBD_ALT ? TEXT("Alt+") : NULL;
  LPTSTR shift = mod & C_KBD_SHIFT ? TEXT("Shift+") : NULL;
  LPTSTR ctrl = mod & C_KBD_CTRL ? TEXT("Ctrl+") : NULL;
  const SIZE_T n = 0x10;
  TCHAR ascii[n];
  switch (key) {
  case VK_TAB: lstrcpyn(ascii, TEXT("Tab"), n); break;
  case VK_RETURN: lstrcpyn(ascii, TEXT("Enter"), n); break;
  case VK_SPACE: lstrcpyn(ascii, TEXT("Space"), n); break;
  case VK_ESCAPE: lstrcpyn(ascii, TEXT("Esc"), n); break;
  case VK_DELETE: lstrcpyn(ascii, TEXT("Del"), n); break;
  case VK_INSERT: lstrcpyn(ascii, TEXT("Ins"), n); break;
  case VK_UP: lstrcpyn(ascii, TEXT("↑"), n); break;
  case VK_DOWN: lstrcpyn(ascii, TEXT("↓"), n); break;
  case VK_RIGHT: lstrcpyn(ascii, TEXT("→"), n); break;
  case VK_LEFT: lstrcpyn(ascii, TEXT("←"), n); break;
  case VK_F1: lstrcpyn(ascii, TEXT("F1"), n); break;
  case VK_F2: lstrcpyn(ascii, TEXT("F2"), n); break;
  case VK_F3: lstrcpyn(ascii, TEXT("F3"), n); break;
  case VK_F4: lstrcpyn(ascii, TEXT("F4"), n); break;
  case VK_F5: lstrcpyn(ascii, TEXT("F5"), n); break;
  case VK_F6: lstrcpyn(ascii, TEXT("F6"), n); break;
  case VK_F7: lstrcpyn(ascii, TEXT("F7"), n); break;
  case VK_F8: lstrcpyn(ascii, TEXT("F8"), n); break;
  case VK_F9: lstrcpyn(ascii, TEXT("F9"), n); break;
  case VK_F10: lstrcpyn(ascii, TEXT("F10"), n); break;
  case VK_F11: lstrcpyn(ascii, TEXT("F11"), n); break;
  case VK_F12: lstrcpyn(ascii, TEXT("F12"), n); break;
  default: wsprintf(ascii, TEXT("%c"), key); break;
  }
  wsprintf(dst, TEXT("%s%s%s%s"), alt, shift, ctrl, ascii);
}

void setMenuText(HMENU menu, WORD id, WORD lang, int pos = 0) {
  // get text:"すべて選択" by id
  TCHAR fulltext[199];
  int res = LoadString(GetModuleHandle(NULL),
    lang | id, fulltext, sizeof(fulltext));
  if (!res) {
    // get text:"Select all" default menu text
    MENUITEMINFO mii = {
      sizeof(MENUITEMINFO), MIIM_STRING, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    GetMenuItemInfo(menu, pos ? pos - 1 : id, !!pos, &mii);
    mii.dwTypeData = fulltext;
    mii.cch++;
    GetMenuItemInfo(menu, pos ? pos - 1 : id, !!pos, &mii);
  }
  // get key stroke: ('A', C_KBD_CTRL) by id
  HOTKEYDATA kmap = {id, 0, 0};
  Hotkey.getHOTKEYDATA(&kmap);
  if (kmap.key) {
    // get text:"Ctrl+A" by ('A', C_KBD_CTRL)
    TCHAR keytext[99];
    strifyKeyCombo(keytext, kmap.key, kmap.mod);
    // join text:"すべて選択\tCtrl+A"
    lstrcat(fulltext, TEXT("\t"));
    lstrcat(fulltext, keytext);
  }
  // set menu item text
  SetMenuItemInfo(menu, pos ? pos - 1 : id, !!pos, &MENUITEMINFO {
    sizeof(MENUITEMINFO), MIIM_STRING, 0, 0, 0, 0, 0, 0, 0, fulltext, 0, 0
  });
}

void getLocaleString(LPTSTR fulltext, WORD id, WORD lang) {
  SIZE_T size = 0x800;
  int res = LoadString(GetModuleHandle(NULL), lang | id, fulltext, size);
  if (!res) {
    LoadString(GetModuleHandle(NULL), id, fulltext, size);
  }
}
