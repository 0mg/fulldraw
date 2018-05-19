#include <windows.h>
// common defs (used by *.cpp and *.rc)
#define C_APPVER 0,7,1,0
#define C_APPNAME TEXT("fulldraw")
// icon
#define C_APPICON 0x0001
// menu
#define C_CTXMENU 0x0001
// WM_COMMAND & strings & langs
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

// string ids: DEFAULT
#define C_LANG_DEFAULT 0x0000

// string ids: JAPANESE
#define C_LANG_JA 0x1000
#define C_STR_JA_ERASER C_LANG_JA | C_CMD_ERASER
#define C_STR_JA_PEN_IN C_LANG_JA | C_CMD_PEN_IN
#define C_STR_JA_PEN_DE C_LANG_JA | C_CMD_PEN_DE
#define C_STR_JA_PRS_IN C_LANG_JA | C_CMD_PRS_IN
#define C_STR_JA_PRS_DE C_LANG_JA | C_CMD_PRS_DE
#define C_STR_JA_FLIP C_LANG_JA | C_CMD_FLIP
#define C_STR_JA_CLEAR C_LANG_JA | C_CMD_CLEAR
#define C_STR_JA_REFRESH C_LANG_JA | C_CMD_REFRESH
#define C_STR_JA_MINIMIZE C_LANG_JA | C_CMD_MINIMIZE
#define C_STR_JA_VERSION C_LANG_JA | C_CMD_VERSION
#define C_STR_JA_SAVEAS C_LANG_JA | C_CMD_SAVEAS
#define C_STR_JA_EXIT C_LANG_JA | C_CMD_EXIT
#define C_STR_JA_LANG_DEFAULT C_LANG_JA | C_CMD_LANG_DEFAULT
#define C_STR_JA_LANG_JA C_LANG_JA | C_CMD_LANG_JA
#define C_STR_JA_PEN C_LANG_JA | C_STR_PEN
#define C_STR_JA_LANG C_LANG_JA | C_STR_LANG
#define C_STR_JA_CLEAR_CONFIRM C_LANG_JA | C_STR_CLEAR_CONFIRM
#define C_STR_JA_EXIT_CONFIRM C_LANG_JA | C_STR_EXIT_CONFIRM
#define C_STR_JA_VERSION_TITLE C_LANG_JA | C_STR_VERSION_TITLE
