#include <windows.h>
#include "fulldraw.h"

typedef struct tagKBDMap {
  WORD id;
  WORD key;
  BYTE mod;
} KBDMap;

static class tagKBDAssign {
private:
  KBDMap kmap[99];
  int index = 0;
public:
  void assign(WORD id, TCHAR key, BYTE mod) {
    if (index >= sizeof(kmap) / sizeof(KBDMap)) {
      SetLastError(ERROR_VOLMGR_MAXIMUM_REGISTERED_USERS);
      popError(NULL);
      return;
    }
    KBDMap *tgt = &kmap[index++];
    tgt->id = id;
    tgt->key = key;
    tgt->mod = mod;
  }
  WORD getIdByKBDCmd(TCHAR key, BYTE mod) {
    for (int i = 0; i < index; i++) {
      KBDMap *m = &kmap[i];
      if (m->key == key && m->mod == mod) {
        return m->id;
      }
    }
    return 0;
  }
  void getKBDMap(KBDMap *srcdst) {
    for (int i = 0; i < index; i++) {
      KBDMap *m = &kmap[i];
      if (m->id == srcdst->id) {
        *srcdst = *m;
        return;
      }
    }
  }
} KBDAssign;

enum C_KBD_MOD {
  C_KBD_CTRL = 1,
  C_KBD_SHIFT = 2,
  C_KBD_ALT = 4
};
void strifyKBDCmd(LPTSTR dst, TCHAR key, BYTE mod) {
  LPTSTR alt = mod & C_KBD_ALT ? TEXT("Alt+") : NULL;
  LPTSTR shift = mod & C_KBD_SHIFT ? TEXT("Shift+") : NULL;
  LPTSTR ctrl = mod & C_KBD_CTRL ? TEXT("Ctrl+") : NULL;
  TCHAR ascii[0x10];
  switch (key) {
  case VK_TAB: lstrcpy(ascii, TEXT("Tab")); break;
  case VK_RETURN: lstrcpy(ascii, TEXT("Enter")); break;
  case VK_SPACE: lstrcpy(ascii, TEXT("Space")); break;
  case VK_ESCAPE: lstrcpy(ascii, TEXT("Esc")); break;
  case VK_DELETE: lstrcpy(ascii, TEXT("Del")); break;
  case VK_INSERT: lstrcpy(ascii, TEXT("Ins")); break;
  case VK_UP: lstrcpy(ascii, TEXT("↑")); break;
  case VK_DOWN: lstrcpy(ascii, TEXT("↓")); break;
  case VK_RIGHT: lstrcpy(ascii, TEXT("→")); break;
  case VK_LEFT: lstrcpy(ascii, TEXT("←")); break;
  case VK_F1: lstrcpy(ascii, TEXT("F1")); break;
  case VK_F2: lstrcpy(ascii, TEXT("F2")); break;
  case VK_F3: lstrcpy(ascii, TEXT("F3")); break;
  case VK_F4: lstrcpy(ascii, TEXT("F4")); break;
  case VK_F5: lstrcpy(ascii, TEXT("F5")); break;
  case VK_F6: lstrcpy(ascii, TEXT("F6")); break;
  case VK_F7: lstrcpy(ascii, TEXT("F7")); break;
  case VK_F8: lstrcpy(ascii, TEXT("F8")); break;
  case VK_F9: lstrcpy(ascii, TEXT("F9")); break;
  case VK_F10: lstrcpy(ascii, TEXT("F10")); break;
  case VK_F11: lstrcpy(ascii, TEXT("F11")); break;
  case VK_F12: lstrcpy(ascii, TEXT("F12")); break;
  default: wsprintf(ascii, TEXT("%c"), key); break;
  }
  wsprintf(dst, TEXT("%s%s%s%s"), alt, shift, ctrl, ascii);
}

void setMenuText(HMENU menu, WORD id, BYTE lang, int pos = 0) {
  // get text:"すべて選択" by id
  TCHAR fulltext[199];
  int res = LoadString(GetModuleHandle(NULL),
    lang * 0x1000 | id, fulltext, sizeof(fulltext));
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
  // get text:"Ctrl+A" by id
  KBDMap kmap = {id, 0, 0};
  KBDAssign.getKBDMap(&kmap);
  if (kmap.key) {
    // join text:"すべて選択\tCtrl+A"
    TCHAR keytext[99];
    strifyKBDCmd(keytext, kmap.key, kmap.mod);
    lstrcat(fulltext, TEXT("\t"));
    lstrcat(fulltext, keytext);
  }
  // set menu item text
  SetMenuItemInfo(menu, pos ? pos - 1 : id, !!pos, &MENUITEMINFO {
    sizeof(MENUITEMINFO), MIIM_STRING, 0, 0, 0, 0, 0, 0, 0, fulltext, 0, 0
  });
}
