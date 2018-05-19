#include <windows.h>
#include "fulldraw.h"

// MOD >= Alt+Shift+Ctrl+ \0
// KEY >= PrintScreen \0
// KEYCOMBO >= MOD KEY \0
// MENUTEXT >= ITEM \t KEYCOMBO \0
#define C_MAX_MODTEXT 0x21
#define C_MAX_KEYTEXT 0x11
#define C_MAX_KEYCOMBOTEXT (C_MAX_MODTEXT + C_MAX_KEYTEXT)
#define C_MAX_ITEMTEXT 0xD1
#define C_MAX_MENUTEXT (C_MAX_ITEMTEXT + C_MAX_KEYCOMBOTEXT)
#define C_MAX_MSGTEXT 0x401

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
  const SIZE_T n = C_MAX_KEYTEXT;
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
  TCHAR fulltext[C_MAX_MENUTEXT];
  int res = LoadString(GetModuleHandle(NULL),
    lang | id, fulltext, C_MAX_ITEMTEXT);
  if (!res) {
    // get text:"Select all" default menu text
    GetMenuItemInfo(menu, pos ? pos - 1 : id, !!pos, &MENUITEMINFO {
      sizeof(MENUITEMINFO), MIIM_STRING, 0, 0, 0, 0, 0, 0, 0,
      fulltext, C_MAX_ITEMTEXT, 0
    });
  }
  // get key combo: ('A', C_KBD_CTRL) by id
  HOTKEYDATA kmap = {id, 0, 0};
  Hotkey.getHOTKEYDATA(&kmap);
  if (kmap.key) {
    // get text:"Ctrl+A" by ('A', C_KBD_CTRL)
    TCHAR keytext[C_MAX_KEYCOMBOTEXT];
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
  SIZE_T size = C_MAX_MSGTEXT;
  int res = LoadString(GetModuleHandle(NULL), lang | id, fulltext, size);
  if (!res) {
    LoadString(GetModuleHandle(NULL), id, fulltext, size);
  }
}
