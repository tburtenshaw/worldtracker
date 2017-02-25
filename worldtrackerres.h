/* Weditres generated include file. Do NOT edit */
#include <windows.h>
#include <lfc.h>
#define	IDD_ABOUT	100
#define	IDM_NEW	200
#define	IDM_OPEN	210
#define	IDM_EXPORTGPX	211
#define	IDM_SAVE	220
#define	IDM_SAVEAS	230
#define	IDM_CLOSE	240
#define	IDM_PRINT	250
#define	IDM_PAGESETUP	260

#define	IDM_ZOOMIN 270
#define	IDM_ZOOMOUT 271
#define	IDM_ZOOMFIT 272
#define	IDM_ZOOMALL 273
#define IDM_CREATEREGIONFROMVIEW 274
#define IDM_EXPORTPNG 275


#define	IDM_EXIT	300
#define	IDM_ABOUT	500


#define	IDMAINMENU	600
#define	IDAPPLICON	710
#define	IDAPPLCURSOR	810

#define ID_EDITNORTH 1001
#define ID_EDITSOUTH 1002
#define ID_EDITWEST 1003
#define ID_EDITEAST 1004
#define ID_EDITPRESET 1005
#define ID_EDITEXPORTWIDTH 1006
#define ID_EDITEXPORTHEIGHT 1007
#define ID_EDITDATEFROM 1008
#define ID_EDITDATETO 1009
#define ID_EDITTHICKNESS 1010
#define ID_EDITCOLOURBY 1011	//not used
#define ID_EDITCOLOURCYCLE 1012
#define ID_COMBOCOLOURBY 1013
#define ID_EDITONEHOUR 1014
#define ID_EDITONEDAY 1015
#define ID_EDITONEWEEK 1016
#define ID_EDITONEYEAR 1017

#define ID_DROPDOWNPRESET 1101


#define	IDS_FILEMENU	2000
#define	IDS_HELPMENU	2010
#define	IDS_SYSMENU	2030
#define	IDM_STATUSBAR	3000
#define	IDACCEL	10000

#define IDI_WORLDTRACKER 1
#define IDB_OVERVIEW 2
#define IDI_LOCK 3
#define IDB_LOGO 4

/*@ Prototypes @*/
#ifndef WEDIT_PROTOTYPES
#define WEDIT_PROTOTYPES
/*
 * Structure for dialog Dlg100
 */
#endif
void SetDlgBkColor(HWND,COLORREF);
BOOL APIENTRY HandleCtlColor(UINT,DWORD);
/*
 * Callbacks for dialog Dlg100
 */
extern void *GetDialogArguments(HWND);
extern char *GetDico(int,long);
/*@@ End Prototypes @@*/
