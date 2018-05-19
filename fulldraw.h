#include <windows.h>
// common defs (used by *.cpp and *.rc)
#define C_APPVER 0,7,5,1
#define C_APPNAME TEXT("fulldraw")
// icon
#define C_APPICON 0x0001
// menu
#define C_CTXMENU 0x0001
// WM_COMMAND & strings
#define C_CMD_VERSION 0x001
#define C_CMD_REFRESH 0x002
#define C_CMD_FLIP 0x003
#define C_CMD_CLEAR 0x004
#define C_CMD_MINIMIZE 0x005
#define C_CMD_SAVEAS 0x006
#define C_CMD_EXIT 0x007
#define C_CMD_DRAW 0x008
#define C_CMD_ERASER 0x009
#define C_CMD_PEN_IN 0x00A
#define C_CMD_PEN_DE 0x00B
#define C_CMD_PRS_IN 0x00C
#define C_CMD_PRS_DE 0x00D
#define C_STR_PEN 0x701
#define C_STR_LANG 0x702
#define C_STR_CLEAR_CONFIRM 0x703
#define C_STR_EXIT_CONFIRM 0x704
#define C_STR_VERSION_TITLE 0x705
#define C_CMD_LANG_FIRST 0xFF0
#define C_CMD_LANG_DEFAULT 0xFF0
#define C_CMD_LANG_JA 0xFF1
#define C_CMD_LANG_LAST 0xFFF

// lang mask
#define C_LANG_DEFAULT 0x0000
#define C_LANG_JA 0x1000
#ifdef dev
#endif
