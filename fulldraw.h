#include <windows.h>
// common defs (used by *.cpp and *.rc)
#define C_APPVER 0,7,5,1
#define C_APPNAME TEXT("fulldraw")
#define C_APPICON 0x010
#define C_CTXMENU 0x001
#define C_CMD_VERSION 0x002
#define C_CMD_REFRESH 0x003
#define C_CMD_FLIP 0x004
#define C_CMD_CLEAR 0x005
#define C_CMD_MINIMIZE 0x006
#define C_CMD_SAVEAS 0x007
#define C_CMD_EXIT 0x008
#define C_CMD_DRAW 0x009
#define C_CMD_ERASER 0x00A
#define C_CMD_PEN_IN 0x00B
#define C_CMD_PEN_DE 0x00C
#define C_CMD_PRS_IN 0x00D
#define C_CMD_PRS_DE 0x00E
#define C_ID_PEN 0x00F

// string ids: JAPANESE
#define C_STR_JA 0x1000
#define C_STR_JA_PEN C_STR_JA | C_ID_PEN
#define C_STR_JA_ERASER C_STR_JA | C_CMD_ERASER
#define C_STR_JA_PEN_IN C_STR_JA | C_CMD_PEN_IN
#define C_STR_JA_PEN_DE C_STR_JA | C_CMD_PEN_DE
#define C_STR_JA_PRS_IN C_STR_JA | C_CMD_PRS_IN
#define C_STR_JA_PRS_DE C_STR_JA | C_CMD_PRS_DE
#define C_STR_JA_FLIP C_STR_JA | C_CMD_FLIP
#define C_STR_JA_CLEAR C_STR_JA | C_CMD_CLEAR
#define C_STR_JA_REFRESH C_STR_JA | C_CMD_REFRESH
#define C_STR_JA_MINIMIZE C_STR_JA | C_CMD_MINIMIZE
#define C_STR_JA_VERSION C_STR_JA | C_CMD_VERSION
#define C_STR_JA_SAVEAS C_STR_JA | C_CMD_SAVEAS
#define C_STR_JA_EXIT C_STR_JA | C_CMD_EXIT
