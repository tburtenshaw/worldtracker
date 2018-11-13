#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string.h>
#include <time.h>

#include "worldtrackerres.h"
#include "mytrips.h"
#include "lodepng.h"
#include "worldtracker.h"
#include "wtgraphs.h"
#include "wt_messages.h"
#include "wttabs.h"

#define SHIFTED 0x8000	//for windows VKs

#define MAX_ASPECT_RATIO 20
#define MIN_ASPECT_RATIO 1/MAX_ASPECT_RATIO


#define IDT_PREVIEWTIMER 1
#define IDT_OVERVIEWTIMER 2

#define OVERVIEW_WIDTH 360
#define OVERVIEW_HEIGHT 180
#define OVB_RESTINGWIDTH 4	//overviewbar width when not adjusting
#define OVB_MOVINGWIDTH 2
#define PCB_GRABWIDTH 10	//Preview Cropbar - the invisible size of the edges


#define MARGIN 12
#define TEXT_HEIGHT 20
#define TEXT_WIDTH_QUARTER (OVERVIEW_WIDTH-3*MARGIN)/4
#define TEXT_WIDTH_THIRD (OVERVIEW_WIDTH-2*MARGIN)/3
#define TEXT_WIDTH_HALF (OVERVIEW_WIDTH-MARGIN)/2
#define TEXT_WIDTH_QSHORT TEXT_WIDTH_QUARTER-MARGIN
#define TEXT_WIDTH_QLONG TEXT_WIDTH_QUARTER+MARGIN
#define TEXT_WIDTH_THREEQUARTERS (OVERVIEW_WIDTH-TEXT_WIDTH_QUARTER-MARGIN)

#define SWATCH_WIDTH_DAY (OVERVIEW_WIDTH-(MARGIN*6))/7
#define SWATCH_WIDTH_MONTH (OVERVIEW_WIDTH-(MARGIN*11))/12


//these don't really need to be the bearing, but I thought it would be easier
#define D_NORTH 0
#define D_SOUTH 180
#define D_WEST 270
#define D_EAST 90

//is the mouse on TO control or FROM
#define MS_DRAGTO 1
#define MS_DRAGFROM 2
#define MS_DRAGBOTH 3

#define CS_MONTH 0x100

typedef struct sMovebar MOVEBARINFO;
typedef struct sStretch STRETCH;


struct sMovebar	{
	int direction;
	float position;
	COLORREF colour;
};

struct sStretch	{
	BOOL useStretch;	//should paint use this to stretchblt
	int   nXOriginDest;	//these are set by mousewheel zoom function
	int   nYOriginDest; //to mimic the look of zooming
	int   nWidthDest;	//without having to recalculate fully
	int   nHeightDest;
	int   nXOriginSrc;
	int   nYOriginSrc;
	int   nWidthSrc;
	int   nHeightSrc;
};


HWND hwndOverview;	//The overview is also used to hold the positions to be drawn and exported
HWND hwndPreview;

//The bars that can be moved to set the viewport
HWND hwndOverviewMovebarNorth;
HWND hwndOverviewMovebarSouth;
HWND hwndOverviewMovebarEast;
HWND hwndOverviewMovebarWest;
//each of these windows has the same windowproc, so the WindowLongPtr is set to some info
MOVEBARINFO mbiNorth;
MOVEBARINFO mbiSouth;
MOVEBARINFO mbiEast;
MOVEBARINFO mbiWest;

//These are the handles for the crop bar - I don't think they'll need a WindowLong
HWND hwndPreviewCropbarWest;
HWND hwndPreviewCropbarEast;
HWND hwndPreviewCropbarNorth;
HWND hwndPreviewCropbarSouth;

HWND hwndMainGraph;

//some of the dialog boxes
HWND hwndButtonRadioOneHour;

HBITMAP hbmOverview;	//this bitmap is generated when the overview is updated.
HBITMAP hbmPreview;

STRETCH stretchPreview;

BM overviewBM;
BM previewBM;

extern HWND hwndTabImport;
extern HWND hwndTabExport;
extern HWND hwndTabRegions;


//Mouse dragging
POINT overviewOriginalPoint;
POINT previewOriginalPoint;

NSWE overviewOriginalNSWE;
NSWE previewOriginalNSWE;

BOOL mouseDragMovebar;	//in the overview
BOOL mouseDragOverview;
BOOL mouseDragPreview;
BOOL mouseDragCropbar;	//in the preview

WORLDREGION *regionFirst;
WORLDREGION *regionLast;
WORLDREGION previewRegion;

//Options are still based on the command line program
OPTIONS optionsOverview;
OPTIONS optionsPreview;
LRESULT CALLBACK OverviewWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK PreviewWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK OverviewMovebarWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK PreviewCropbarWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);

//To intercept keys to the edit boxes
WNDPROC DefEditProc; //remembers the default
WNDPROC DefComboboxProc; //remembers the default combobox proc, apparently things can crash if you don't reset on program close.
WNDPROC DefTabcontrolProc;	//the old tabcontrol proc

LRESULT EditPresetProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);//for editing the presets
LRESULT EditDirectionProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);//for editing N, S, W, E boxes
LRESULT EditDateProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);//for editing dates
LRESULT EditIntegerProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);//for editing integers (thickness, export size, etc, manages up/down,tabs)
LRESULT ComboboxColourByProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);	//the combobox, handles tabs not much else
LRESULT TabsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);	//handles tab key presses from the tabcontrol window


//The bitmaps to display the overview and preview
HBITMAP MakeHBitmapOverview(HWND hwnd, HDC hdc, LOCATIONHISTORY * lh);
HBITMAP MakeHBitmapPreview(HDC hdc, LOCATIONHISTORY * lh, long queuechit);

int PaintOverview(HWND hwnd);
int PaintPreview(HWND hwnd, LOCATIONHISTORY * lh);


int HandleSliderMouse(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int HandlePreviewKeydown(HWND hwnd, WPARAM wParam, LPARAM lParam);
int HandlePreviewMousewheel(HWND hwnd, WPARAM wParam, LPARAM lParam);
int HandlePreviewLbutton(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
int HandleCropbarMouse(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int CreateOverviewMovebarWindows(HWND hwnd);
int CreatePreviewCropbarWindows(HWND hwnd);
int ResetCropbarWindowPos(HWND hwnd);

char* szDayOfWeek[7];
char* szMonthLetter[12];
char *szColourByOption[MAX_COLOURBY_OPTION+1];

OPTIONS optionsBackground;
HBITMAP hbmBackground;
int BltNsweFromBackground(HDC hdc, NSWE * d, int height, int width, OPTIONS * oBkgrnd);
int CreateBackground(HBITMAP * hbm, OPTIONS *oP, OPTIONS *oB, LOCATIONHISTORY * lh);

//time_t ReduceTimeByOneDay(time_t s);
time_t RoundTimeUp(time_t s);
time_t RoundTimeDown(time_t s);



#define MAX_PRESETS 250
PRESET presetArray[MAX_PRESETS];
int numberOfPresets;

void UpdateStatusBar(LPSTR lpszStatusString, WORD partNumber, WORD displayFlags)
{
    SendMessage(hWndStatusbar, SB_SETTEXT, partNumber | displayFlags, (LPARAM)lpszStatusString);
}

//See here: https://cboard.cprogramming.com/windows-programming/67604-capture-enter-key-press-edit-box.html
LRESULT EditIntegerProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	long value;
    char szText[128];
	long min,max;
	long step;

    switch (uMsg)
    {
		case WM_KEYDOWN: //claim the up and down arrow

			switch (wParam)
			{
				case VK_UP:
				case VK_DOWN:
					if (hwnd==hwndEditThickness)	{
						min=1;
						max=100;
						step=1;
						}
					else	{
						min=0;
						max=1000000;
						step=1;
					}

					GetWindowText(hwnd, szText, 128);
					value = strtol(szText,NULL,0);

					if (wParam==VK_UP)	{
						value+=step;
					}
					else if (wParam==VK_DOWN)	{
						value-=step;
					}

					if (value<min)	{value=min;}
					if (value>max)	{value=max;}

					sprintf(szText,"%i",value);
					SetWindowText(hwnd, szText);
					return FALSE;

				case VK_TAB:
					printf("tab");
					if (GetKeyState(VK_SHIFT) & SHIFTED) {	//shift-tab
						if (hwnd==hwndEditThickness) {SetFocus(hwndEditDateTo);}
						if (hwnd==hwndEditColourCycle) {SetFocus(hwndComboboxColourBy);}
						}
					else	{
						if (hwnd==hwndEditThickness) {SetFocus(hwndComboboxColourBy);}
						if (hwnd==hwndEditColourCycle) {SetFocus(hwndButtonRadioOneHour);}
					}
					return FALSE;
			}

		default:
            return CallWindowProc(DefEditProc, hwnd, uMsg, wParam, lParam);

    }

}

LRESULT ComboboxColourByProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
		case WM_KEYDOWN: //claim the up and down arrow

			switch (wParam)
			{
				case VK_TAB:
					if (GetKeyState(VK_SHIFT) & SHIFTED) {SetFocus(hwndEditThickness);}
					else {	//we chose the first
						//int index = SendMessage((HWND) hwnd, (UINT) CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
						switch	(optionsPreview.colourby)	{
							case COLOUR_BY_TIME:
								SetFocus(hwndEditColourCycle);
								break;
							default:
								SetFocus(hwndTab);
								break;
						}
						}
				break;
			}
	}

	return CallWindowProc(DefComboboxProc, hwnd, uMsg, wParam, lParam);
}


LRESULT TabsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
		case WM_KEYDOWN: //claim the up and down arrow

			switch (wParam)
			{
				case VK_TAB:
					printf("Tab");
					if (GetKeyState(VK_SHIFT) & SHIFTED) {SetFocus(hwndComboboxColourBy);}
					else {SetFocus(hwndPreview);}
				break;

			}
	}

	return CallWindowProc(DefTabcontrolProc, hwnd, uMsg, wParam, lParam);
}

LRESULT EditPresetProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
		case WM_KEYDOWN: //claim the up and down arrow

			switch (wParam)
			{
				case VK_DOWN:
					SendMessage(hwndDropdownPreset, WT_WM_PRESETDOWN,0,0);
					ShowWindow(hwndDropdownPreset, SW_SHOW);
					//printf("down");
				return FALSE;
				case VK_UP:
					SendMessage(hwndDropdownPreset, WT_WM_PRESETUP,0,0);
					ShowWindow(hwndDropdownPreset, SW_SHOW);
					//printf("up");
				return FALSE;
				case VK_RETURN:
					if (IsWindowVisible(hwndDropdownPreset))	{
					SendMessage(hwndDropdownPreset, WT_WM_PRESETCHOSEN, 0,0);
					}
					//printf("enter");
				return FALSE;
				case VK_TAB:
					//printf("tab");
					if (GetKeyState(VK_SHIFT) & SHIFTED) {SetFocus(hwndEditEast);}
					else	{SetFocus(hwndEditDateFrom);}
					return FALSE;
				case VK_ESCAPE:
					ShowWindow(hwndDropdownPreset, SW_HIDE);
					return FALSE;
				default:
					ShowWindow(hwndDropdownPreset, SW_SHOW);
			}

		default:
            return CallWindowProc(DefEditProc, hwnd, uMsg, wParam, lParam);

    }
}

LRESULT EditDirectionProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    char szText[128];

	double value;
	double oldvalue;
	double step;

	switch (uMsg)
    {
		case WM_KEYDOWN: //claim the up and down arrow

			switch (wParam)
			{
				case VK_UP:
				case VK_DOWN:
					SendMessage(hwnd, WM_GETTEXT, 128,(long)&szText[0]);
					value=atof(szText);
					oldvalue=value;	//so we can check whether it changed


					if (wParam==VK_DOWN)	{step=-1;}
					else if (wParam==VK_UP)	{step=1;}
					if (GetKeyState(VK_CONTROL) & SHIFTED)	{
						step*=0.1;
					}
					if (GetKeyState(VK_SHIFT) & SHIFTED)	{
						step*=5;
					}

					value+=step;

					if (value<-180)	{value=-180;}
					if (value>180)	{value=180;}

					if ((hwnd==hwndEditNorth)||(hwnd==hwndEditSouth))	{
						if (value<-90)	{value=-90;}
						if (value>90)	{value=90;}
					}

					sprintf(szText,"%f", value);

					if (value!=oldvalue)	{	//only redraw if changed
						SetWindowText(hwnd, szText);

						SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
						UpdateBarsFromNSWE(&optionsPreview.nswe);
					}

				return FALSE;

				case VK_TAB:	//rather complicated way of managing tabs!
					printf("tab");
					SHORT shifttab;
					shifttab=GetKeyState(VK_SHIFT) & SHIFTED;
					if (hwnd==hwndEditNorth)	{
						if (!shifttab)	{SetFocus(hwndEditSouth);}
						else {SetFocus(hwndPreview);}
					}
					else if	(hwnd==hwndEditSouth)	{
						if (!shifttab)	{SetFocus(hwndEditWest);}
						else {SetFocus(hwndEditNorth);}
					}
					else if	(hwnd==hwndEditWest)	{
						if (!shifttab)	{SetFocus(hwndEditEast);}
						else {SetFocus(hwndEditSouth);}
					}
					else if	(hwnd==hwndEditEast)	{
						if (!shifttab)	{SetFocus(hwndEditPreset);}
						else {SetFocus(hwndEditWest);}
					}

					return FALSE;
			}

		default:
            return CallWindowProc(DefEditProc, hwnd, uMsg, wParam, lParam);

    }

}


LRESULT EditDateProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int daystomoveby;	//default will be 7 (a week), control will move one, Shift will move 120, Ctrl-Shift will move 30.
    switch (uMsg)
    {
		case WM_KEYDOWN: //claim the up and down arrow
			if ((GetKeyState(VK_SHIFT) & SHIFTED) && (GetKeyState(VK_CONTROL) & SHIFTED))	{daystomoveby = 30;}
			else if (GetKeyState(VK_SHIFT) & SHIFTED)	{daystomoveby=120;}
			else if (GetKeyState(VK_CONTROL) & SHIFTED) {daystomoveby=1;}
			else {daystomoveby=7;};

			switch (wParam)
			{
				case VK_DOWN:
					SetDateFromControl(hwnd);

					if (hwnd==hwndEditDateFrom)	{
						optionsPreview.fromtimestamp-=daystomoveby*24*60*60;


						if (optionsPreview.fromtimestamp<locationHistory.earliesttimestamp)	{	//avoid going lower than the first date
							optionsPreview.fromtimestamp = locationHistory.earliesttimestamp;
						}
					}
					if (hwnd==hwndEditDateTo)	{
						optionsPreview.totimestamp-=daystomoveby*24*60*60;
						if (optionsPreview.totimestamp<locationHistory.earliesttimestamp)	{	//it should not be smaller than the earliest time of our data
							optionsPreview.totimestamp = locationHistory.earliesttimestamp;
						}
						//and it shouldn't be less than the fromtime
						if (optionsPreview.totimestamp<optionsPreview.fromtimestamp)	{
							optionsPreview.totimestamp = RoundTimeUp(optionsPreview.fromtimestamp);
						}


					}

					UpdateDateControlsFromOptions(&optionsPreview);
					InvalidateRect(hwndDateSlider, NULL, 0);
					SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
					SendMessage(hwndOverview, WT_WM_QUEUERECALC, 5,0);	//5ms delay (rather than higher default)

				return FALSE;
				case VK_UP:
					SetDateFromControl(hwnd);
//					if (GetKeyState(VK_SHIFT) & SHIFTED)	{daystomoveby=7;} else {daystomoveby=1;}
					if (hwnd==hwndEditDateFrom)	{
						optionsPreview.fromtimestamp+=daystomoveby*24*60*60;

						if (optionsPreview.fromtimestamp>locationHistory.latesttimestamp)	{	//shouldn't be later than our data
							optionsPreview.fromtimestamp = locationHistory.latesttimestamp;
						}
						//shouldn't be larger than the to timestamp
						if (optionsPreview.fromtimestamp>optionsPreview.totimestamp)	{
							optionsPreview.fromtimestamp = RoundTimeDown(optionsPreview.totimestamp);	//we round down, as we're the from date should be midnight
						}



					}
					if (hwnd==hwndEditDateTo)	{
						optionsPreview.totimestamp+=daystomoveby*24*60*60;
						if (optionsPreview.totimestamp>locationHistory.latesttimestamp)	{	//shouldn't be later than our data
							optionsPreview.totimestamp = locationHistory.latesttimestamp;
						}

					}

					UpdateDateControlsFromOptions(&optionsPreview);
					InvalidateRect(hwndDateSlider, NULL, 0);
					SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
					SendMessage(hwndOverview, WT_WM_QUEUERECALC, 5,0);

				return FALSE;
				case VK_END:
					if (GetKeyState(VK_CONTROL) & SHIFTED)	{	//control end goes to very last date
						if (hwnd==hwndEditDateTo)	{
							optionsPreview.totimestamp = locationHistory.latesttimestamp;
							UpdateDateControlsFromOptions(&optionsPreview);
							InvalidateRect(hwndDateSlider, NULL, 0);
							SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
							SendMessage(hwndOverview, WT_WM_QUEUERECALC, 0,0);
							return FALSE;
						}
						if (hwnd==hwndEditDateFrom)	{	//Ctrl-End on the from date, brings it to the to date, just at midnight
							optionsPreview.fromtimestamp = RoundTimeDown(optionsPreview.totimestamp);
							UpdateDateControlsFromOptions(&optionsPreview);
							InvalidateRect(hwndDateSlider, NULL, 0);
							SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
							SendMessage(hwndOverview, WT_WM_QUEUERECALC, 0,0);
							return FALSE;
						}
					}
					return CallWindowProc(DefEditProc, hwnd, uMsg, wParam, lParam);

				case VK_HOME:
					if (GetKeyState(VK_CONTROL) & SHIFTED)	{	//control home goes to very first date
						if (hwnd==hwndEditDateFrom)	{
							optionsPreview.fromtimestamp = locationHistory.earliesttimestamp;
							UpdateDateControlsFromOptions(&optionsPreview);
							InvalidateRect(hwndDateSlider, NULL, 0);
							SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
							SendMessage(hwndOverview, WT_WM_QUEUERECALC, 0,0);
							return FALSE;
						}
						if (hwnd==hwndEditDateTo)	{	//Ctrl-Home on the To date, brings it to the "From" date, just at 1 minute to the next midnight
							optionsPreview.totimestamp = RoundTimeUp(optionsPreview.fromtimestamp);
							UpdateDateControlsFromOptions(&optionsPreview);
							InvalidateRect(hwndDateSlider, NULL, 0);
							SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
							SendMessage(hwndOverview, WT_WM_QUEUERECALC, 0,0);
							return FALSE;
						}
					}
					return CallWindowProc(DefEditProc, hwnd, uMsg, wParam, lParam);


				case VK_NEXT:	//Page Down (and Up) moves both the From and To dates, maintaining the gap between them
				case VK_PRIOR:
					if ((hwnd==hwndEditDateTo) || (hwnd==hwndEditDateFrom))	{
						//if (GetKeyState(VK_SHIFT) & SHIFTED)	{daystomoveby=7;} else {daystomoveby=1;}
						SetDateFromControl(hwnd);
						long secondstomoveby;
						secondstomoveby = daystomoveby*24*60*60;

						//if we're already close to the end, and pushing page down, we'll move by a smaller amount
						if (wParam==VK_NEXT)	{
							if ((optionsPreview.totimestamp + secondstomoveby)>locationHistory.latesttimestamp)	{
								secondstomoveby = locationHistory.latesttimestamp-optionsPreview.totimestamp;
							}
							optionsPreview.totimestamp+=secondstomoveby;
							optionsPreview.fromtimestamp+=secondstomoveby;
						}

						if (wParam==VK_PRIOR)	{	//if we're going back in time, don't move more than allowed
							if ((optionsPreview.fromtimestamp - secondstomoveby)<locationHistory.earliesttimestamp)	{
								secondstomoveby = optionsPreview.fromtimestamp-locationHistory.earliesttimestamp;
							}
							optionsPreview.totimestamp-=secondstomoveby;
							optionsPreview.fromtimestamp-=secondstomoveby;
						}



						UpdateDateControlsFromOptions(&optionsPreview);
						InvalidateRect(hwndDateSlider, NULL, 0);
						SendMessage(hwndPreview, WT_WM_QUEUERECALC, 20,0);
						SendMessage(hwndOverview, WT_WM_QUEUERECALC, 5,0);

					}
					return FALSE;

				case VK_RETURN:
					//not sure why I'm doing this, but pressing enter in the field will force a recalc
					SetDateFromControl(hwnd);
					InvalidateRect(hwndDateSlider, NULL, 0);
					SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
					SendMessage(hwndOverview, WT_WM_QUEUERECALC, 0,0);
					return FALSE;
				case VK_TAB:
					if (GetKeyState(VK_SHIFT) & SHIFTED) {	//shift-tab
						if (hwnd==hwndEditDateFrom) {SetFocus(hwndEditPreset);}
						else if (hwnd==hwndEditDateTo) {SetFocus(hwndEditDateFrom);}
						}
					else	{
						if (hwnd==hwndEditDateFrom) {SetFocus(hwndEditDateTo);}
						else if (hwnd==hwndEditDateTo) {SetFocus(hwndEditThickness);}
					}
					return FALSE;
			}

		default:
            return CallWindowProc(DefEditProc, hwnd, uMsg, wParam, lParam);

    }

}




LRESULT MsgMenuSelect(HWND hwnd, UINT uMessage, WPARAM wparam, LPARAM lparam)
{
    static char szBuffer[256];
    UINT   nStringID = 0;
    UINT   fuFlags = GET_WM_MENUSELECT_FLAGS(wparam, lparam) & 0xffff;
    UINT   uCmd    = GET_WM_MENUSELECT_CMD(wparam, lparam);
    HMENU  hMenu   = GET_WM_MENUSELECT_HMENU(wparam, lparam);

    szBuffer[0] = 0;                            // First reset the buffer
    if (fuFlags == 0xffff && hMenu == NULL)     // Menu has been closed
        nStringID = 0;

    else if (fuFlags & MFT_SEPARATOR)           // Ignore separators
        nStringID = 0;

    else if (fuFlags & MF_POPUP)                // Popup menu
    {
        if (fuFlags & MF_SYSMENU)               // System menu
            nStringID = IDS_SYSMENU;
        else
            // Get string ID for popup menu from idPopup array.
            nStringID = 0;
    }  // for MF_POPUP
    else                                        // Must be a command item
        nStringID = uCmd;                       // String ID == Command ID

    // Load the string if we have an ID
    if (0 != nStringID)
        LoadString(hInst, nStringID, szBuffer, sizeof(szBuffer));
    // Finally... send the string to the status bar
    UpdateStatusBar(szBuffer, 0, 0);
    return 0;
}

void InitializeStatusBar(HWND hwndParent,int nrOfParts)
{
	int   ptArray[3];   // Array defining the number of parts/sections
    RECT  rect;

    GetClientRect(hwndParent, &rect);

    ptArray[0] = rect.right/2;
    ptArray[1] = rect.right*3/4;
	ptArray[2] = rect.right;
    //---TODO--- Add code to calculate the size of each part of the status
    // bar here.


	SendMessage(hWndStatusbar, SB_SETPARTS, nrOfParts, (LPARAM)(LPINT)ptArray);
	SendMessage(hWndStatusbar, SB_SIMPLE, FALSE, 0);
    UpdateStatusBar("Ready", 0, 0);
//    UpdateStatusBar("2", 1, 0);
//    UpdateStatusBar("3", 2, 0);

//	SendMessage(hWndStatusbar, SB_SETTEXT,1, (LPARAM)L"Ready");

}



static BOOL CreateSBar(HWND hwndParent,char *initialText,int nrOfParts)
{
    hWndStatusbar = CreateStatusWindow(WS_CHILD | WS_VISIBLE | WS_BORDER|SBARS_SIZEGRIP,
                                       initialText,
                                       hwndParent,
                                       IDM_STATUSBAR);
    if(hWndStatusbar)
    {
        InitializeStatusBar(hwndParent,nrOfParts);
        return TRUE;
    }

    return FALSE;
}

int GetFileName(char *buffer,int buflen)
{
	OPENFILENAME ofn;

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hInstance = GetModuleHandle(NULL);
	ofn.hwndOwner = GetActiveWindow();
	ofn.lpstrFile = buffer;
	ofn.nMaxFile = buflen;
	ofn.lpstrTitle = "Import";
	ofn.nFilterIndex = 2;
	strcpy(buffer, "*.json; *.log; *.csv");


	ofn.Flags = OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY|OFN_ALLOWMULTISELECT|OFN_EXPLORER;
	ofn.lpstrFilter = "All Files (*.*)\0*.*\0All Supported Files (*.json; *.log; *.csv)\0*.json;*.log;*.csv\0Google History JSON Files (*.json)\0*.json\0Canon Camera NMEA Files (*.log)\0*.log\0Backitude CSV Files (*.csv)\0*.csv\0\0";

	return GetOpenFileName(&ofn);

}


void InitiateColours(void)
{
	cBlack.R=cBlack.G=cBlack.B = 0x00;
	cBlack.A=0xFF;

	cWhite.R=cWhite.G=cWhite.B=cWhite.A=0xFF;
	cRed.R=0xFF;cRed.G=0;cRed.B=0; cRed.A=0xFF;

	cDaySwatch[0].R=0x32;	cDaySwatch[0].G=0x51;	cDaySwatch[0].B=0xA7;	cDaySwatch[0].A=0xFF;
	cDaySwatch[1].R=0xc0;	cDaySwatch[1].G=0x46;	cDaySwatch[1].B=0x54;	cDaySwatch[1].A=0xFF;
	cDaySwatch[2].R=0xe1;	cDaySwatch[2].G=0x60;	cDaySwatch[2].B=0x3d;	cDaySwatch[2].A=0xFF;
	cDaySwatch[3].R=0xe4;	cDaySwatch[3].G=0xb7;	cDaySwatch[3].B=0x4a;	cDaySwatch[3].A=0xFF;
	cDaySwatch[4].R=0xa1;	cDaySwatch[4].G=0xfc;	cDaySwatch[4].B=0x58;	cDaySwatch[4].A=0xFF;
	cDaySwatch[5].R=0x96;	cDaySwatch[5].G=0x54;	cDaySwatch[5].B=0xa9;	cDaySwatch[5].A=0xFF;
	cDaySwatch[6].R=0x00;	cDaySwatch[6].G=0x82;	cDaySwatch[6].B=0x94;	cDaySwatch[6].A=0xFF;


	cMonthSwatch[0].R=0xA6;	cMonthSwatch[0].G=0xCE;	cMonthSwatch[0].B=0xE3;	cMonthSwatch[0].A=0xFF;
	cMonthSwatch[1].R=0x1F;	cMonthSwatch[1].G=0x78;	cMonthSwatch[1].B=0xB4;	cMonthSwatch[1].A=0xFF;
	cMonthSwatch[2].R=0xB2;	cMonthSwatch[2].G=0xDF;	cMonthSwatch[2].B=0x8A;	cMonthSwatch[2].A=0xFF;
	cMonthSwatch[3].R=0x33;	cMonthSwatch[3].G=0xA0;	cMonthSwatch[3].B=0x2C;	cMonthSwatch[3].A=0xFF;
	cMonthSwatch[4].R=0xFB;	cMonthSwatch[4].G=0x9A;	cMonthSwatch[4].B=0x99;	cMonthSwatch[4].A=0xFF;
	cMonthSwatch[5].R=0xE3;	cMonthSwatch[5].G=0x1A;	cMonthSwatch[5].B=0x1C;	cMonthSwatch[5].A=0xFF;
	cMonthSwatch[6].R=0xFD;	cMonthSwatch[6].G=0xBF;	cMonthSwatch[6].B=0x6F;	cMonthSwatch[6].A=0xFF;
	cMonthSwatch[7].R=0xFF;	cMonthSwatch[7].G=0x7F;	cMonthSwatch[7].B=0x00;	cMonthSwatch[7].A=0xFF;
	cMonthSwatch[8].R=0xCA;	cMonthSwatch[8].G=0xB2;	cMonthSwatch[8].B=0xD6;	cMonthSwatch[8].A=0xFF;
	cMonthSwatch[9].R=0x6A;	cMonthSwatch[9].G=0x3D;	cMonthSwatch[9].B=0x9A;	cMonthSwatch[9].A=0xFF;
	cMonthSwatch[10].R=0xFF;	cMonthSwatch[10].G=0xFF;	cMonthSwatch[10].B=0x99;	cMonthSwatch[10].A=0xFF;
	cMonthSwatch[11].R=0xB1;	cMonthSwatch[11].G=0x59;	cMonthSwatch[11].B=0x28;	cMonthSwatch[11].A=0xFF;


	return;
}

void InitStrings(void)
{
	szMonthLetter[0]="J";
	szMonthLetter[1]="F";
	szMonthLetter[2]="M";
	szMonthLetter[3]="A";
	szMonthLetter[4]="M";
	szMonthLetter[5]="J";
	szMonthLetter[6]="J";
	szMonthLetter[7]="A";
	szMonthLetter[8]="S";
	szMonthLetter[9]="O";
	szMonthLetter[10]="N";
	szMonthLetter[11]="D";



	szDayOfWeek[0]="Sun";
	szDayOfWeek[1]="Mon";
	szDayOfWeek[2]="Tue";
	szDayOfWeek[3]="Wed";
	szDayOfWeek[4]="Thu";
	szDayOfWeek[5]="Fri";
	szDayOfWeek[6]="Sat";

	szColourByOption[COLOUR_BY_TIME]="Time";
	szColourByOption[COLOUR_BY_SPEED]="Speed";
	szColourByOption[COLOUR_BY_ACCURACY]="Accuracy";
	szColourByOption[COLOUR_BY_DAYOFWEEK]="Day";
	szColourByOption[COLOUR_BY_HOUR]="Hour";
	szColourByOption[COLOUR_BY_MONTH]="Month";
	szColourByOption[COLOUR_BY_GROUP]="Group";
	szColourByOption[COLOUR_BY_FADEOVERTIME] = "Fade";

	return;
}

int InitWindowClasses(void)
{
	WNDCLASS wc;

	//Make the Window Classes
	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC)MainWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszClassName = "worldtrackerWndClass";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	wc.hIcon = LoadIcon(hInst,MAKEINTRESOURCE(IDI_WORLDTRACKER));
	if (!RegisterClass(&wc))
		return 0;

	//The Overview window class
	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = (WNDPROC)OverviewWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
	wc.lpszClassName = "OverviewClass";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;

	//The Preview window class
	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)PreviewWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
	wc.lpszClassName = "PreviewClass";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL,IDC_HAND);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;

	//The class for movable bar to select position in Overview
	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)OverviewMovebarWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "OverviewMovebarClass";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL,IDC_SIZEWE);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;

	//PreviewCrop bars	(dragged from the side of the preview to crop it)
	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)PreviewCropbarWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "PreviewCropbarClass";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL,IDC_SIZEWE);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;

	//Date slider
	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)DateSliderWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "DateSlider";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;

	//Colour swatch
	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)ColourSwatchWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "ColourSwatch";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;

	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)ColourByWeekdayWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "ColourByWeekday";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;


	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)ColourByMonthWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "ColourByMonth";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;

	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)ColourByDateWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "ColourByDate";
	wc.lpszMenuName = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;


	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)DropdownPresetWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "DropdownPreset";
	wc.lpszMenuName = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;



	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)GraphWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "MainGraph";
	wc.lpszMenuName = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;


	if (!InitTabWindowClasses())	{	//in wt_tabs.c
		return 0;
	}


	return 1;
}

static BOOL InitApplication(void)
{
	hbmQueueSize=0;

	//Set locationHistory to NULL
	memset(&locationHistory,0,sizeof(locationHistory));
	memset(&previewBM,0,sizeof(previewBM));

	memset(&optionsPreview,0,sizeof(optionsPreview));
	memset(&optionsOverview,0,sizeof(optionsPreview));

	//default view
	optionsPreview.nswe.north=90;
	optionsPreview.nswe.south=-90;
	optionsPreview.nswe.west=-180;
	optionsPreview.nswe.east=180;
	optionsPreview.thickness=1;
	optionsPreview.colourcycle=60*60*24*7;
	optionsPreview.colourby = COLOUR_BY_TIME;
	optionsPreview.colourextra = cDaySwatch;

	InitiateColours();

	//set dialog font
	int height;
	HDC hdc;
	hdc=GetDC(0);
	height = -MulDiv(10, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	ReleaseDC(0,hdc);
	hFontDialog = CreateFont(height, 0, 0, 0, FW_DONTCARE, FALSE,
    FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
    CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_DONTCARE,
    "Tahoma");


	/*
COLOUR c;
NSWE nswe;


	regionFirst = NULL;
	regionLast = NULL;


	c.R = 255;
	c.G = 156;
	c.B = 0;
	c.A=75;
	nswe.north=-37.015956;
	nswe.south=-37.025932;
	nswe.west=174.889670;
	nswe.east=174.903095;


	regionLast = CreateRegionAfter(regionLast, &nswe, "Home", REGIONTYPE_HOME, &c, &regionFirst);
	if (!regionFirst)	{
		regionFirst = regionLast;
	}


	nswe.north=-36.843975;
	nswe.south=-36.857656;
	nswe.west=174.757584;
	nswe.east=174.774074;
	regionLast = CreateRegionAfter(regionLast, &nswe, "Away", REGIONTYPE_AWAY, &c, &regionFirst);
	if (!regionFirst)	{
		regionFirst = regionLast;
	}



c.R = 160;
c.G = 10;
c.B = 10;
c.A=180;


nswe.north = -36.865730;
nswe.south =-37.087670;
nswe.west =174.966750;
nswe.east =175.095146;
	regionLast = CreateRegionAfter(regionLast, &nswe, NULL, REGIONTYPE_EXCLUSION, &c, &regionFirst);
	if (!regionFirst)	{
		regionFirst = regionLast;
	}


nswe.north =-36.804920;
nswe.south =-36.885830;
nswe.west =174.822950;
nswe.east =174.924479;

	regionLast = CreateRegionAfter(regionLast, &nswe, NULL, REGIONTYPE_EXCLUSION, &c, &regionFirst);
	if (!regionFirst)	{
		regionFirst = regionLast;
	}


nswe.north =-36.877190;
nswe.south =-36.968430;
nswe.west =174.877340;
nswe.east =174.979382;

	regionLast = CreateRegionAfter(regionLast, &nswe, NULL, REGIONTYPE_EXCLUSION, &c, &regionFirst);
	if (!regionFirst)	{
		regionFirst = regionLast;
	}

nswe.north =-36.992380;
nswe.south =-37.131420;
nswe.west =174.531930;
nswe.east =174.810390;

	regionLast = CreateRegionAfter(regionLast, &nswe, NULL, REGIONTYPE_EXCLUSION, &c, &regionFirst);
	if (!regionFirst)	{
		regionFirst = regionLast;
	}

nswe.north =-36.497400;
nswe.south =-36.989100;
nswe.west =173.663100;
nswe.east =174.649000;


	regionLast = CreateRegionAfter(regionLast, &nswe, NULL, REGIONTYPE_EXCLUSION, &c, &regionFirst);
	if (!regionFirst)	{
		regionFirst = regionLast;
	}

*/

	//Sets strings
	InitStrings();

	//Load presets into an array
	LoadPresets(presetArray, &numberOfPresets, MAX_PRESETS);

	//Register the Window Classes used
	InitWindowClasses();


	return 1;
}

BOOL _stdcall AboutDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
		case WM_CLOSE:
			EndDialog(hwnd,0);
			return 1;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					EndDialog(hwnd,1);
					return 1;
			}
			break;
	}
	return 0;
}

int OpenFiles(void)
{
	OPENFILENAME ofn;
	char buffer[2048];
//	int offset;

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hInstance = GetModuleHandle(NULL);
	ofn.hwndOwner = GetActiveWindow();
	ofn.lpstrFile = buffer;
	ofn.nMaxFile = 2048;
	ofn.lpstrTitle = "Import";
	ofn.nFilterIndex = 2;
	strcpy(buffer, "*.json; *.log; *.csv");
	ofn.Flags = OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY|OFN_ALLOWMULTISELECT|OFN_EXPLORER;
	ofn.lpstrFilter = "All Files (*.*)\0*.*\0All Supported Files (*.json; *.log; *.csv)\0*.json;*.log;*.csv\0Google History JSON Files (*.json)\0*.json\0Canon Camera NMEA Files (*.log)\0*.log\0Backitude CSV Files (*.csv)\0*.csv\0\0";

	if (!GetOpenFileName(&ofn))	{
		return 0;
	}

	//copy this info to the global variable, so it can be loaded by the loading thread
	LoadingThreadParameter.nFileOffset = ofn.nFileOffset;
	memcpy(LoadingThreadParameter.loadfilebuffer, buffer, 2048);
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LoadingThread, &LoadingThreadParameter, 0, NULL);

	return 1;
}

void MainWndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	switch(id) {
		case IDM_ABOUT:
			DialogBox(hInst,MAKEINTRESOURCE(IDD_ABOUT),
				hWndMain,AboutDlg);
			break;

		case IDM_OPEN:
			OpenFiles();
			return;

		break;

		case IDM_SAVE:
			ExportKMLDialogAndComplete(hwnd, &optionsPreview);
		break;

		case IDM_EXPORTGPX:
			ExportGPXDialogAndComplete(hwnd, &locationHistory);
		break;

		case IDM_CLOSE:
			FreeLocations(&locationHistory);
			memset(&locationHistory,0,sizeof(locationHistory));
			SendMessage(hwndOverview, WT_WM_QUEUERECALC, 0,0);
			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
			SendMessage(hwndTabImport, WT_WM_TAB_RESETCONTENT, 0, 0);
			InvalidateRect(hwndDateSlider, NULL, 0);
		break;

		case IDM_EXIT:
			PostMessage(hwnd,WM_CLOSE,0,0);
		break;

		case IDM_ZOOMALL:
			optionsPreview.nswe.north = 90;
			optionsPreview.nswe.south = -90;
			optionsPreview.nswe.west = -180;
			optionsPreview.nswe.east = 180;

			ConstrainNSWE(&optionsPreview.nswe);		//rather than constrain after the move, I need to stop while moving
			SendMessage(hwnd, WT_WM_RECALCBITMAP, 0,0);
			UpdateBarsFromNSWE(&optionsPreview.nswe);
			UpdateEditNSWEControls(&optionsPreview.nswe);
		break;
		case IDM_ZOOMFIT:
			CopyNSWE(&optionsPreview.nswe, &locationHistory.bounds);
			SendMessage(hwnd, WT_WM_RECALCBITMAP, 0,0);
			UpdateBarsFromNSWE(&optionsPreview.nswe);
			UpdateEditNSWEControls(&optionsPreview.nswe);
		break;
		case IDM_EXPORTPNG:
			DialogHBMtoPNG(hwnd, &previewBM, "test.png");
			break;

		case IDM_CREATEREGIONFROMVIEW:
			printf("\nCreate region from view");
			COLOUR c;

			c.R = 25;
			c.G = 250;
			c.B = 70;
			c.A=115;

			regionLast = CreateRegionAfter(regionLast, &optionsPreview.nswe, "Home", REGIONTYPE_HOME, &c, &regionFirst);
			SendMessage(hwnd, WT_WM_RECALCBITMAP, 0,0);
			SendMessage(hwndTabRegions,WT_WM_TAB_REGIONADDTOLIST,0,(LPARAM)regionLast);
			break;
		case IDM_CONSTRAINTIMETOVIEW:
			DateRangeInNSWE(&locationHistory, &optionsPreview.nswe,  &optionsPreview.fromtimestamp, &optionsPreview.totimestamp);
			optionsPreview.fromtimestamp = RoundTimeDown(optionsPreview.fromtimestamp);
			optionsPreview.totimestamp = RoundTimeUp(optionsPreview.totimestamp);
			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
			SendMessage(hwndOverview, WT_WM_QUEUERECALC, 0,0);
			UpdateDateControlsFromOptions(&optionsPreview);
			InvalidateRect(hwndDateSlider, NULL, 0);
			break;



		case ID_EDITNORTH:
		case ID_EDITSOUTH:
		case ID_EDITEAST:
		case ID_EDITWEST:
			HandleEditControls(hwnd, id, hwndCtl, codeNotify);
		break;
		case ID_EDITEXPORTHEIGHT:
		case ID_EDITEXPORTWIDTH:
		case ID_EDITPRESET:
		case ID_EDITTHICKNESS:
			HandleEditControls(hwnd, id, hwndCtl, codeNotify);
		break;

		case ID_EDITCOLOURCYCLE:
		case ID_EDITONEHOUR:
		case ID_EDITONEDAY:
		case ID_EDITONEWEEK:
		case ID_EDITONEYEAR:
			HandleColourCycleRadiobuttons(hwnd, id, hwndCtl, codeNotify);
			break;

		case ID_EDITDATETO:
		case ID_EDITDATEFROM:
			HandleEditDateControls(hwnd, id, hwndCtl, codeNotify);
		break;
		case ID_COMBOCOLOURBY:
			HandleComboColourBy(hwnd, id, hwndCtl, codeNotify);
		break;
	}
}

int HandleColourCycleRadiobuttons(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	int bst;
	long seconds;
	char secondsstring[128];

	if ((id == ID_EDITCOLOURCYCLE) && codeNotify==EN_CHANGE)	{
		SendMessage(hwndCtl, WM_GETTEXT, 128,(long)&secondsstring[0]);
		seconds=atof(secondsstring);

		if (seconds>0)	{
			optionsPreview.colourcycle=seconds;
		}
		if (seconds==3600)
			SendMessage(GetDlgItem(GetParent(hwndCtl), ID_EDITONEHOUR), BM_SETCHECK, BST_CHECKED,0);
		else
			SendMessage(GetDlgItem(GetParent(hwndCtl), ID_EDITONEHOUR), BM_SETCHECK, BST_UNCHECKED,0);

		if (seconds==3600*24)
			SendMessage(GetDlgItem(GetParent(hwndCtl), ID_EDITONEDAY), BM_SETCHECK, BST_CHECKED,0);
		else
			SendMessage(GetDlgItem(GetParent(hwndCtl), ID_EDITONEDAY), BM_SETCHECK, BST_UNCHECKED,0);

		if (seconds==3600*24*7)
			SendMessage(GetDlgItem(GetParent(hwndCtl), ID_EDITONEWEEK), BM_SETCHECK, BST_CHECKED,0);
		else
			SendMessage(GetDlgItem(GetParent(hwndCtl), ID_EDITONEWEEK), BM_SETCHECK, BST_UNCHECKED,0);

		if (seconds==3600*24*365.2425)
			SendMessage(GetDlgItem(GetParent(hwndCtl), ID_EDITONEYEAR), BM_SETCHECK, BST_CHECKED,0);
		else
			SendMessage(GetDlgItem(GetParent(hwndCtl), ID_EDITONEYEAR), BM_SETCHECK, BST_UNCHECKED,0);



		SendMessage(hwndOverview, WT_WM_QUEUERECALC, 0,0);
		SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
		return 0;
	}



	if (codeNotify == BN_CLICKED)	{
		bst = SendMessage(hwndCtl, BM_GETCHECK, 0, 0);
		if (bst == BST_UNCHECKED)	{
			SendMessage(hwndCtl, BM_SETCHECK, BST_CHECKED,0);
			if (id == ID_EDITONEHOUR)
				seconds = 3600;
			if (id == ID_EDITONEDAY)
				seconds = 3600*24;
			if (id == ID_EDITONEWEEK)
				seconds = 3600*24*7;
			if (id == ID_EDITONEYEAR)
				seconds = 3600*24*365.2425;

			optionsPreview.colourcycle=seconds;
			SendMessage(hwndOverview, WT_WM_QUEUERECALC, 0,0);
			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
			sprintf(secondsstring, "%i", seconds);
			SetWindowText(hwndEditColourCycle, secondsstring);

			for (int i=ID_EDITONEHOUR; i<=ID_EDITONEYEAR; i++)	{	//go through all the checkboxes
				if (i!=id)	{	//except for the clicked one
					SendMessage(GetDlgItem(GetParent(hwndCtl), i), BM_SETCHECK, BST_UNCHECKED,0);	//and uncheck them
				}
			}
		}
	}
	return 0;
}



void UpdateProgressBar(int progress)	//dummy for the progress bar
{
	char szBuffer[255];
	sprintf(szBuffer, "Loading file... (%i%%)", progress*100/256);

	UpdateStatusBar(szBuffer, 0, 0);
	return;
}

//This is the thread that loads the file
DWORD WINAPI LoadingThread(LOADINGTHREADPARAMETER *ltParam)
{
	char dir[MAX_PATH];
	char filename[MAX_PATH];
	char fullpath[MAX_PATH];

	UpdateStatusBar("Loading file...", 0, 0);

	memcpy(dir, ltParam->loadfilebuffer, ltParam->nFileOffset);
	dir[ltParam->nFileOffset] = 0;
	if (dir[ltParam->nFileOffset-1]!='\\')	{
//		printf("noslash %c %i",dir[ltParam->nFileOffset-1],ltParam->nFileOffset);
		dir[ltParam->nFileOffset-1]='\\';
		dir[ltParam->nFileOffset]=0;
	}

	while (ltParam->loadfilebuffer[ltParam->nFileOffset])	{
		strcpy(filename, ltParam->loadfilebuffer+ltParam->nFileOffset);
		ltParam->nFileOffset+=strlen(filename)+1;

		sprintf(fullpath, "%s%s", dir, filename);
//		printf("\nDir: %s\nFilename:%s %i",  dir,filename, ltParam->nFileOffset);



		typedef int (__cdecl *MYPROC)(int, char *);
		HMODULE lib;
		MYPROC procSHAddToRecentDocs;

		lib = LoadLibrary("Shell32.dll");
		//printf("\nlib: %i",lib);
		if (lib)	{
			procSHAddToRecentDocs = (MYPROC)GetProcAddress(lib, "SHAddToRecentDocs");
		}
		//printf("\nproc: %i",proc);
		#define SHARD_PATHA 2

		if (procSHAddToRecentDocs)	{
			(procSHAddToRecentDocs)(SHARD_PATHA, fullpath);
		}

		printf("\nFull path%s", fullpath);

    EnterCriticalSection(&critAccessLocations);
	LoadLocations(&locationHistory, fullpath, UpdateProgressBar);
	LeaveCriticalSection(&critAccessLocations);

	//Add to listbox
	SendMessage(hwndTabImport, WT_WM_TAB_ADDLASTIMPORTEDFILE, 0, (LPARAM)&locationHistory);
	//Queue redraw

	}




	//Once loaded, tell the windows they can update
    UpdateStatusBar("Ready", 0, 0);

	optionsPreview.fromtimestamp = locationHistory.earliesttimestamp;
	optionsPreview.totimestamp = locationHistory.latesttimestamp;

	SendMessage(hwndOverview, WT_WM_RECALCBITMAP, 0,0);
	SendMessage(hwndPreview, WT_WM_RECALCBITMAP, 0,0);


	UpdateDateControlsFromOptions(&optionsPreview);

	InvalidateRect(hwndDateSlider, NULL, 0);

	return 0;
}


int UpdateEditNSWEControls(NSWE * d)
{
	char buffer[256];

	sprintf(buffer,"%f", d->north);
	SetWindowText(hwndEditNorth, buffer);
	sprintf(buffer,"%f", d->south);
	SetWindowText(hwndEditSouth, buffer);
	sprintf(buffer,"%f", d->west);
	SetWindowText(hwndEditWest, buffer);
	sprintf(buffer,"%f", d->east);
	SetWindowText(hwndEditEast, buffer);

	CopyNSWE(&previewRegion.nswe, d);

	return 0;
}


int HandleComboColourBy(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	int index;
	int show;


	if (codeNotify == CBN_SELCHANGE)	{
		index = SendMessage((HWND) hwndCtl, (UINT) CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
		//printf("combo %i, ", index);
		if (optionsPreview.colourby != index)	{
			optionsPreview.colourby = index;
			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
			SendMessage(hwndOverview, WT_WM_QUEUERECALC, 0,0);


			if (index == COLOUR_BY_TIME)
				show = SW_SHOW;
			else
				show = SW_HIDE;
			ShowWindow(hwndColourByDate, show);

			if (index == COLOUR_BY_DAYOFWEEK)	{
				show = SW_SHOW;
				optionsPreview.colourextra = cDaySwatch;
			}
			else
				show = SW_HIDE;
			ShowWindow(hwndColourByWeekday, show);

			if (index == COLOUR_BY_MONTH)	{
				show = SW_SHOW;
				optionsPreview.colourextra = cMonthSwatch;
			}
			else
				show = SW_HIDE;
			ShowWindow(hwndColourByMonth, show);





		}
	}

	return 0;
}



//Thanks Kit Fisto from Stack Overflow for the next two functions!
//https://stackoverflow.com/questions/12353011/how-to-convert-a-utc-date-time-to-a-time-t-in-c

const int SecondsPerMinute = 60;
const int SecondsPerHour = 3600;
const int SecondsPerDay = 86400;
const int DaysOfMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

BOOL IsLeapYear(short year)
{
    if (year % 4 != 0) return FALSE;
    if (year % 100 != 0) return TRUE;
    return (year % 400) == 0;
}

static time_t mkgmtime(const struct tm *ptm) {
    time_t secs = 0;
    // tm_year is years since 1900
    int year = ptm->tm_year + 1900;
    for (int y = 1970; y < year; ++y) {
        secs += (IsLeapYear(y)? 366: 365) * SecondsPerDay;
    }
    // tm_mon is month from 0..11
    for (int m = 0; m < ptm->tm_mon; ++m) {
        secs += DaysOfMonth[m] * SecondsPerDay;
        if (m == 1 && IsLeapYear(year)) secs += SecondsPerDay;
    }
    secs += (ptm->tm_mday - 1) * SecondsPerDay;
    secs += ptm->tm_hour       * SecondsPerHour;
    secs += ptm->tm_min        * SecondsPerMinute;
    secs += ptm->tm_sec;
    return secs;
}


BOOL SetDateFromControl(HWND hwndCtl)	//returns whether to redraw
{
	char szText[128];
	char *pEnd;
	int year, month, day;

	struct tm time;
	time_t	t;

	SendMessage(hwndCtl, WM_GETTEXT, 128,(long)&szText[0]);
	szText[16]=0;	//we've got up to 128 characters, but we'll make sure to truncate well before then.
	printf("\nBefore: %s\t",szText);



	//Need to check there's a proper date inserted
	char firstnumber[12];
	char secondnumber[12];
	char thirdnumber[12];

	int readingnumber=0;
	int waslastreadnumeral=0;	//was the last thing we read a numeral

	int p;	//where we are in the number string

	memset(firstnumber,0,12);
	memset(secondnumber,0,12);
	memset(thirdnumber,0,12);

	int c;
	c=0;
	while (szText[c])	{	//read through until zero-termination
		if ((szText[c]>=0x30)&&(szText[c]<=0x39))	{	//we have a numeral
//			printf("\n%i,'%c',%i,%i %i",c,szText[c], waslastreadnumeral, readingnumber,p);
			if (waslastreadnumeral==0)	{
				waslastreadnumeral=1;
				readingnumber++;
				p=0;
			}
		}	else	{
			waslastreadnumeral=0;
			p=0;
		}

		if (waslastreadnumeral==1)	{
			if (readingnumber==1)	{
				firstnumber[p]=szText[c];
			}
			if (readingnumber==2)	{
				secondnumber[p]=szText[c];
			}
			if (readingnumber==3)	{
				thirdnumber[p]=szText[c];
			}
			p++;
			if (p>11)	{p=11;}	//don't allow the string to overrun

		}

		c++;

	}

	printf("%s %s %s,",firstnumber,secondnumber,thirdnumber);

	if (strlen(firstnumber)==0)	{	//if the whole thing is blank
		year=2010; month=1;day=1;
	}


	if (strlen(secondnumber)==0)	{	//if there's only one number
		//check whether its 31122001 or a unix timestamp, or 20011231 or 011231 or 311201 etc
	}
	else	{	//assume YMD for now
		year=strtoul(firstnumber, NULL, 10);
		month=strtoul(secondnumber, NULL, 10);
		day=strtoul(thirdnumber, NULL, 10);
	}

	//This gets a Y-M-D
	//year = strtoul(szText,&pEnd,10);
	//month = strtoul(pEnd,&pEnd,10);
	//day = strtoul(pEnd,&pEnd,10);
//	if (month<0) month*=-1;
//	if (day<0) day*=-1;

	time.tm_isdst = -1;

	time.tm_year = year-1900;
	time.tm_mon = month-1;
	time.tm_mday = day;
	if (hwndCtl == hwndEditDateFrom)	{
 		time.tm_sec = time.tm_min = time.tm_hour = 0;
		printf("current time: %i %i-%i-%i\t", optionsPreview.fromtimestamp,time.tm_year, time.tm_mon, time.tm_mday);
		//time.tm_sec =1;
	}
	else if (hwndCtl == hwndEditDateTo)	{	//we want to go all the way to the end of the day
		time.tm_hour = 23;
		time.tm_min = 59;
 		time.tm_sec=59;
	}



//	t=mktime(&time);
	t=mkgmtime(&time);
	printf("Time: %i\t", t);



	struct tm timeprint;
	gmtime_s(&t, &timeprint);
	strftime (szText, 256, "%Y-%m-%d %z", &timeprint);	//ISO8601YYYY-MM-DD
	printf("After: %s\n",szText);



	if (t<locationHistory.earliesttimestamp)	{
		t=locationHistory.earliesttimestamp;
	}

	if (t>locationHistory.latesttimestamp)	{
		t=locationHistory.latesttimestamp;
	}

	if (hwndCtl == hwndEditDateFrom)	{
		if (optionsPreview.fromtimestamp!=t)	{
			optionsPreview.fromtimestamp=t;
			return TRUE;	//we need to redraw as the control has changed value
		}
	}
	if (hwndCtl == hwndEditDateTo)	{
		if (optionsPreview.totimestamp!=t)	{	//if there's a change
			optionsPreview.totimestamp=t;
			return TRUE;	//we need to redraw
		}
	}



	return FALSE;
}


int HandleEditDateControls(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	if (codeNotify == EN_KILLFOCUS)	{
		//Redraw appropriate areas
		if (SetDateFromControl(hwndCtl))	{
			InvalidateRect(hwndDateSlider, NULL, 0);
			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
			SendMessage(hwndOverview, WT_WM_QUEUERECALC, 0,0);
			UpdateDateControlsFromOptions(&optionsPreview);
		}
	}

	return	0;
}

int HandleEditControls(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	char szText[128];
	double value;	//to hold the textbox value
	int needsRedraw;

	//printf("\n%i", codeNotify);

	//If the control in question gains focus
	if (codeNotify == EN_SETFOCUS)	{
		char szBuffer[256];
		LoadString(hInst, id, szBuffer, sizeof(szBuffer));
	    UpdateStatusBar(szBuffer, 0, 0);

		if (id==ID_EDITPRESET)	{
			//SendMessage(hwndEditPreset, EM_SETSEL,0,-1);	//select the whole line when focus gained //I don't like how this acts
			//printf("select line");

			SendMessage(hwndDropdownPreset, WT_WM_PRESETRECALC, 0,0);
		}

		return 0;
	}

	else if (codeNotify==EN_CHANGE)	{
	//If not in focus we shouldn't do anything with the change
		if (hwndCtl!=GetFocus())
			return 0;

		needsRedraw=0;
		SendMessage(hwndCtl, WM_GETTEXT, 128,(long)&szText[0]);
		value=atof(szText);

		switch (id)	{
		case ID_EDITNORTH:
			if ((value >= -90) && (value<=90))
				if (optionsPreview.nswe.north!=value)	{
					optionsPreview.nswe.north=value;
					needsRedraw = 1;
					printf("N");
				}
			break;
		case ID_EDITSOUTH:
			if ((value >= -90) && (value<=90))
				if (optionsPreview.nswe.south!=value)	{
					optionsPreview.nswe.south=value;
					needsRedraw = 1;
					printf("S");
				}
			break;
		case ID_EDITWEST:
			if ((value >= -180) && (value<=180))	{
				if (optionsPreview.nswe.west!=value)	{
					optionsPreview.nswe.west=value;
					needsRedraw = 1;
					printf("W");
				}
			}
		break;
		case ID_EDITEAST:
			if ((value >= -180) && (value<=180))
				if (optionsPreview.nswe.east != value)	{
					optionsPreview.nswe.east=value;
					needsRedraw = 1;
					printf("E");
				}
			break;

/*		case ID_EDITEXPORTHEIGHT:
			UpdateExportAspectRatioFromOptions(&optionsPreview, 1);
			optionsPreview.forceheight=1;
			break;
		case ID_EDITEXPORTWIDTH:
			UpdateExportAspectRatioFromOptions(&optionsPreview, 0);
			optionsPreview.forceheight=0;
			break;
*/
		case ID_EDITTHICKNESS:
			optionsPreview.thickness=value;
			needsRedraw=1;
			break;

		case ID_EDITPRESET:
			SendMessage(hwndDropdownPreset, WT_WM_PRESETRECALC, 0,0);
			break;



		}

		if (needsRedraw)	{
			printf("redraw from edit\n");
			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
			UpdateBarsFromNSWE(&optionsPreview.nswe);
		}

	}
	else if (codeNotify == EN_KILLFOCUS)	{
		switch (id)	{
			case ID_EDITPRESET:
				//printf("\nhide dropdown");
				ShowWindow(hwndDropdownPreset, SW_HIDE);
				break;
		}

	}
	//else {
		//printf("\nNotification: %i", codeNotify);
	//}


	return 0;
}

LRESULT CALLBACK DropdownPresetWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	DROPDOWNINFO *DropDown;
	int x,y;
	int row;
	HDC hdc;
	RECT redrawRect;	//the part to redraw, so we don't flicker

	int keydirection;	//up =-1 or down = 1

	switch (msg) {
		case WM_CREATE:
			RECT editRect;
			SIZE textSize;
			DropDown = calloc(sizeof(DROPDOWNINFO),1);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)DropDown);
			DropDown->numberPresets = 6;	//we can only go up to eight
			hdc = GetDC(hwnd);
			SelectObject(hdc, hFontDialog);
			GetTextExtentPoint32(hdc, "AQZ2fgjhlM", 10 , &textSize);//we just use this to get the text height using these ten chars
			ReleaseDC(hwnd, hdc);
			DropDown->displayHeight = textSize.cy+2;
			GetWindowRect(hwndEditPreset, &editRect);
			DropDown->displayWidth = editRect.right-editRect.left;
			SetWindowPos(hwndDropdownPreset, HWND_BOTTOM,0,0,DropDown->displayWidth,DropDown->displayHeight*DropDown->displayedPresets,SWP_NOMOVE);

			break;

		case WT_WM_PRESETRECALC:
			char szEditBox[128];

			DropDown = (DROPDOWNINFO *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

			char *oldPresetText[DropDown->numberPresets];
			//memset(oldPresetText, 0, DropDown->numberPresets * sizeof(char *));

			for (int i=0; i<DropDown->displayedPresets; i++)	{
				oldPresetText[i] = DropDown->bestPresets[i].name;
			}

			//Get the messagebox text
			SendMessage(hwndEditPreset, WM_GETTEXT, 128,(long)&szEditBox[0]);
			int displaycount;

			if (!strcmp(szEditBox, "Type a place"))	{
				printf("No place");
				SendMessage(hwndEditPreset, EM_SETSEL,0,-1);	//select the whole line when focus gained
				szEditBox[0]=0;
			}

			if (strcmp(DropDown->previousEditBox, szEditBox))	{	//only bother updating if the text has changed.
				displaycount = GetBestPresets(szEditBox, presetArray, numberOfPresets, DropDown->bestPresets, DropDown->numberPresets);
				DropDown->displayedPresets = displaycount;
			}

			strcpy(DropDown->previousEditBox, szEditBox);

			if (DropDown->displayedPresets == 0)	{	//if w're not displaying anything, we show one line
				if (!IsWindowVisible(hwndDropdownPreset))	{
					SetWindowPos(hwndDropdownPreset, HWND_BOTTOM,0,0,DropDown->displayWidth,DropDown->displayHeight*1,SWP_NOMOVE|SWP_SHOWWINDOW);
				}
				InvalidateRect(hwnd, NULL, FALSE);
				return 0;
			}


			if (!IsWindowVisible(hwndDropdownPreset))	{
				SetWindowPos(hwndDropdownPreset, HWND_BOTTOM,0,0,DropDown->displayWidth,DropDown->displayHeight*DropDown->displayedPresets,SWP_NOMOVE|SWP_SHOWWINDOW);
			}
			else	{	//if the window is technically visible, but sized to nothing
				RECT isEmptyRect;
				GetWindowRect(hwndDropdownPreset, &isEmptyRect);
				if ((isEmptyRect.bottom-isEmptyRect.top)!=DropDown->displayHeight*DropDown->displayedPresets)	{
					SetWindowPos(hwndDropdownPreset, HWND_BOTTOM,0,0,DropDown->displayWidth,DropDown->displayHeight*DropDown->displayedPresets,SWP_NOMOVE|SWP_SHOWWINDOW);
				}
				printf("not drawn");
			}




			for (int i=0; i<DropDown->numberPresets; i++)	{
				if (oldPresetText[i] != DropDown->bestPresets[i].name)	{
					redrawRect.top=i*DropDown->displayHeight;
					redrawRect.bottom=(i+1)*DropDown->displayHeight;
					redrawRect.left=0;redrawRect.right=DropDown->displayWidth;
					InvalidateRect(hwnd, &redrawRect, FALSE);
				}
			}

			break;


		case WT_WM_PRESETCHOSEN:
			DropDown = (DROPDOWNINFO *)GetWindowLongPtr(hwnd, GWLP_USERDATA);



			//stolen from mouse routine - need to merge
			CopyNSWE(&optionsPreview.nswe, &DropDown->bestPresets[DropDown->highlightedPreset].nswe);

			UpdateEditNSWEControls(&optionsPreview.nswe);
			UpdateBarsFromNSWE(&optionsPreview.nswe);
			//UpdateExportAspectRatioFromOptions(&optionsPreview,0);
			InvalidateRect(hwndOverview, NULL, FALSE);
			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
			SetWindowText(hwndEditPreset, DropDown->bestPresets[DropDown->highlightedPreset].name);
			SendMessage(hwndEditPreset, EM_SETSEL, strlen(DropDown->bestPresets[DropDown->highlightedPreset].name), strlen(DropDown->bestPresets[DropDown->highlightedPreset].name));
//			SetFocus(NULL);	//by killing the focus, we can display the dropdown again later, but instead we'll bring up when typing


			ShowWindow(hwnd, SW_HIDE);
			break;

		case WM_LBUTTONDOWN:
			SetCapture(hwnd);
			break;
		case WM_LBUTTONUP:	//we've clicked on a place
			y=GET_Y_LPARAM(lParam);
			DropDown = (DROPDOWNINFO *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

			row = y/DropDown->displayHeight;
			if ((row>=0) && (row<DropDown->numberPresets))	{

				SendMessage(hwnd, WT_WM_PRESETCHOSEN,0,0);
/*
				CopyNSWE(&optionsPreview.nswe, &DropDown->bestPresets[row].nswe);

				UpdateEditNSWEControls(&optionsPreview.nswe);
				UpdateBarsFromNSWE(&optionsPreview.nswe);
				//UpdateExportAspectRatioFromOptions(&optionsPreview,0);
				InvalidateRect(hwndOverview, NULL, FALSE);
				SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
				SetWindowText(hwndEditPreset, DropDown->bestPresets[row].name);
				//SetFocus(NULL);	//by killing the focus, we can display the dropdown again later
*/
			}
			//ShowWindow(hwnd, SW_HIDE);
			ReleaseCapture();
			break;



		case WM_MOUSEMOVE:
			x=GET_X_LPARAM(lParam);
			y=GET_Y_LPARAM(lParam);
			DropDown = (DROPDOWNINFO *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

			if (DropDown->displayHeight)	{
				row=y/DropDown->displayHeight;
			}
			if (row != DropDown->highlightedPreset)	{
				//invalidate the prev and new highlighted lines
				redrawRect.top=DropDown->highlightedPreset*DropDown->displayHeight;
				redrawRect.bottom=(DropDown->highlightedPreset+1)*DropDown->displayHeight;
				redrawRect.left=0;redrawRect.right=DropDown->displayWidth;
				InvalidateRect(hwnd, &redrawRect, FALSE);

				redrawRect.top=row*DropDown->displayHeight;
				redrawRect.bottom=(row+1)*DropDown->displayHeight;
				redrawRect.left=0;redrawRect.right=DropDown->displayWidth;
				InvalidateRect(hwnd, &redrawRect, FALSE);

				DropDown->highlightedPreset = row;
			}
			break;

		case WT_WM_PRESETDOWN:	//the keydown is pressed
		case WT_WM_PRESETUP: //pressed up
			if (msg == WT_WM_PRESETDOWN) {
				keydirection=1;
				}
			else	{
				keydirection=-1;
			}
			printf("key %i", keydirection);

			DropDown = (DROPDOWNINFO *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

			//Redraw the old one
			redrawRect.top=DropDown->highlightedPreset*DropDown->displayHeight;
			redrawRect.bottom=(DropDown->highlightedPreset+1)*DropDown->displayHeight;
			redrawRect.left=0;redrawRect.right=DropDown->displayWidth;
			InvalidateRect(hwnd, &redrawRect, FALSE);



			DropDown->highlightedPreset+=keydirection;
			if (DropDown->highlightedPreset>=DropDown->numberPresets) {
				DropDown->highlightedPreset=0;
				}
			else if (DropDown->highlightedPreset<0)	{
				DropDown->highlightedPreset=DropDown->numberPresets-1;
			}

			//Then the new
			redrawRect.top=DropDown->highlightedPreset*DropDown->displayHeight;
			redrawRect.bottom=(DropDown->highlightedPreset+1)*DropDown->displayHeight;
			redrawRect.left=0;redrawRect.right=DropDown->displayWidth;
			InvalidateRect(hwnd, &redrawRect, FALSE);


		break;


		case WM_PAINT:
			PAINTSTRUCT ps;
			RECT rect;
			//int textHeight;
			//int heightBox;



			DropDown = (DROPDOWNINFO *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

			hdc = BeginPaint(hwnd, &ps);

			//SelectObject(hdc, (HFONT)SendMessage(GetParent(hwnd), WM_GETFONT, 0,0));
			SelectObject(hdc, hFontDialog);

			GetWindowRect(hwnd, &rect);
			//printf("\nPaint rect: %i, %i %i %i", ps.rcPaint.left,ps.rcPaint.top, ps.rcPaint.right,ps.rcPaint.bottom);
			rect.right-=rect.left;

			rect.top=0;
			rect.left= 0;

			if (DropDown->displayedPresets>0)	{
				SetWindowPos(hwnd, HWND_TOP, 0,0,rect.right,DropDown->displayHeight * DropDown->displayedPresets, SWP_NOMOVE);
			}
			else	{
				SetWindowPos(hwnd, HWND_TOP, 0,0,rect.right,DropDown->displayHeight, SWP_NOMOVE);	//display one line, telling them to type something
			}

			int firstrow=ps.rcPaint.top/DropDown->displayHeight;
			int lastrow=ps.rcPaint.bottom/DropDown->displayHeight;

			if (firstrow<0)	{
				firstrow=0;
			}
			if (lastrow>DropDown->displayedPresets)	{	//make sure funny resizing of the window doesn't crash anything.
				lastrow = DropDown->displayedPresets;
			}

			if (DropDown->displayedPresets==0)	{
				rect.top=0;
				rect.bottom=DropDown->displayHeight;

				SetBkColor(hdc, RGB(192,192,192));
				ExtTextOut(hdc, rect.left,rect.top,ETO_OPAQUE, &rect, "Start typing", 12, NULL);
				lastrow=-1;
			}

			for (int n=firstrow;(n<=lastrow);n++)	{
				rect.top=n*DropDown->displayHeight;
				rect.bottom=(n+1)*DropDown->displayHeight;

				if (n == DropDown->highlightedPreset)	{
					SetBkColor(hdc, RGB(160,255,n*32));
				}
				else 	{
					SetBkColor(hdc, RGB(255,160+n*8,n*32));
				}
				//printf("\nrect: %i %i %i, %i", rect.left, rect.right, rect.top, rect.bottom);
				ExtTextOut(hdc, rect.left,rect.top,ETO_OPAQUE, &rect, DropDown->bestPresets[n].name, strlen(DropDown->bestPresets[n].name), NULL);
				rect.top=rect.bottom;
			}
			EndPaint(hwnd, &ps);
			break;
/*
		case WM_SETFONT:
			printf("\nFont");
			hdc = GetDC(hwnd);
			SelectObject(hdc, (HFONT)wParam);
			//GetTextExtentPoint32(hdc, "AQZ2fgjhlM", 10 , &textSize);//we just use this to get the text height using these ten chars
			ReleaseDC(hwnd, hdc);
			break;
			*/

		default:
			return DefWindowProc(hwnd,msg,wParam,lParam);
		}

	return 0;
}


int ExportGPXDialogAndComplete(HWND hwnd, LOCATIONHISTORY *lh)
{
	char filename[MAX_PATH];
	OPENFILENAME ofn;

	memset(&ofn,0,sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = hInst;
	filename[0]=0;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = "Export GPX to...";
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = "gpx";
	ofn.Flags =OFN_OVERWRITEPROMPT;
	ofn.lpstrFilter = "GPX files (*.gpx)\0*.gpx\0\0";


	if (GetSaveFileName(&ofn)==0)
		return 0;

	ExportGPXFile(lh, filename, optionsPreview.fromtimestamp, optionsPreview.totimestamp);

	return 1;
}

int ExportKMLDialogAndComplete(HWND hwnd, OPTIONS * o)
{
	char filename[MAX_PATH];
	//char editboxtext[128];
	OPENFILENAME ofn;

	memset(&ofn,0,sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = hInst;
	filename[0]=0;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = "Export KML to...";
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = "kml";
	ofn.Flags =OFN_OVERWRITEPROMPT;
	ofn.lpstrFilter = "Kml files (*.kml)\0*.kml\0\0";


	if (GetSaveFileName(&ofn)==0)
		return 0;

//	printf("Filename:%s\n",filename);

	//Create export options
	memset(&optionsExport, 0, sizeof(optionsExport));

	optionsExport.kmlfilenameinput = filename;
	sprintf(optionsExport.kmlfilenamefinal, "%s", filename);
	sprintf(optionsExport.pngfilenamefinal, "%s.png", filename);

	optionsExport.pngfilenameinput = filename;

	//SendMessage(hwndEditExportWidth, WM_GETTEXT, 128,(long)&editboxtext[0]);
	//optionsExport.width=atol(&editboxtext[0]);
	//SendMessage(hwndEditExportHeight, WM_GETTEXT, 128,(long)&editboxtext[0]);
	//optionsExport.height=atol(&editboxtext[0]);
	optionsExport.width=SendMessage(hwndTabExport, WT_WM_TAB_GETEXPORTWIDTH, 0, 0);
	optionsExport.height=SendMessage(hwndTabExport, WT_WM_TAB_GETEXPORTHEIGHT, 0, 0);

	//get the NSWE, thickness, dates and colour info from the options
	optionsExport.nswe.north=o->nswe.north;
	optionsExport.nswe.south=o->nswe.south;
	optionsExport.nswe.west=o->nswe.west;
	optionsExport.nswe.east=o->nswe.east;
	optionsExport.thickness = o->thickness;

	optionsExport.colourextra = optionsPreview.colourextra;
	optionsExport.colourcycle = optionsPreview.colourcycle;
	optionsExport.totimestamp =o->totimestamp;
	optionsExport.fromtimestamp =o->fromtimestamp;
	optionsExport.zoom=optionsExport.width/(optionsExport.nswe.east-optionsExport.nswe.west);
	optionsExport.alpha=200;	//default
	optionsExport.colourby = optionsPreview.colourby;

	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadSaveKML, &optionsExport ,0,NULL);

	return 0;
}

DWORD WINAPI ThreadSaveKML(OPTIONS *options)
{
	BM exportBM;
	int error;

	UpdateStatusBar("Exporting...", 0, 0);

//	EnterCriticalSection(&critAccessLocations);

	printf("bitmap init %i", options->width);
	bitmapInit(&exportBM, options, &locationHistory);
	DrawGrid(&exportBM);
	PlotPaths(&exportBM, &locationHistory, options);

	printf("png:%s, w:%i, h:%i size:%i\n",exportBM.options->pngfilenamefinal, exportBM.width, exportBM.height, exportBM.sizebitmap);
	error = lodepng_encode32_file(exportBM.options->pngfilenamefinal, exportBM.bitmap, exportBM.width, exportBM.height);
	if(error) fprintf(stderr, "LodePNG error %u: %s\n", error, lodepng_error_text(error));

	//Write KML file
	fprintf(stdout, "KML to %s. PNG to %s\n", exportBM.options->kmlfilenamefinal, exportBM.options->pngfilenamefinal);
	WriteKMLFile(&exportBM);
	bitmapDestroy(&exportBM);

//	LeaveCriticalSection(&critAccessLocations);
    UpdateStatusBar("Ready", 0, 0);

	return 0;
}



HBITMAP MakeHBitmapOverview(HWND hwnd, HDC hdc, LOCATIONHISTORY * lh)
{
	HBITMAP bitmap;
	BITMAPINFO bi;
	BYTE * bits;
	int x;
	int y;
	COLOUR c;
	COLOUR d;	//window colour

	int result;

	if (hbmOverview!=NULL)	{
		DeleteObject(hbmOverview);
	}


	bitmap = LoadImage(hInst, MAKEINTRESOURCE(IDB_OVERVIEW), IMAGE_BITMAP, 0,0,LR_DEFAULTSIZE|LR_CREATEDIBSECTION);
	printf("\nBMO:%x", (unsigned long)bitmap);

	//If there's no locations loaded, don't bother with the rest
	if (lh->first==NULL)	return bitmap;



	memset(&bi, 0, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);


	GdiFlush();
	result = GetDIBits(hdc, bitmap, 0, 180, NULL, &bi,DIB_RGB_COLORS);	//get the info

	//printf("\nOverview\nbi.height: %i\t", bi.bmiHeader.biHeight);
	//printf("bi.bitcount: %i\t", bi.bmiHeader.biBitCount);
	//printf("bi.sizeimage: %i\t\n", bi.bmiHeader.biSizeImage);

	//bits=malloc(bi.bmiHeader.biSizeImage);
	bits = VirtualAlloc(NULL, bi.bmiHeader.biSizeImage, MEM_COMMIT, PAGE_READWRITE);	//malloc can cause crashes see http://pastebin.com/L8rrC4mQ


	result = GetDIBits(hdc, bitmap, 0, 180, bits, &bi, DIB_RGB_COLORS);	//get the bitmap location

	memset(&overviewBM, 0, sizeof(overviewBM));
	bitmapInit(&overviewBM, &optionsOverview, &locationHistory);
	//printf("\n%i", overviewBM.sizebitmap);
	PlotPaths(&overviewBM, &locationHistory, &optionsOverview);


	for (y=0;y<180;y++)	{
		for (x=0;x<360;x++)	{
			c= bitmapPixelGet(&overviewBM, x,y);
			if (c.A>0) {	//don't bother if there's not a visible pixel
				d.B=bits[x*3+(180-y-1)*360*3+0];
				d.G=bits[x*3+(180-y-1)*360*3+1];
				d.R=bits[x*3+(180-y-1)*360*3+2];

				mixColours(&d, &c);

				bits[x*3+(180-y-1)*360*3+0] = d.B;
				bits[x*3+(180-y-1)*360*3+1] = d.G;
				bits[x*3+(180-y-1)*360*3+2] = d.R;
			}
		}
	}


	result = SetDIBits(hdc,bitmap,0,180, bits,&bi,DIB_RGB_COLORS);
	GdiFlush();
	VirtualFree(bits, 0, MEM_RELEASE);

	bitmapDestroy(&overviewBM);	//importantly, I missed this ?was it causing crashes.

	return bitmap;
}

int PaintOverview(HWND hwnd)
{
	HDC hdc;
	HDC memDC;
	PAINTSTRUCT ps;
	HGDIOBJ oldBitmap;

	int r;

	if (!hbmOverview)
		printf("\nHBMOVERVIEWNOT HERE!!");

	hdc= BeginPaint(hwnd, &ps);
	if (!hdc)	printf("\nHDC NULL");

	memDC = CreateCompatibleDC(hdc);

	oldBitmap = SelectObject(memDC, hbmOverview);
	if (!oldBitmap)	printf("\noldbitmap NULL");		//this seems to be the problem, sometimes this is NULL
	r = BitBlt(hdc,0, 0, 360, 180, memDC, 0, 0, SRCCOPY);
	if (!r)	printf("\nbitblt NULL");
	SelectObject(memDC, oldBitmap);
	GdiFlush();

	DeleteDC(memDC);
	EndPaint(hwnd, &ps);

	return 0;
}


int UpdateBarsFromNSWE(NSWE * d)
{
	MoveWindow(hwndOverviewMovebarWest, 180 + d->west-OVB_RESTINGWIDTH/2 , 0, OVB_RESTINGWIDTH,180,TRUE);
	MoveWindow(hwndOverviewMovebarEast, 180 + d->east-OVB_RESTINGWIDTH/2, 0, OVB_RESTINGWIDTH,180,TRUE);
	MoveWindow(hwndOverviewMovebarNorth, 0, 90 - d->north-OVB_RESTINGWIDTH/2, 360,OVB_RESTINGWIDTH,TRUE);
	MoveWindow(hwndOverviewMovebarSouth, 0, 90 - d->south-OVB_RESTINGWIDTH/2, 360,OVB_RESTINGWIDTH,TRUE);

	InvalidateRect(hwndOverview,0,FALSE);	//this doesn't always work
	return 0;
}



LRESULT CALLBACK OverviewMovebarWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	MOVEBARINFO * pMbi;
	POINT mousePoint;

	switch (msg) {
		case WM_SETCURSOR:
			pMbi = (MOVEBARINFO *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			if ((pMbi->direction == D_NORTH)||(pMbi->direction == D_SOUTH))	{
				SetCursor(LoadCursor(NULL,IDC_SIZENS));
			}
			else	{
				SetCursor(LoadCursor(NULL,IDC_SIZEWE));
			}
			return 1;
			break;
		case WM_LBUTTONDOWN:
			SetFocus(hwnd);	//mainly to get the focus out of the edit boxes
			GetCursorPos(&mousePoint);
			mouseDragMovebar=TRUE;
			SetCapture(hwnd);
		break;
		case WM_MOUSEMOVE:
			if (mouseDragMovebar == FALSE)
				return 1;
			GetCursorPos(&mousePoint);
			ScreenToClient(GetParent(hwnd), &mousePoint);
			pMbi = (MOVEBARINFO *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

			if ((pMbi->direction == D_NORTH)||(pMbi->direction == D_SOUTH))	{
				if (mousePoint.y<0)	mousePoint.y=0;
				if (mousePoint.y>180)	mousePoint.y=180;
				MoveWindow(hwnd, 0, mousePoint.y-OVB_MOVINGWIDTH/2, 360, OVB_MOVINGWIDTH, TRUE);
			}


			if ((pMbi->direction == D_WEST)||(pMbi->direction == D_EAST))	{
				if (mousePoint.x<-1)	mousePoint.x=-1;
				if (mousePoint.x>359)	mousePoint.x=359;
				MoveWindow(hwnd, mousePoint.x-OVB_MOVINGWIDTH/2, 0, OVB_MOVINGWIDTH, 180, TRUE);
			}

		break;
		case WM_LBUTTONUP:
			if (mouseDragMovebar == FALSE)
				return 1;

			pMbi = (MOVEBARINFO *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			mouseDragMovebar=FALSE;
			ReleaseCapture();
			GetCursorPos(&mousePoint);
			ScreenToClient(GetParent(hwnd), &mousePoint);

			if ((pMbi->direction == D_NORTH)||(pMbi->direction == D_SOUTH))	{
				if (mousePoint.y<0)	mousePoint.y=0;
				if (mousePoint.y>180)	mousePoint.y=180;
				MoveWindow(hwnd, 0, mousePoint.y-OVB_RESTINGWIDTH/2, 360,OVB_RESTINGWIDTH,TRUE);
			}


			if ((pMbi->direction == D_WEST)||(pMbi->direction == D_EAST))	{
				if (mousePoint.x<0)	mousePoint.x=0;
				if (mousePoint.x>359)	mousePoint.x=359;
				MoveWindow(hwnd, mousePoint.x-1, 0, 2,180,TRUE);
			}

			if (pMbi->direction == D_WEST)	{
				optionsPreview.nswe.west = mousePoint.x-180;
			}
			if (pMbi->direction == D_EAST)	{
				optionsPreview.nswe.east = mousePoint.x-180;
			}
			if (pMbi->direction == D_NORTH)	{
				optionsPreview.nswe.north =90-mousePoint.y;
			}
			if (pMbi->direction == D_SOUTH)	{
				optionsPreview.nswe.south =90-mousePoint.y;
			}
			UpdateEditNSWEControls(&optionsPreview.nswe);
			//UpdateExportAspectRatioFromOptions(&optionsPreview,0);

			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);

		break;
		default:
			return DefWindowProc(hwnd,msg,wParam,lParam);
	}


	return 0;
}

int CreateOverviewMovebarWindows(HWND hwnd)
{

	hwndOverviewMovebarWest = CreateWindow("OverviewMovebarClass", "West", WS_CHILD|WS_VISIBLE, 10,0,OVB_RESTINGWIDTH,180,hwnd, NULL, hInst, NULL);
	mbiWest.direction = D_WEST;
	SetWindowLongPtr(hwndOverviewMovebarWest, GWL_USERDATA, (LONG)&mbiWest);

	hwndOverviewMovebarEast = CreateWindow("OverviewMovebarClass", "East", WS_CHILD|WS_VISIBLE, 350,0,OVB_RESTINGWIDTH,180,hwnd, NULL, hInst, NULL);
	mbiEast.direction = D_EAST;
	SetWindowLongPtr(hwndOverviewMovebarEast, GWL_USERDATA, (LONG)&mbiEast);

	hwndOverviewMovebarNorth = CreateWindow("OverviewMovebarClass", "North", WS_CHILD|WS_VISIBLE, 0,10,360,OVB_RESTINGWIDTH,hwnd, NULL, hInst, NULL);
	mbiNorth.direction = D_NORTH;
	SetWindowLongPtr(hwndOverviewMovebarNorth, GWL_USERDATA, (LONG)&mbiNorth);

	hwndOverviewMovebarSouth = CreateWindow("OverviewMovebarClass", "South", WS_CHILD|WS_VISIBLE, 0,20,360,OVB_RESTINGWIDTH,hwnd, NULL, hInst, NULL);
	mbiSouth.direction = D_SOUTH;
	SetWindowLongPtr(hwndOverviewMovebarSouth, GWL_USERDATA, (LONG)&mbiSouth);

	UpdateBarsFromNSWE(&optionsPreview.nswe);

	return 1;
}

LRESULT CALLBACK OverviewWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	POINT mousePoint;
	HDC hdc;

	switch (msg) {
		case WM_CREATE:
			hbmOverview = NULL;		//set to null, as it deletes the object if not
			SendMessage(hwnd, WT_WM_RECALCBITMAP, 0,0);
			CreateOverviewMovebarWindows(hwnd);
			break;


		case WT_WM_QUEUERECALC:		//start a timer, and send the recalc bitmap when appropriate
			int timerlength;
			timerlength= 20;	//default timer length of 20 ms (shorter than the preview window, as I want it more responsive and it's less intensive)
			if ((wParam>0) && (wParam<10000))	{	//if it's zero, or too high, then use default
				timerlength = wParam;
			}
			KillTimer(hwnd, IDT_OVERVIEWTIMER);
			SetTimer(hwnd, IDT_OVERVIEWTIMER, timerlength, NULL);
			return 1;
			break;
		case WM_TIMER:
			KillTimer(hwnd, IDT_OVERVIEWTIMER);
		case WT_WM_RECALCBITMAP:
			memset(&optionsOverview, 0, sizeof(optionsOverview));	//set all the option results to 0

			optionsOverview.height=180;optionsOverview.width=360;
			optionsOverview.thickness = 1;

			optionsOverview.colourcycle = optionsPreview.colourcycle;
			optionsOverview.totimestamp = optionsPreview.totimestamp;
			optionsOverview.fromtimestamp = optionsPreview.fromtimestamp;
			optionsOverview.zoom=optionsOverview.width/(optionsOverview.nswe.east-optionsOverview.nswe.west);
			optionsOverview.alpha=200;	//default
			optionsOverview.colourby = optionsPreview.colourby;
			optionsOverview.colourextra = optionsPreview.colourextra;

			optionsOverview.nswe.north=90;
			optionsOverview.nswe.south=-90;
			optionsOverview.nswe.west=-180;
			optionsOverview.nswe.east=180;


			hdc = GetDC(hwnd);
			hbmOverview = MakeHBitmapOverview(hwnd, hdc, &locationHistory);
			ReleaseDC(hwnd, hdc);

			printf("\nrecalc'd overview BM");

			InvalidateRect(hwnd, 0, 0);

			break;
		case WM_ERASEBKGND:	//to avoid flicker:(although shouldn't need if hbrush is null)
    		return 1;
		case WM_PAINT:
			PaintOverview(hwnd);
			break;
		case WT_WM_SIGNALMOUSEWHEEL:	//The is the Overview window proc
			//Translate to corresponding position, then treat as if did it on preview
			//At the moment I'm not doing this, so
			POINT mousePoint;
			mousePoint.x=GET_X_LPARAM(lParam);
			mousePoint.y=GET_Y_LPARAM(lParam);


			HandlePreviewMousewheel(hwnd, wParam, lParam);
			break;
		case WM_LBUTTONDOWN:
			SetFocus(hwnd);	//mainly to get the focus out of the edit boxes
			GetCursorPos(&overviewOriginalPoint);
			CopyNSWE(&overviewOriginalNSWE, &optionsPreview.nswe);

			mouseDragOverview=TRUE;
			SetCapture(hwnd);
			break;
		case WM_MOUSEMOVE:
			if (mouseDragOverview == FALSE)
				return 1;
			GetCursorPos(&mousePoint);
			optionsPreview.nswe.north=overviewOriginalNSWE.north-mousePoint.y+overviewOriginalPoint.y;
			optionsPreview.nswe.south=overviewOriginalNSWE.south-mousePoint.y+overviewOriginalPoint.y;
			optionsPreview.nswe.west=overviewOriginalNSWE.west+mousePoint.x-overviewOriginalPoint.x;
			optionsPreview.nswe.east=overviewOriginalNSWE.east+mousePoint.x-overviewOriginalPoint.x;

			ConstrainNSWE(&optionsPreview.nswe);
			UpdateBarsFromNSWE(&optionsPreview.nswe);
			UpdateEditNSWEControls(&optionsPreview.nswe);
			//UpdateExportAspectRatioFromOptions(&optionsPreview,0);
			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);

			break;

		case WM_LBUTTONUP:
			if (mouseDragOverview == FALSE)
				return 1;
			mouseDragOverview=FALSE;
			ReleaseCapture();
			break;



		default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
}

int HandlePreviewLbutton(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	double dpp;	//degrees per pixel
	POINT mousePoint;
	int mouseDeltaX, mouseDeltaY;

	mousePoint.x = GET_X_LPARAM(lParam);
	mousePoint.y = GET_Y_LPARAM(lParam);

	switch (msg)	{
		case WM_LBUTTONDOWN:
			previewOriginalPoint.x=mousePoint.x;
			previewOriginalPoint.y=mousePoint.y;
			CopyNSWE(&previewOriginalNSWE, &optionsPreview.nswe);
			mouseDragPreview=TRUE;
			SetFocus(hwnd);
			SetCapture(hwnd);
		break;
		case WM_MOUSEMOVE:
			if (mouseDragPreview == FALSE)
				return 1;
			dpp = (optionsPreview.nswe.east - optionsPreview.nswe.west)/(optionsPreview.width);	//dpp aspect ratio should be 1, so only need to read x
			mouseDeltaX = mousePoint.x-previewOriginalPoint.x;
			mouseDeltaY = mousePoint.y-previewOriginalPoint.y;

			//Confine the coords of the box
			if (previewOriginalNSWE.east - dpp*(mouseDeltaX)>180)
				mouseDeltaX=(previewOriginalNSWE.east - 180)/dpp;
			if (previewOriginalNSWE.west - dpp*(mouseDeltaX)<-180)
				mouseDeltaX=(previewOriginalNSWE.west + 180)/dpp;
			if (previewOriginalNSWE.north + dpp*(mouseDeltaY)>90)
				mouseDeltaY = (90 - previewOriginalNSWE.north)/dpp;
			if (previewOriginalNSWE.south + dpp*(mouseDeltaY)<-90)
				mouseDeltaY = (-90 - previewOriginalNSWE.south)/dpp;


			optionsPreview.nswe.east= previewOriginalNSWE.east - dpp*(mouseDeltaX);
			optionsPreview.nswe.west= previewOriginalNSWE.west - dpp*(mouseDeltaX);
			optionsPreview.nswe.north= previewOriginalNSWE.north + dpp*(mouseDeltaY);
			optionsPreview.nswe.south= previewOriginalNSWE.south + dpp*(mouseDeltaY);

			//Round to appropriate amount
			optionsPreview.nswe.east = TruncateByDegreesPerPixel(optionsPreview.nswe.east, dpp);
			optionsPreview.nswe.west = TruncateByDegreesPerPixel(optionsPreview.nswe.west, dpp);
			optionsPreview.nswe.south = TruncateByDegreesPerPixel(optionsPreview.nswe.south, dpp);
			optionsPreview.nswe.north = TruncateByDegreesPerPixel(optionsPreview.nswe.north, dpp);



			stretchPreview.nXOriginSrc=0;
			stretchPreview.nYOriginSrc=0;
			stretchPreview.nWidthSrc=optionsPreview.width;
			stretchPreview.nHeightSrc=optionsPreview.height;


			stretchPreview.nXOriginDest=0;
			stretchPreview.nYOriginDest=0;
			stretchPreview.nWidthDest=optionsPreview.width;
			stretchPreview.nHeightDest=optionsPreview.height;

			//now set the stretchblt info
			if (mouseDeltaX<0)	{	//moving left
				stretchPreview.nXOriginSrc=mouseDeltaX*-1;
				stretchPreview.nXOriginDest=0;
				stretchPreview.nWidthSrc = optionsPreview.width + mouseDeltaX;
				stretchPreview.nWidthDest = optionsPreview.width + mouseDeltaX;
				stretchPreview.useStretch = TRUE;

				InvalidateRect(hwndPreview, NULL, 0);

			}
			else if (mouseDeltaX>0)	{	//moving right
				stretchPreview.nXOriginSrc=0;
				stretchPreview.nXOriginDest=mouseDeltaX;
				stretchPreview.nWidthSrc = optionsPreview.width - mouseDeltaX;
				stretchPreview.nWidthDest = optionsPreview.width - mouseDeltaX;
				stretchPreview.useStretch = TRUE;

				InvalidateRect(hwndPreview, NULL, 0);
			}


			if (mouseDeltaY>0)	{	//moving down
				stretchPreview.nYOriginSrc=mouseDeltaY*-1;
				stretchPreview.nYOriginDest=0;
				stretchPreview.nHeightSrc = optionsPreview.height + mouseDeltaY;
				stretchPreview.nHeightDest = optionsPreview.height + mouseDeltaY;
				stretchPreview.useStretch = TRUE;

				InvalidateRect(hwndPreview, NULL, 0);

			}
			else if (mouseDeltaY<0)	{	//moving up
				stretchPreview.nYOriginSrc=0;
				stretchPreview.nYOriginDest=mouseDeltaY;
				stretchPreview.nHeightSrc = optionsPreview.height - mouseDeltaY;
				stretchPreview.nHeightDest = optionsPreview.height - mouseDeltaY;
				stretchPreview.useStretch = TRUE;

				InvalidateRect(hwndPreview, NULL, 0);
			}



			ConstrainNSWE(&optionsPreview.nswe);		//rather than constrain after the move, I need to stop while moving
			UpdateBarsFromNSWE(&optionsPreview.nswe);
			UpdateEditNSWEControls(&optionsPreview.nswe);

		break;
		case WM_LBUTTONUP:
			if (mouseDragPreview == FALSE)
				return 1;
			mouseDragPreview=FALSE;
			ReleaseCapture();
			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
		break;


	}

	return 0;
}

int HandlePreviewMousewheel(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	signed int zDelta;
	double longitude;
	double latitude;

	double longspan;
	double latspan;

	double zoomfactor;
	double dpp;

	POINT mousePoint;

	SetFocus(hwnd);
	zDelta = HIWORD(wParam);

	if (zDelta>32768)	{//convert it to a signed int
		zDelta -=65536;
	}

	zoomfactor = pow(0.9, zDelta/120);	//0.9 ^ zDelta/120, zDelta is usually 120 on a standard mouse wheel

	longspan = optionsPreview.nswe.east - optionsPreview.nswe.west;
	latspan = optionsPreview.nswe.north - optionsPreview.nswe.south;

	if ((zDelta>0)&&((longspan<0.01) || (latspan<0.01)))	{	//if we're too zoomed in already (i.e. there's less than 0.01 degrees between sides)
		return 0;
	}


	mousePoint.x = GET_X_LPARAM(lParam);
	mousePoint.y = GET_Y_LPARAM(lParam);
	ScreenToClient(hwnd, &mousePoint);

	dpp = longspan/optionsPreview.width;

	longitude = (double)mousePoint.x*longspan/optionsPreview.width + optionsPreview.nswe.west;
	latitude = optionsPreview.nswe.north - latspan* (double)mousePoint.y/(double)optionsPreview.height;

	//printf("\nInitial longspan: %f, latspan %f, aspect ratio: %f\n", longspan, latspan, longspan/latspan);
//	printf("\nmw x%i, y%i ht:%i: wt:%i long:%f,lat:%f, zoom:%f\n",mousePoint.x, mousePoint.y,  optionsPreview.height, optionsPreview.width, longitude, latitude, zoomfactor);

	//This shifts the origin to where the mouse was
	optionsPreview.nswe.west-=longitude;
	optionsPreview.nswe.north-=latitude;
	optionsPreview.nswe.east-=longitude;
	optionsPreview.nswe.south-=latitude;
	//resizes
	optionsPreview.nswe.west*=(zoomfactor);
	optionsPreview.nswe.north*=(zoomfactor);
	optionsPreview.nswe.east*=(zoomfactor);
	optionsPreview.nswe.south*=(zoomfactor);
	//then moves the origin back
	optionsPreview.nswe.west+=longitude;
	optionsPreview.nswe.north+=latitude;
	optionsPreview.nswe.east+=longitude;
	optionsPreview.nswe.south+=latitude;

	//Now we set the coords for the StretchBlt if required
	if (zoomfactor<1)	{	//if we're zooming in
		//we'll stretch the centre to the whole window
		stretchPreview.nXOriginDest=0;
		stretchPreview.nYOriginDest=0;
		stretchPreview.nWidthDest=optionsPreview.width;
		stretchPreview.nHeightDest=optionsPreview.height;

		//if this isn't already set, it'll be the same as the destination
		if (!stretchPreview.useStretch)	{
			stretchPreview.nXOriginSrc=0;
			stretchPreview.nYOriginSrc=0;
			stretchPreview.nWidthSrc=optionsPreview.width;
			stretchPreview.nHeightSrc=optionsPreview.height;
		}

		stretchPreview.nWidthSrc*=zoomfactor;
		stretchPreview.nHeightSrc*=zoomfactor;

		stretchPreview.nXOriginSrc-=mousePoint.x;
		stretchPreview.nYOriginSrc-=mousePoint.y;

		stretchPreview.nXOriginSrc*=zoomfactor;
		stretchPreview.nYOriginSrc*=zoomfactor;

		stretchPreview.nXOriginSrc+=mousePoint.x;
		stretchPreview.nYOriginSrc+=mousePoint.y;

		stretchPreview.useStretch = TRUE;

		//printf("Stretch dest:(%i,%i,%i,%i)", stretchPreview.nXOriginDest, stretchPreview.nYOriginDest, stretchPreview.nWidthDest, stretchPreview.nHeightDest);
		//printf(" from src (%i,%i,%i,%i)\n", stretchPreview.nXOriginSrc, stretchPreview.nYOriginSrc, stretchPreview.nWidthSrc, stretchPreview.nHeightSrc);
	}
	else	{	//zoom out
		//when zooming out the src wile be the whole window, and we'll compress it to a smaller area
		stretchPreview.nXOriginSrc=0;
		stretchPreview.nYOriginSrc=0;
		stretchPreview.nWidthSrc=optionsPreview.width;
		stretchPreview.nHeightSrc=optionsPreview.height;

		//initially set it up to just the same
		if (!stretchPreview.useStretch)	{
			stretchPreview.nXOriginDest=0;
			stretchPreview.nYOriginDest=0;
			stretchPreview.nWidthDest=optionsPreview.width;
			stretchPreview.nHeightDest=optionsPreview.height;
		}

		//then scale size
		stretchPreview.nWidthDest/=zoomfactor;
		stretchPreview.nHeightDest/=zoomfactor;

		//translating so the origin is at the mouse point
		stretchPreview.nXOriginDest-=mousePoint.x;
		stretchPreview.nYOriginDest-=mousePoint.y;

		stretchPreview.nXOriginDest/=zoomfactor;
		stretchPreview.nYOriginDest/=zoomfactor;

		stretchPreview.nXOriginDest+=mousePoint.x;
		stretchPreview.nYOriginDest+=mousePoint.y;

		stretchPreview.useStretch = TRUE;


	}


	optionsPreview.nswe.north = TruncateByDegreesPerPixel(optionsPreview.nswe.north, dpp);
	optionsPreview.nswe.south = TruncateByDegreesPerPixel(optionsPreview.nswe.south, dpp);
	optionsPreview.nswe.west = TruncateByDegreesPerPixel(optionsPreview.nswe.west, dpp);
	optionsPreview.nswe.east = TruncateByDegreesPerPixel(optionsPreview.nswe.east, dpp);


	ConstrainNSWE(&optionsPreview.nswe);
	UpdateEditNSWEControls(&optionsPreview.nswe);
	UpdateBarsFromNSWE(&optionsPreview.nswe);

	SendMessage(hwndPreview, WT_WM_QUEUERECALC , 0,0);

	return 1;
}

LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT msg, WPARAM wParam,LPARAM lParam)
{
	RECT clientRect;

	switch (msg) {
		case WM_KEYDOWN:
			HandlePreviewKeydown(hwnd, wParam, lParam);
			break;
		case WM_CREATE:
			hbmPreview = NULL;
			CreatePreviewCropbarWindows(hwnd);	//creates the child windows we'll use to help crop the frame
			break;
		case WT_WM_QUEUERECALC:		//start a timer, and send the recalc bitmap when appropriate

    		UpdateStatusBar(SuggestAreaFromNSWE(&optionsPreview.nswe, presetArray, numberOfPresets), 1, SBT_POPOUT);


			int timerlength;
			timerlength= 100;	//default timer length of 100 ms (longer in the preview window)
			if ((wParam>0) && (wParam<10000))	{	//if it's zero, or too high, then use default
				timerlength = wParam;
			}
			KillTimer(hwnd, IDT_PREVIEWTIMER);
			SetTimer(hwnd, IDT_PREVIEWTIMER, timerlength, NULL);


			//int w,h;
			GetClientRect(hWndMain, &clientRect);
			PreviewWindowFitToAspectRatio(hWndMain, clientRect.bottom, clientRect.right, (optionsPreview.nswe.east-optionsPreview.nswe.west)/(optionsPreview.nswe.north-optionsPreview.nswe.south));
			InvalidateRect(hwnd, NULL, 0);	//trigger a WM_PAINT, as we may be able to stretch or translate
			hbmQueueSize++;
			return 1;
			break;
		case WM_TIMER:
			KillTimer(hwnd, IDT_PREVIEWTIMER);
		case WT_WM_RECALCBITMAP:
			//printf("\n*Recalc Preview*");
			GetClientRect(hwnd, &clientRect);
//			PreviewWindowFitToAspectRatio(hWndMain, clientRect.bottom, clientRect.right, (optionsPreview.nswe.east-optionsPreview.nswe.west)/(optionsPreview.nswe.north-optionsPreview.nswe.south));
//			optionsPreview.width = clientRect.right;
//			optionsPreview.height = clientRect.bottom;
			CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadSetHBitmap, (LPVOID)hbmQueueSize ,0,NULL));
    		return 1;
			break;
		case WM_SIZE:	//might have to ensure it doesn't waste time if size doesn't change.
			//printf("***size***\n");
			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
			ResetCropbarWindowPos(hwndPreviewCropbarWest);
			ResetCropbarWindowPos(hwndPreviewCropbarEast);
			ResetCropbarWindowPos(hwndPreviewCropbarNorth);
			ResetCropbarWindowPos(hwndPreviewCropbarSouth);
			UpdateExportAspectRatioFromOptions(&optionsPreview, optionsPreview.forceheight);

			InvalidateRect(hwnd, NULL, 0);
			break;
		case WM_PAINT:
			PaintPreview(hwnd, &locationHistory);
			break;
		case WM_LBUTTONDOWN:
		case WM_MOUSEMOVE:
		case WM_LBUTTONUP:
			HandlePreviewLbutton(hwnd, msg, wParam, lParam);
			break;
		case WT_WM_SIGNALMOUSEWHEEL:
			HandlePreviewMousewheel(hwnd, wParam, lParam);
			break;
		case WM_CONTEXTMENU:
			HMENU hMenu;
			hMenu = CreatePopupMenu();
			InsertMenu(hMenu, 0, MF_BYPOSITION|MF_STRING, IDM_ZOOMALL, "Zoom to whole world");
			InsertMenu(hMenu, 1, MF_BYPOSITION|MF_STRING, IDM_ZOOMFIT, "Zoom to fit all points");
			InsertMenu(hMenu, 2, MF_BYPOSITION|MF_STRING, IDM_CREATEREGIONFROMVIEW, "Create region from view");
			InsertMenu(hMenu, 3, MF_BYPOSITION|MF_STRING, IDM_EXPORTPNG, "Export image to PNG file");
			InsertMenu(hMenu, 4, MF_BYPOSITION|MF_STRING, IDM_CONSTRAINTIMETOVIEW, "Constrain time to this view");
			TrackPopupMenu(hMenu, TPM_LEFTALIGN|TPM_TOPALIGN|TPM_RIGHTBUTTON, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0, hwnd, NULL);
			break;
		case WM_COMMAND:
			HANDLE_WM_COMMAND(hwnd,wParam,lParam,MainWndProc_OnCommand);	//note we use the same as the main window, to make things central
			break;
		case WM_SETFOCUS:
	    	UpdateStatusBar("Click and drag to move viewpoint. Use the mouse scrollwheel to zoom in and out.", 0, 0);
			break;
		default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
}


int PaintPreview(HWND hwnd, LOCATIONHISTORY * lh)
{
	HDC hdc;
	HDC memDC;
	PAINTSTRUCT ps;
	HGDIOBJ oldBitmap;

	RECT clientRect;
	int width, height;


	GetClientRect(hwnd, &clientRect);
	width=clientRect.right+2;
	height=clientRect.bottom+2;

//	optionsPreview.width = clientRect.right;
//	optionsPreview.height = clientRect.bottom;

	if (!previewBM.bitmap)	{
		hdc= BeginPaint(hwnd, &ps);

		EndPaint(hwnd, &ps);
		return 0;
	}

	EnterCriticalSection(&critAccessPreviewHBitmap);
	hdc= BeginPaint(hwnd, &ps);
	memDC = CreateCompatibleDC(hdc);



	oldBitmap = SelectObject(memDC, hbmPreview);

	if (stretchPreview.useStretch)	{
		SetStretchBltMode(hdc, HALFTONE);
		StretchBlt(hdc, stretchPreview.nXOriginDest, stretchPreview.nYOriginDest, stretchPreview.nWidthDest, stretchPreview.nHeightDest,
			memDC, stretchPreview.nXOriginSrc,stretchPreview.nYOriginSrc,stretchPreview.nWidthSrc, stretchPreview.nHeightSrc, SRCCOPY);

		//fill in the remaining areas
		int top,bottom,left,right;
		top = stretchPreview.nYOriginDest;
		left= stretchPreview.nXOriginDest;
		bottom = stretchPreview.nYOriginDest+stretchPreview.nHeightDest;
		right = stretchPreview.nXOriginDest+stretchPreview.nWidthDest;

		//top panels
		if (top>0)	{
			Rectangle(hdc,0,0, previewBM.width,top);
		}
		if (bottom<previewBM.height)	{
			Rectangle(hdc,0,bottom, previewBM.width, previewBM.height);
		}

		//side panels
		if (left>0)	{
			Rectangle(hdc,0,0, left, previewBM.height);
		}
		if (right<previewBM.width)	{
			Rectangle(hdc,right,0, previewBM.width, previewBM.height);
		}



	}
	else if ((width!=previewBM.width) || (height!=previewBM.height))	{
		printf("\nwidth not the same %i vs %i, ht: %i vs %i",width, previewBM.width, height, previewBM.height);
		SetStretchBltMode(hdc, HALFTONE);
		StretchBlt(hdc,0,0,width, height, memDC, 0,0,previewBM.width, previewBM.height, SRCCOPY);
	}
	else	{
		BitBlt(hdc,0, 0, width, height, memDC, 0, 0, SRCCOPY);
	}




	SelectObject(memDC, oldBitmap);
	DeleteDC(memDC);

	EndPaint(hwnd, &ps);

	LeaveCriticalSection(&critAccessPreviewHBitmap);


	return 0;
}

DWORD WINAPI ThreadSetHBitmap(long queuechit)
{
	HDC hdc;


	if (queuechit<hbmQueueSize)	{
		printf("\nQUEUE SKIPPED start thread!!");
		return 0;
	}

	hdc = GetDC(hwndPreview);
	hbmPreview = MakeHBitmapPreview(hdc, &locationHistory, queuechit);
	ReleaseDC(hwndPreview, hdc);
	stretchPreview.useStretch = FALSE;
	InvalidateRect(hwndPreview, NULL, 0);

	return 1;
}


HBITMAP MakeHBitmapPreview(HDC hdc, LOCATIONHISTORY * lh, long queuechit)
{
	HBITMAP bitmap;
	BITMAPINFO bmi;
	BYTE * bits;
	int width, height;
	int x;
	int y;
	COLOUR c;
	COLOUR d;	//window colour

	height = optionsPreview.height;	//just in case these are changed halfway through the processing
	width = optionsPreview.width;


	if (queuechit<hbmQueueSize)	{
		printf("\nQUEUE SKIPPED in thread!!");
		return hbmPreview;
	}

	optionsPreview.zoom=optionsPreview.width/(optionsPreview.nswe.east-optionsPreview.nswe.west);
	optionsPreview.alpha=200;	//default


	EnterCriticalSection(&critAccessLocations);
	//printf("bi");

	if (previewBM.bitmap)	{
		bitmapDestroy(&previewBM);
	}
	bitmapInit(&previewBM, &optionsPreview, &locationHistory);

	PlotPaths(&previewBM, &locationHistory, &optionsPreview);


	DrawListOfRegions(&previewBM, regionFirst);

	HeatMap(&previewBM, &locationHistory, &optionsPreview.nswe, 0, 0);

//Display the presets

/*
	WORLDREGION displayRegion;
	for (int i=0;i<numberOfPresets;i++	)	{
		CopyNSWE(&displayRegion.nswe, &presetArray[i].nswe);
		displayRegion.baseColour.R=255;
		displayRegion.baseColour.G=15;
		displayRegion.baseColour.B=115;
		displayRegion.baseColour.A=30;
		DrawRegion(&previewBM, &displayRegion);
	}
*/

	//printf("\nCreating Preview bitmap %i %i %i %i",width,height, previewBM.width, previewBM.height);
	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height; // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;

	EnterCriticalSection(&critAccessPreviewHBitmap);
	GdiFlush();
	bitmap = CreateDIBSection(hdc, &bmi,DIB_RGB_COLORS, &bits, NULL, 0);
	//printf("create dib: %x", (unsigned long)bitmap);

	int b;	//the bit we're up to in the DIB
	b=0;
	for (y=0;y<height;y++)	{
		for (x=0;x<width;x++)	{
			memcpy(&c, previewBM.bitmap+(x+y* width) *4, sizeof(COLOUR));	//set c to the colour in the previewBM.bitmap

			d.R = d.G =d.B =0;	//d, for destination. This is if i need to merge with something else, for now it's just black

			mixColours(&d, &c);	//mix the colour on the bitmap to the destination
			bits[b] =d.B;	b++;
			bits[b] =d.G;	b++;
			bits[b] =d.R;	b++;

//			bits[b] =c.B;	b++;
//			bits[b] =c.G;	b++;
//			bits[b] =c.R;	b++;


		}
		b=(b+3) & ~3;	//round to next WORD alignment at end of line
	}

	if (hbmPreview!=NULL)	{
		DeleteObject(hbmPreview);
	}
	hbmPreview=bitmap;	//this probably needs to happen in the critical section, making the return value useless
	GdiFlush();
	LeaveCriticalSection(&critAccessPreviewHBitmap);
	LeaveCriticalSection(&critAccessLocations);

	return bitmap;
}




BOOL CALLBACK UpdateFont(HWND hwnd, LPARAM hFont)
{
	//printf("\nUpdate Font%x", hwnd);
	SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
	return TRUE;
}


int UpdateFontOfChildren(HWND hwnd, HFONT hFont)
{
//	UpdateFont(hwnd, (LPARAM)hFont);
	EnumChildWindows(hwnd, UpdateFont, (LPARAM)hFont);
	return 0;
}



LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	RECT rect;	//temporary rect, currently used for checkingposition of preview window

	switch (msg) {
	case WM_CREATE:
		int y=MARGIN;
		int x=MARGIN;
		//now create child windows
//		hwndStaticFilename =CreateWindow("Static","Filename:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, OVERVIEW_WIDTH, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
//		y+=MARGIN+TEXT_HEIGHT;
		//overview Window
		hwndOverview = CreateWindow("OverviewClass", NULL, WS_CHILD|WS_VISIBLE|WS_BORDER|WS_CLIPCHILDREN, x, y ,OVERVIEW_WIDTH, OVERVIEW_HEIGHT, hwnd,NULL,hInst,NULL);
		y+=MARGIN+OVERVIEW_HEIGHT;
		//dialog
		hwndStaticNorth = CreateWindow("Static","North:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditNorth = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP, x, y, TEXT_WIDTH_QLONG, 20, hwnd, (HMENU)ID_EDITNORTH, hInst, NULL);
//		y+=MARGIN+TEXT_HEIGHT;
		x+=MARGIN+TEXT_WIDTH_QLONG;
		hwndStaticSouth = CreateWindow("Static","South:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditSouth = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP, x, y, TEXT_WIDTH_QLONG, 20, hwnd, (HMENU)ID_EDITSOUTH, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;
		x=MARGIN;

		hwndStaticWest = CreateWindow("Static","West:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditWest = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QLONG, 20, hwnd, (HMENU)ID_EDITWEST, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QLONG;
		//y+=MARGIN+TEXT_HEIGHT;

		hwndStaticEast = CreateWindow("Static","East:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditEast = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QLONG, TEXT_HEIGHT, hwnd, (HMENU)ID_EDITEAST, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;

		//now change the proc for NSWE
		SetWindowLongPtr(hwndEditNorth, GWLP_WNDPROC, (long)&EditDirectionProc);
		SetWindowLongPtr(hwndEditSouth, GWLP_WNDPROC, (long)&EditDirectionProc);
		SetWindowLongPtr(hwndEditEast, GWLP_WNDPROC, (long)&EditDirectionProc);
		SetWindowLongPtr(hwndEditWest, GWLP_WNDPROC, (long)&EditDirectionProc);


		x=MARGIN;
		hwndStaticPreset = CreateWindow("Static", "Preset:",  WS_CHILD | WS_VISIBLE| WS_BORDER, x,y,TEXT_WIDTH_QUARTER, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=TEXT_WIDTH_QUARTER+ MARGIN;
		hwndEditPreset = CreateWindow("Edit", "Type a place",  WS_CHILD | WS_VISIBLE|WS_BORDER, x,y,TEXT_WIDTH_THREEQUARTERS, TEXT_HEIGHT, hwnd, (HMENU)ID_EDITPRESET, hInst, NULL);
		//Change the WNDPROC of the edit window, so we can take control
		DefEditProc = (WNDPROC)SetWindowLongPtr(hwndEditPreset, GWLP_WNDPROC, (long)&EditPresetProc); //We need to reverse this when the windows closes.

		hwndDropdownPreset = CreateWindow("DropdownPreset", NULL,  WS_CHILD |WS_CLIPSIBLINGS| WS_BORDER, x,y+TEXT_HEIGHT,TEXT_WIDTH_THREEQUARTERS, TEXT_HEIGHT*5, hwnd, (HMENU)ID_DROPDOWNPRESET, hInst, NULL);

		y+=MARGIN+TEXT_HEIGHT;
		x=MARGIN;


		CreateWindow("Static","From:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditDateFrom = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP, x, y, TEXT_WIDTH_QLONG, 20, hwnd, (HMENU)ID_EDITDATEFROM, hInst, NULL);
//		y+=MARGIN+TEXT_HEIGHT;
		x+=MARGIN+TEXT_WIDTH_QLONG;
		CreateWindow("Static","To:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditDateTo = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP, x, y, TEXT_WIDTH_QLONG, 20, hwnd, (HMENU)ID_EDITDATETO, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;
		x=MARGIN;

		SetWindowLongPtr(hwndEditDateFrom, GWLP_WNDPROC, (long)&EditDateProc);
		SetWindowLongPtr(hwndEditDateTo, GWLP_WNDPROC, (long)&EditDateProc);



		hwndDateSlider = CreateWindow("DateSlider",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP, x, y, OVERVIEW_WIDTH, TEXT_HEIGHT, hwnd, NULL, hInst, NULL);


		y+=MARGIN+TEXT_HEIGHT;
		x=MARGIN;
		CreateWindow("Static","Thickness:",	  WS_CHILD | WS_VISIBLE| WS_BORDER, x,y,TEXT_WIDTH_QUARTER, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QUARTER;
		hwndEditThickness = CreateWindow("Edit","1", WS_CHILD | WS_VISIBLE |ES_NUMBER| WS_BORDER|WS_TABSTOP, x, y, TEXT_WIDTH_QUARTER, 20, hwnd, (HMENU)ID_EDITTHICKNESS, hInst, NULL);
		SetWindowLongPtr(hwndEditThickness, GWLP_WNDPROC, (long)&EditIntegerProc);


		x+=MARGIN+TEXT_WIDTH_QUARTER;

		CreateWindow("Static","Colour by:",	  WS_CHILD | WS_VISIBLE| WS_BORDER, x,y,TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndComboboxColourBy = CreateWindow("ComboBox","Date",	  CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE, x,y,TEXT_WIDTH_QLONG, TEXT_HEIGHT*10, hwnd, (HMENU)ID_COMBOCOLOURBY, hInst, NULL);

		for (int i=0; i<=MAX_COLOURBY_OPTION; i++)	{
			SendMessage(hwndComboboxColourBy,(UINT) CB_ADDSTRING,(WPARAM) 0,(LPARAM)szColourByOption[i]);
		}
		SendMessage(hwndComboboxColourBy, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
		DefComboboxProc = (WNDPROC)SetWindowLongPtr(hwndComboboxColourBy, GWLP_WNDPROC, (long)&ComboboxColourByProc);

		y+=MARGIN+TEXT_HEIGHT;
		x=MARGIN;

		hwndColourByDate = CreateWindow("ColourByDate", NULL, WS_CHILD|WS_VISIBLE, x, y, OVERVIEW_WIDTH, TEXT_HEIGHT*2+MARGIN, hwnd, NULL, hInst, NULL);
		hwndColourByWeekday = CreateWindow("ColourByWeekday", NULL, WS_CHILD, x, y, OVERVIEW_WIDTH, TEXT_HEIGHT*2+MARGIN, hwnd, NULL, hInst, NULL);
		hwndColourByMonth = CreateWindow("ColourByMonth", NULL, WS_CHILD, x, y, OVERVIEW_WIDTH, TEXT_HEIGHT*2+MARGIN, hwnd, NULL, hInst, NULL);

		y+=40;

		//Now the right hand side
		x=MARGIN+OVERVIEW_WIDTH+MARGIN+MARGIN;
		y=MARGIN;

		//Create the Graph Window
		//hwndMainGraph = CreateWindow("MainGraph", NULL, WS_CHILD|WS_VISIBLE|WS_BORDER, x, y ,640, 200, hwnd,NULL,hInst,NULL);
		//y+=200+MARGIN;


		//tabs
		hwndTab = CreateWindow(WC_TABCONTROL, NULL, WS_CHILD|WS_VISIBLE, x,y,640,200,hwnd,NULL, hInst, NULL);
		SendMessage(hwndTab, WM_SETFONT, (WPARAM)hFontDialog, TRUE);
		DefTabcontrolProc = (WNDPROC)SetWindowLongPtr(hwndTab, GWLP_WNDPROC, (long)&TabsProc);
		y+=200+MARGIN;
		CreateTabsAndTabWindows(hwndTab);

		//This is the big window down the bottom right
		hwndPreview = CreateWindow("PreviewClass", NULL, WS_CHILD|WS_VISIBLE|WS_BORDER, x, y ,OVERVIEW_WIDTH, OVERVIEW_WIDTH, hwnd,NULL,hInst,NULL);


		SendMessage(hwnd, WM_SETFONT, (WPARAM)hFontDialog, 1);

		UpdateEditNSWEControls(&optionsPreview.nswe);
		break;

	case WM_SETFONT:
		UpdateFontOfChildren(hwnd, hFontDialog);
		break;
	case WM_SIZE:
		SendMessage(hWndStatusbar,msg,wParam,lParam);
		InitializeStatusBar(hWndStatusbar,3);
		PreviewWindowFitToAspectRatio(hwnd, HIWORD(lParam), LOWORD(lParam), (optionsPreview.nswe.east-optionsPreview.nswe.west)/(optionsPreview.nswe.north-optionsPreview.nswe.south));
		break;
	case WM_MENUSELECT:
		return MsgMenuSelect(hwnd,msg,wParam,lParam);
		break;
	case WM_LBUTTONDOWN:
		SetFocus(hwnd);
    	UpdateStatusBar("Ready", 0, 0);
		break;
	case WM_MOUSEWHEEL:
		//if it's within the Preview bounding box, send it there
		GetWindowRect(hwndPreview, &rect);
		if ((GET_X_LPARAM(lParam)>=rect.left) && (GET_X_LPARAM(lParam)<=rect.right) && (GET_Y_LPARAM(lParam)>=rect.top) && (GET_Y_LPARAM(lParam)<=rect.bottom))	{
			SendMessage(hwndPreview, WT_WM_SIGNALMOUSEWHEEL, wParam, lParam);
		}

		//Do the same for the Overview
		GetWindowRect(hwndOverview, &rect);
		if ((GET_X_LPARAM(lParam)>=rect.left) && (GET_X_LPARAM(lParam)<=rect.right) && (GET_Y_LPARAM(lParam)>=rect.top) && (GET_Y_LPARAM(lParam)<=rect.bottom))	{
			SendMessage(hwndOverview, WT_WM_SIGNALMOUSEWHEEL, wParam, lParam);
		}
		return 0;
		break;

	case WT_WM_RECALCBITMAP:
		//if parent gets this, send it to appropriate children
		printf("\nRECALC BOTH");
		SendMessage(hwndOverview, WT_WM_RECALCBITMAP, 0,0);
		SendMessage(hwndPreview, WT_WM_RECALCBITMAP, 0,0);
		break;

	case WM_COMMAND:
		HANDLE_WM_COMMAND(hwnd,wParam,lParam,MainWndProc_OnCommand);
		break;
	case WM_NOTIFY:
		return HANDLE_WM_NOTIFY(hwnd,wParam,lParam,MainWndProc_OnTabNotify);
		break;
	case WM_DESTROY:
		SetWindowLongPtr(hwndEditPreset, GWLP_WNDPROC, (long)&DefEditProc);	//TODO Need to fix the others too!
		SetWindowLongPtr(hwndEditNorth, GWLP_WNDPROC, (long)&DefEditProc);
		SetWindowLongPtr(hwndEditSouth, GWLP_WNDPROC, (long)&DefEditProc);
		SetWindowLongPtr(hwndEditWest, GWLP_WNDPROC, (long)&DefEditProc);
		SetWindowLongPtr(hwndEditEast, GWLP_WNDPROC, (long)&DefEditProc);
		SetWindowLongPtr(hwndEditDateFrom, GWLP_WNDPROC, (long)&DefEditProc);
		SetWindowLongPtr(hwndEditDateTo, GWLP_WNDPROC, (long)&DefEditProc);
		SetWindowLongPtr(hwndEditColourCycle, GWLP_WNDPROC, (long)&DefEditProc);

		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
}

int HandlePreviewKeydown(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	double longspan;
	double latspan;
	double step;


	switch (wParam)	{

		case VK_RETURN:
			//HeatMap(&previewBM, &locationHistory, &optionsPreview.nswe, 5, 5);
			//InvalidateRect(hwndPreview,NULL,0);
			break;

		case VK_TAB:
			if (GetKeyState(VK_SHIFT) & SHIFTED)	{SetFocus(hwndTab);}	//back to the tabs above it
			else {SetFocus(hwndEditNorth);}		//roll on back to the north edit box
			break;

		case VK_LEFT:
		case VK_RIGHT:
		case VK_DOWN:
		case VK_UP:
		case VK_ADD:
		case VK_SUBTRACT:
		case VK_PRIOR: //page up
		case VK_NEXT:

		longspan = optionsPreview.nswe.east - optionsPreview.nswe.west;
		latspan = optionsPreview.nswe.north - optionsPreview.nswe.south;

		step = 0.02;
		if (GetKeyState(VK_SHIFT) & SHIFTED) {step*=5;}
		if (GetKeyState(VK_CONTROL) & SHIFTED) {step*=0.1;}

			switch (wParam)	{	//nested switch! Be careful!
				case VK_LEFT:
					optionsPreview.nswe.east-=longspan*step;
					optionsPreview.nswe.west-=longspan*step;
					break;
				case VK_RIGHT:
					optionsPreview.nswe.east+=longspan*step;
					optionsPreview.nswe.west+=longspan*step;
					break;
				case VK_DOWN:
					optionsPreview.nswe.north-=latspan*step;
					optionsPreview.nswe.south-=latspan*step;
					break;
				case VK_UP:
					optionsPreview.nswe.north+=latspan*step;
					optionsPreview.nswe.south+=latspan*step;
					break;
				case VK_PRIOR:
					optionsPreview.nswe.north+=latspan; //need to constrain these inside as well (to stop south moving up without north etc)
					optionsPreview.nswe.south+=latspan;
					break;
				case VK_NEXT:
					optionsPreview.nswe.north-=latspan;
					optionsPreview.nswe.south-=latspan;
					break;
				case VK_ADD:
					step*=-1;	//invert (so zooms in)
				case VK_SUBTRACT:
					optionsPreview.nswe.north+=latspan*step;
					optionsPreview.nswe.south-=latspan*step;
					optionsPreview.nswe.west-=longspan*step;
					optionsPreview.nswe.east+=longspan*step;
					break;
			}
		ConstrainNSWE(&optionsPreview.nswe);
		UpdateEditNSWEControls(&optionsPreview.nswe);
		UpdateBarsFromNSWE(&optionsPreview.nswe);
		SendMessage(hwndPreview, WT_WM_QUEUERECALC , 0,0);

		break;
	}

	//need to only do this if there's a change


	return 0;
}


int PreviewWindowFitToAspectRatio(HWND hwnd, int mainheight, int mainwidth, double aspectratio)
{
	//int mainheight, mainwidth;
	int availableheight, availablewidth;
	int endheight, endwidth;

	RECT statusbarRect;
	RECT previewRect;
	POINT previewPoint;


	GetWindowRect(hwndPreview, &previewRect);
	previewPoint.x=previewRect.left;
	previewPoint.y=previewRect.top;
	ScreenToClient(hwnd, &previewPoint);

	//Get status bar height
	GetClientRect(hWndStatusbar, &statusbarRect);

	availableheight = mainheight - previewPoint.y - MARGIN-statusbarRect.bottom;
	availablewidth  = mainwidth - previewPoint.x - MARGIN;



	//printf("ht:%i avail:%i top:%i %i\n", mainheight, availableheight,previewRect.top, previewPoint.y);

	if (availableheight*aspectratio > availablewidth)	{	//width limited
		endheight = availablewidth/aspectratio;
		endwidth =  availablewidth;
	} else	{
		endwidth =  availableheight*aspectratio;
		endheight = availableheight;
	}

	if (endwidth<20)	{
		endwidth=20;
		endheight = availablewidth/aspectratio;
	}

	//EnterCriticalSection(&critAccessLocations);
	optionsPreview.width=endwidth;
	optionsPreview.height=endheight;
	optionsPreview.aspectratio=aspectratio;
	//LeaveCriticalSection(&critAccessLocations);

	SetWindowPos(hwndPreview,0,0,0,endwidth,endheight,SWP_NOMOVE|SWP_NOOWNERZORDER);


	//printf("endwt: %i, endht: %i\n", endwidth, endheight);

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
	MSG msg;
	HANDLE hAccelTable;

	//This brings up a command console window


	AllocConsole();
    //freopen("conout$","w",stdout);
    //freopen("conout$","w",stderr);
	freopen("stdout.txt","w",stdout);
	freopen("stderr.txt","w",stderr);
//*/


	hInst = hInstance;
	if (!InitApplication())
		return 0;
	InitializeCriticalSection(&critAccessLocations);
	InitializeCriticalSection(&critAccessPreviewHBitmap);
	hAccelTable = LoadAccelerators(hInst,MAKEINTRESOURCE(IDACCEL));
	hWndMain = CreateWindow("worldtrackerWndClass","WorldTracker", WS_MINIMIZEBOX|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_MAXIMIZEBOX|WS_CAPTION|WS_BORDER|WS_SYSMENU|WS_THICKFRAME,		CW_USEDEFAULT,0,CW_USEDEFAULT,0,		NULL,		NULL,		hInst,		NULL);
	CreateSBar(hWndMain,"Ready",3);
	ShowWindow(hWndMain,SW_SHOW);
	while (GetMessage(&msg,NULL,0,0)) {
		if (!TranslateAccelerator(msg.hwnd,hAccelTable,&msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return msg.wParam;
}



LRESULT CALLBACK DateSliderWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{

	switch (msg) {
		case WM_PAINT:
			return PaintDateSlider(hwnd, &locationHistory, &optionsPreview);
		break;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_MOUSEMOVE:
			HandleSliderMouse(hwnd, msg, wParam, lParam);
		break;
		default:
			return DefWindowProc(hwnd,msg,wParam,lParam);
	}

	return 0;

}


time_t RoundTimeUp(time_t s)
{
	struct tm time;

	gmtime_s(&s, &time);

	time.tm_hour = 23;
	time.tm_min = 59;
	time.tm_sec=59;

	return mkgmtime(&time);
}

time_t RoundTimeDown(time_t s)
{
	struct tm time;

	gmtime_s(&s, &time);

	time.tm_hour =0;
	time.tm_min = 0;
	time.tm_sec=0;


	return mkgmtime(&time);
}


int PaintDateSlider(HWND hwnd, LOCATIONHISTORY * lh, OPTIONS *o)
{
    PAINTSTRUCT ps;
    HDC hdc;
	RECT clientRect;

	HPEN hPenBlack;
	HPEN hPenGrey;
	HPEN hPenWhite;

	int x,y;
	unsigned long secondsspan;
	double	spp;	//seconds per pixel
	time_t	s;
	struct tm time;	//this is stored statically, so not freed
	int	oldyear,oldmonth;

	GetClientRect(hwnd, &clientRect);
	hdc = BeginPaint(hwnd, &ps);


	secondsspan = lh->latesttimestamp - lh->earliesttimestamp;
	spp=secondsspan/clientRect.right;


	s=lh->earliesttimestamp;
	gmtime_s(&s, &time);
	oldyear=time.tm_year;
	oldmonth=time.tm_mon;

	hPenBlack = CreatePen(PS_SOLID, 1, RGB(0,0,0));
	hPenGrey = CreatePen(PS_SOLID, 1, RGB(192,192,192));
	hPenWhite = CreatePen(PS_SOLID, 1, RGB(255,255,255));

	for (x=0;x<clientRect.right;x++)	{
		s=lh->earliesttimestamp+x*spp;

		gmtime_s(&s, &time);
		y=0;
		MoveToEx(hdc, x, y, (LPPOINT) NULL);
		if (time.tm_year!=oldyear)	{
			SelectObject(hdc, hPenBlack);
			y=clientRect.bottom;
        	LineTo(hdc, x, y);
			oldyear=time.tm_year;
			oldmonth=time.tm_mon;
		}
		else if	(time.tm_mon!=oldmonth)	{
			SelectObject(hdc, hPenBlack);
        	y=clientRect.bottom/2;
			LineTo(hdc, x, y);
			oldmonth=time.tm_mon;
		}
		if (y<clientRect.bottom)	{
			if ((s < o->fromtimestamp)	|| (s > o->totimestamp))
				SelectObject(hdc, hPenGrey);
			else
				SelectObject(hdc, hPenWhite);
			y=clientRect.bottom;
			LineTo(hdc, x,y);
		}

	}
	DeleteObject(hPenBlack);
	DeleteObject(hPenGrey);
	DeleteObject(hPenWhite);

    EndPaint(hwnd, &ps);

	return 0;
}

int HandleSliderMouse(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	POINT mousePoint;
	RECT clientRect;

	unsigned long secondsspan;
	double	spp;	//seconds per pixel

	unsigned long timeclicked;

	int redraw;

	int xfrom,xto;

	mousePoint.x = GET_X_LPARAM(lParam);
	mousePoint.y = GET_Y_LPARAM(lParam);

	GetClientRect(hwnd, &clientRect);
	secondsspan = locationHistory.latesttimestamp - locationHistory.earliesttimestamp;
	if (secondsspan<24*60*60)	return 0;
	spp=secondsspan/clientRect.right;
	timeclicked = locationHistory.earliesttimestamp + mousePoint.x*spp;

	//contraints
	if (timeclicked<locationHistory.earliesttimestamp)	//constrain min
		timeclicked = locationHistory.earliesttimestamp;

	if (timeclicked>locationHistory.latesttimestamp)	//constrain max
		timeclicked = locationHistory.latesttimestamp;

	if (optionsPreview.fromtimestamp<locationHistory.earliesttimestamp)	{	//contrain the options timestamp too, as things are calculated from this
		optionsPreview.fromtimestamp=locationHistory.earliesttimestamp;
	}
	if (optionsPreview.totimestamp>locationHistory.latesttimestamp)	{	//contrain the options timestamp too, as things are calculated from this
		optionsPreview.totimestamp=locationHistory.latesttimestamp;
	}

	//don't let the from be greater than the to, or vice versa
	if (mouseDragDateSlider == MS_DRAGFROM)	{
		if (timeclicked > optionsPreview.totimestamp)	{
			timeclicked = RoundTimeDown(optionsPreview.totimestamp);
		}
	}
	if (mouseDragDateSlider == MS_DRAGTO)	{
		if (timeclicked < optionsPreview.fromtimestamp)	{
			timeclicked = RoundTimeUp(optionsPreview.fromtimestamp);
		}
	}



	xfrom = (optionsPreview.fromtimestamp-locationHistory.earliesttimestamp)/spp;
	xto = (optionsPreview.totimestamp-locationHistory.earliesttimestamp)/spp;

	switch (msg) {
		case WM_LBUTTONDOWN:
			SetFocus(hwnd);
			if ((locationHistory.latesttimestamp - locationHistory.earliesttimestamp)<24*60*60)	{	//if there's only one day, don't bother
				return 0;
			}
			if ((mousePoint.x+10 > xfrom) &&(mousePoint.x-10 < xfrom) && (mousePoint.x+10 > xto) &&(mousePoint.x-10 < xto))	{	//if it's in the grabbing ranges of both
				if (abs(mousePoint.x-xfrom) < abs(xto-mousePoint.x))
					mouseDragDateSlider=MS_DRAGFROM;
				else
					mouseDragDateSlider=MS_DRAGTO;
			}
			else if ((mousePoint.x+10 > xfrom) &&(mousePoint.x-10 < xfrom))	{
				mouseDragDateSlider=MS_DRAGFROM;
				SetCapture(hwnd);
			}
			else if ((mousePoint.x+10 > xto) &&(mousePoint.x-10 < xto))	{
				mouseDragDateSlider=MS_DRAGTO;
				SetCapture(hwnd);
			}

		case WM_MOUSEMOVE:
			if (mouseDragDateSlider)	{
				SetCursor(LoadCursor(NULL,IDC_SIZEWE));
				redraw=0;
				if (mouseDragDateSlider == MS_DRAGFROM)	{
					if (optionsPreview.fromtimestamp != timeclicked)	{ //I presumed this is so we don't redraw if no change
						optionsPreview.fromtimestamp = RoundTimeDown(timeclicked);	//the start of a day
						redraw=1;
					}
				}

				if (mouseDragDateSlider == MS_DRAGTO)	{
					if (optionsPreview.totimestamp != timeclicked)	{	//if there's a change
						optionsPreview.totimestamp = RoundTimeUp(timeclicked);		//the end of a day
						redraw=1;
					}
				}

				//Redraw if set above
				if (redraw)	{
					InvalidateRect(hwnd, NULL, 0);	//invalidate itself
					SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
					SendMessage(hwndOverview, WT_WM_QUEUERECALC, 0,0);
					UpdateDateControlsFromOptions(&optionsPreview);
				}
			}
			else	{	//if we're not dragging, then change cursor if we're on a drag
				if ((mousePoint.x+10 > xfrom) &&(mousePoint.x-10 < xfrom))	{
					SetCursor(LoadCursor(NULL,IDC_SIZEWE));
				}
				else if ((mousePoint.x+10 > xto) &&(mousePoint.x-10 < xto))	{
					SetCursor(LoadCursor(NULL,IDC_SIZEWE));
				}

			}
			break;

		case WM_LBUTTONUP:
			mouseDragDateSlider=0;
			ReleaseCapture();
			break;
	}
	return 0;
}


int UpdateDateControlsFromOptions(OPTIONS * o)
{
	char buffer[256];
	struct tm time;

	gmtime_s(&o->fromtimestamp, &time);
	strftime (buffer, 256, "%Y-%m-%d", &time);	//ISO8601YYYY-MM-DD
	SetWindowText(hwndEditDateFrom, buffer);

	gmtime_s(&o->totimestamp, &time);
	strftime (buffer, 256, "%Y-%m-%d", &time);	//ISO8601YYYY-MM-DD
	SetWindowText(hwndEditDateTo, buffer);
	return 0;
}

int CreatePreviewCropbarWindows(HWND hwnd)
{
	RECT rect;
	GetWindowRect(hwnd, &rect);	//get the parent rect

	hwndPreviewCropbarWest = CreateWindow("PreviewCropbarClass", "West", WS_CHILD|WS_VISIBLE, 0,0,PCB_GRABWIDTH,600,hwnd, NULL, hInst, NULL);
	hwndPreviewCropbarEast = CreateWindow("PreviewCropbarClass", "East", WS_CHILD|WS_VISIBLE, 600,0,PCB_GRABWIDTH,600,hwnd, NULL, hInst, NULL);

	hwndPreviewCropbarNorth = CreateWindow("PreviewCropbarClass", "North", WS_CHILD|WS_VISIBLE, 0,0,600,PCB_GRABWIDTH,hwnd, NULL, hInst, NULL);
	hwndPreviewCropbarSouth = CreateWindow("PreviewCropbarClass", "South", WS_CHILD|WS_VISIBLE, 0,600,600,610,hwnd, NULL, hInst, NULL);

	return 1;
}

int ResetCropbarWindowPos(HWND hwnd)
{
	RECT rect;
	int w, h;

	GetWindowRect(GetParent(hwnd), &rect);
	w=rect.right-rect.left;
	h=rect.bottom-rect.top;


	//if (hwnd==NULL)	{
//		SetWindowPos(hwndPreviewCropbarWest, NULL, 0,0, 10, optionsPreview.height, SWP_NOMOVE|SWP_NOOWNERZORDER);
//		SetWindowPos(hwndPreviewCropbarEast, NULL, optionsPreview.width-10,0, 10, optionsPreview.height, SWP_NOOWNERZORDER);
//		SetWindowPos(hwndPreviewCropbarNorth, NULL, 0,0,optionsPreview.width,10, SWP_NOOWNERZORDER);
//		SetWindowPos(hwndPreviewCropbarSouth, NULL, 0, optionsPreview.height-10,optionsPreview.width,10, SWP_NOOWNERZORDER);
//		printf("NSWE");
//	}

	if (hwnd==hwndPreviewCropbarWest)	{
		SetWindowPos(hwnd, NULL, 0,0, 10, h, SWP_NOMOVE|SWP_NOOWNERZORDER);
	}
	else if (hwnd==hwndPreviewCropbarEast)	{
		SetWindowPos(hwnd, NULL, w - PCB_GRABWIDTH,0, PCB_GRABWIDTH, optionsPreview.height, SWP_NOOWNERZORDER);
	}
	else if (hwnd==hwndPreviewCropbarNorth)	{
		SetWindowPos(hwnd, NULL, 0,0,w,PCB_GRABWIDTH, SWP_NOOWNERZORDER);
	}
	else if (hwnd==hwndPreviewCropbarSouth)	{
		SetWindowPos(hwnd, NULL, 0, h-PCB_GRABWIDTH, optionsPreview.width,PCB_GRABWIDTH, SWP_NOOWNERZORDER);
	}

	return 1;
}

int HandleCropbarMouse(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

	POINT mousePoint;
	NSWE EditNSWE;

	double east, north, south;
	double	dpp;	//degrees per pixel

	mousePoint.x = GET_X_LPARAM(lParam);
	mousePoint.y = GET_Y_LPARAM(lParam);


	switch (msg)	{
		case WM_SETCURSOR:
			if ((hwnd==hwndPreviewCropbarNorth) || (hwnd==hwndPreviewCropbarSouth))
				SetCursor(LoadCursor(NULL,IDC_SIZENS));
			else
				SetCursor(LoadCursor(NULL,IDC_SIZEWE));
			break;
		case WM_LBUTTONDOWN:
			SetCapture(hwnd);
			SetFocus(hwnd);
			mouseDragCropbar=1;
			InvalidateRect(hwnd, NULL, 0);
			break;
		case WM_LBUTTONUP:
			ReleaseCapture();
			if (mouseDragCropbar)	{
				if ((hwnd==hwndPreviewCropbarWest)&&(mousePoint.x>0))	{
					if (mousePoint.x > optionsPreview.width - optionsPreview.height/MAX_ASPECT_RATIO)		//prevent too thin an aspect ratio
						mousePoint.x = optionsPreview.width - optionsPreview.height/MAX_ASPECT_RATIO;
					if (mousePoint.x > optionsPreview.width - PCB_GRABWIDTH*2)		//then also constrain to a usable size
						mousePoint.x = optionsPreview.width - PCB_GRABWIDTH*2;

					dpp = (optionsPreview.nswe.east-optionsPreview.nswe.west)/optionsPreview.width;
					optionsPreview.nswe.west=TruncateByDegreesPerPixel(optionsPreview.nswe.west + dpp* mousePoint.x, dpp);
				}
				else if (hwnd==hwndPreviewCropbarEast)	{
					//HWND parentHwnd;
					ClientToScreen(hwnd, &mousePoint);	//converts from the position here
					//parentHwnd = GetParent(hwnd);

					ScreenToClient(GetParent(hwnd), &mousePoint);	//to the one on the preview
					//constrain to within the window if we're widening
					if (mousePoint.x>optionsPreview.width)
						mousePoint.x = optionsPreview.width;
					if (mousePoint.x < optionsPreview.height/MAX_ASPECT_RATIO)		//prevent too thin an aspect ratio
						mousePoint.x = optionsPreview.height/MAX_ASPECT_RATIO;
					if (mousePoint.x < PCB_GRABWIDTH*2)		//then also constrain to a usable size
						mousePoint.x = PCB_GRABWIDTH*2;

					optionsPreview.nswe.east=optionsPreview.nswe.west + (optionsPreview.nswe.east-optionsPreview.nswe.west)* mousePoint.x/optionsPreview.width;
				}
				else if	((hwnd==hwndPreviewCropbarNorth)&&(mousePoint.y>0))	{
					if (mousePoint.y > optionsPreview.height - optionsPreview.width/MAX_ASPECT_RATIO)		//prevent too thin an aspect ratio
						mousePoint.y = optionsPreview.height - optionsPreview.width/MAX_ASPECT_RATIO;
					if (mousePoint.y > optionsPreview.height - PCB_GRABWIDTH*2)		//then also constrain to a usable size
						mousePoint.y = optionsPreview.height - PCB_GRABWIDTH*2;
					dpp = (optionsPreview.nswe.south-optionsPreview.nswe.north)/optionsPreview.height;
					north=optionsPreview.nswe.north + dpp* mousePoint.y;
					optionsPreview.nswe.north=TruncateByDegreesPerPixel(north, dpp);

//					printf("x: %i, y: %i w:%f d:%f\n",mousePoint.x, mousePoint.y,north,dpp);
					//optionsPreview.nswe.north = optionsPreview.nswe.north + (optionsPreview.nswe.south-optionsPreview.nswe.north)* mousePoint.y/optionsPreview.height;
				}
				else if (hwnd==hwndPreviewCropbarSouth)	{
					ClientToScreen(hwnd, &mousePoint);	//converts from the position here
					ScreenToClient(GetParent(hwnd), &mousePoint);	//to the one on the preview
					if (mousePoint.y>optionsPreview.height)
						mousePoint.y = optionsPreview.height;
					if (mousePoint.y < optionsPreview.width/MAX_ASPECT_RATIO)		//prevent too thin an aspect ratio
						mousePoint.y = optionsPreview.width/MAX_ASPECT_RATIO;
					if (mousePoint.y < PCB_GRABWIDTH*2)		//then also constrain to a usable size
						mousePoint.y = PCB_GRABWIDTH*2;


					dpp = (optionsPreview.nswe.south-optionsPreview.nswe.north)/optionsPreview.height;
					south=optionsPreview.nswe.north + dpp* mousePoint.y;
					optionsPreview.nswe.south=TruncateByDegreesPerPixel(south, dpp);
				}


				//ResetCropbarWindowPos(hwnd);
				SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
				UpdateEditNSWEControls(&optionsPreview.nswe);
				UpdateBarsFromNSWE(&optionsPreview.nswe);
				mouseDragCropbar=0;
				InvalidateRect(hwnd, NULL, 0);
			}
			break;
		case WM_MOUSEMOVE:
			if (mouseDragCropbar)	{
				CopyNSWE(&EditNSWE, &optionsPreview.nswe);
				if	(hwnd==hwndPreviewCropbarWest)	{
					if (mousePoint.x<0)
						mousePoint.x=0;
					if (mousePoint.x > optionsPreview.width - optionsPreview.height/MAX_ASPECT_RATIO)		//prevent too thin an aspect ratio
						mousePoint.x = optionsPreview.width - optionsPreview.height/MAX_ASPECT_RATIO;
					if (mousePoint.x > optionsPreview.width - PCB_GRABWIDTH*2)		//then also constrain to a usable size
						mousePoint.x = optionsPreview.width - PCB_GRABWIDTH*2;


					SetWindowPos(hwnd, NULL, 0,0, mousePoint.x, optionsPreview.height, SWP_NOMOVE|SWP_NOOWNERZORDER);
					dpp = (optionsPreview.nswe.east-optionsPreview.nswe.west)/optionsPreview.width;


					EditNSWE.west=optionsPreview.nswe.west + dpp* mousePoint.x;
					EditNSWE.west=TruncateByDegreesPerPixel(EditNSWE.west, dpp);

					//printf("x: %i, y: %i w:%f d:%f\n",mousePoint.x, mousePoint.y,EditNSWE.west,dpp);
				}
				else if (hwnd==hwndPreviewCropbarEast)	{
					ClientToScreen(hwnd, &mousePoint);	//converts from the position here
					ScreenToClient(GetParent(hwnd), &mousePoint);	//to the one on the preview

					if (mousePoint.x>optionsPreview.width)
						mousePoint.x = optionsPreview.width;
					if (mousePoint.x < optionsPreview.height/MAX_ASPECT_RATIO)		//prevent too thin an aspect ratio
						mousePoint.x = optionsPreview.height/MAX_ASPECT_RATIO;
					if (mousePoint.x < PCB_GRABWIDTH*2)		//then also constrain to a usable size
						mousePoint.x = PCB_GRABWIDTH*2;



					SetWindowPos(hwnd, NULL, mousePoint.x,0, optionsPreview.width, optionsPreview.height, SWP_NOOWNERZORDER);
					dpp = (optionsPreview.nswe.east-optionsPreview.nswe.west)/optionsPreview.width;
					EditNSWE.east=optionsPreview.nswe.west + dpp* mousePoint.x;
					EditNSWE.east=TruncateByDegreesPerPixel(EditNSWE.east, dpp);
					//printf("x: %i  %f\n",mousePoint.x, east);
				}
				else if (hwnd==hwndPreviewCropbarNorth)	{
					if (mousePoint.y<0)
						mousePoint.y=0;
					if (mousePoint.y > optionsPreview.height - optionsPreview.width/MAX_ASPECT_RATIO)		//prevent too thin an aspect ratio
						mousePoint.y = optionsPreview.height - optionsPreview.width/MAX_ASPECT_RATIO;
					if (mousePoint.y > optionsPreview.height - PCB_GRABWIDTH*2)		//then also constrain to a usable size
						mousePoint.y = optionsPreview.height - PCB_GRABWIDTH*2;



					SetWindowPos(hwnd, NULL, 0,0, optionsPreview.width, mousePoint.y, SWP_NOMOVE|SWP_NOOWNERZORDER);
					dpp = (optionsPreview.nswe.south-optionsPreview.nswe.north)/optionsPreview.height;
					EditNSWE.north=optionsPreview.nswe.north + dpp* mousePoint.y;
					EditNSWE.north=TruncateByDegreesPerPixel(EditNSWE.north, dpp);
					//printf("x: %i, y: %i %f\n",mousePoint.x, mousePoint.y,north);
				}
				else if (hwnd==hwndPreviewCropbarSouth)	{
					ClientToScreen(hwnd, &mousePoint);	//converts from the position here
					ScreenToClient(GetParent(hwnd), &mousePoint);	//to the one on the preview
					if (mousePoint.y>optionsPreview.height)
						mousePoint.y = optionsPreview.height;
					if (mousePoint.y < optionsPreview.width/MAX_ASPECT_RATIO)		//prevent too thin an aspect ratio
						mousePoint.y = optionsPreview.width/MAX_ASPECT_RATIO;
					if (mousePoint.y < PCB_GRABWIDTH*2)		//then also constrain to a usable size
						mousePoint.y = PCB_GRABWIDTH*2;


					dpp = (optionsPreview.nswe.south-optionsPreview.nswe.north)/optionsPreview.height;
					SetWindowPos(hwnd, NULL, 0,mousePoint.y, optionsPreview.width, optionsPreview.height-mousePoint.y, SWP_NOMOVE|SWP_NOOWNERZORDER);
					EditNSWE.south=optionsPreview.nswe.north + dpp* mousePoint.y;
					EditNSWE.south=TruncateByDegreesPerPixel(EditNSWE.south, dpp);
				}



				UpdateEditNSWEControls(&EditNSWE);
				UpdateBarsFromNSWE(&EditNSWE);
			}
			//HandlePreviewCropbarMouse(hwnd, msg, wParam, lParam);
			break;

	}
	return 0;
}


LRESULT CALLBACK PreviewCropbarWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{

	switch (msg) {
		case WM_SETCURSOR:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_MOUSEMOVE:
			HandleCropbarMouse(hwnd, msg, wParam, lParam);
			break;
		case WM_PAINT:
			PAINTSTRUCT ps;
			HDC hdc;
			hdc=BeginPaint(hwnd, &ps);
			if (mouseDragCropbar)	{
				PatBlt(hdc, 0, 0, ps.rcPaint.right, optionsPreview.height, PATINVERT);
			}
			EndPaint(hwnd, &ps);
			break;
		default:
			return DefWindowProc(hwnd,msg,wParam,lParam);
	}


	return 0;
}

LRESULT CALLBACK ColourSwatchWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	int swatchNum;

	switch (msg) {
		case WM_PAINT:
			COLORREF swatchColour;

			PAINTSTRUCT ps;
			HDC hdc;
			hdc=BeginPaint(hwnd, &ps);
			swatchNum = GetWindowLong(hwnd, 0);
			if (swatchNum & CS_MONTH)	{
				swatchNum= swatchNum & 0xFF;
				swatchColour = RGB(cMonthSwatch[swatchNum].R, cMonthSwatch[swatchNum].G, cMonthSwatch[swatchNum].B);
			}
			else	{
				swatchColour = RGB(cDaySwatch[swatchNum].R, cDaySwatch[swatchNum].G, cDaySwatch[swatchNum].B);
			}
			SetBkColor(hdc, swatchColour);
			ExtTextOut(hdc, 0,0, ETO_CLIPPED|ETO_OPAQUE, &ps.rcPaint, "", 0, NULL);
			EndPaint(hwnd, &ps);
		break;
		case WM_LBUTTONDOWN:
			COLORREF acrCustClr[16];

			CHOOSECOLOR	cc;
			memset(&cc, 0, sizeof(cc));
			cc.lStructSize = sizeof(cc);
			cc.hwndOwner = hwnd;
			cc.hInstance = hInst;
			cc.Flags = 0x00000100;
			cc.lpCustColors = acrCustClr;
			cc.rgbResult = GetWindowLong(hwnd, 0);

			for (int i=0; i<7; i++)	{
				acrCustClr[i] = RGB(cDaySwatch[i].R, cDaySwatch[i].G, cDaySwatch[i].B);
			}

			if (ChooseColor(&cc))	{
				swatchNum = GetWindowLong(hwnd, 0);
				if (swatchNum & CS_MONTH)	{
					swatchNum= swatchNum & 0xFF;
					cMonthSwatch[swatchNum].R = GetRValue(cc.rgbResult);
					cMonthSwatch[swatchNum].G = GetGValue(cc.rgbResult);
					cMonthSwatch[swatchNum].B = GetBValue(cc.rgbResult);
					cMonthSwatch[swatchNum].A = 255;
				}
				else	{
					cDaySwatch[swatchNum].R = GetRValue(cc.rgbResult);
					cDaySwatch[swatchNum].G = GetGValue(cc.rgbResult);
					cDaySwatch[swatchNum].B = GetBValue(cc.rgbResult);
					cDaySwatch[swatchNum].A = 255;
				}
				InvalidateRect(hwnd, 0, 0);
				SendMessage(hwndOverview, WT_WM_RECALCBITMAP, 0,0);
				SendMessage(hwndPreview, WT_WM_RECALCBITMAP, 0,0);
			}

		break;
		default:
			return DefWindowProc(hwnd,msg,wParam,lParam);

	}
		return 0;
}

LRESULT CALLBACK ColourByWeekdayWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {
		case WM_CREATE:
			int x;
			int y;
			x=y=0;
			for (int i=0;i<7;i++)	{
				hwndColourSwatchDay[i] = CreateWindow("ColourSwatch",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP, x, y, SWATCH_WIDTH_DAY, 20, hwnd, NULL, hInst, NULL);
				SetWindowLong(hwndColourSwatchDay[i], 0, i);
				CreateWindow("Static",szDayOfWeek[i],WS_CHILD | WS_VISIBLE | WS_BORDER,x,y+18, 41,20, hwnd, NULL, hInst, NULL);
				x+=41+MARGIN;
			}

			break;
		default:
			return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
}

LRESULT CALLBACK ColourByMonthWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {
		case WM_CREATE:
			int x;
			int y;
			x=y=0;
			for (int i=0;i<12;i++)	{
				hwndColourSwatchMonth[i] = CreateWindow("ColourSwatch",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP, x, y, SWATCH_WIDTH_MONTH, 20, hwnd, NULL, hInst, NULL);
				SetWindowLong(hwndColourSwatchMonth[i], 0, i|CS_MONTH);
				CreateWindow("Static",szMonthLetter[i],WS_CHILD | WS_VISIBLE | WS_BORDER,x,y+18, SWATCH_WIDTH_MONTH,20, hwnd, NULL, hInst, NULL);
				x+=SWATCH_WIDTH_MONTH+MARGIN;
			}

			break;
		default:
			return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
}


LRESULT CALLBACK ColourByDateWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {
		case WM_CREATE:
			int x;
			int y;
			x=0;
			y=0;
			CreateWindow("Static", "Cycle time (seconds):", WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP, x, y, TEXT_WIDTH_HALF, 20, hwnd, NULL, hInst, NULL);
			x+=TEXT_WIDTH_HALF + MARGIN;
			hwndEditColourCycle = CreateWindow("Edit","604800", WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP, x, y, TEXT_WIDTH_QUARTER, 20, hwnd, (HMENU)ID_EDITCOLOURCYCLE, hInst, NULL);
			SetWindowLongPtr(hwndEditColourCycle, GWLP_WNDPROC, (long)&EditIntegerProc);

			y+=TEXT_HEIGHT+MARGIN;
			x=0;
			hwndButtonRadioOneHour = CreateWindow("BUTTON","1 hour", WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP|BS_RADIOBUTTON, x, y, TEXT_WIDTH_QUARTER, 20, hwnd, (HMENU)ID_EDITONEHOUR, hInst, NULL);
			x+=TEXT_WIDTH_QUARTER + MARGIN;
			CreateWindow("BUTTON","1 day", WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP|BS_RADIOBUTTON, x, y, TEXT_WIDTH_QUARTER, 20, hwnd, (HMENU)ID_EDITONEDAY, hInst, NULL);
			x+=TEXT_WIDTH_QUARTER + MARGIN;
			HWND hwndRadioOneWeek;
			hwndRadioOneWeek = CreateWindow("BUTTON","1 week", WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP|BS_RADIOBUTTON, x, y, TEXT_WIDTH_QUARTER, 20, hwnd, (HMENU)ID_EDITONEWEEK, hInst, NULL);
			x+=TEXT_WIDTH_QUARTER + MARGIN;
			CreateWindow("BUTTON","1 year", WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP|BS_RADIOBUTTON, x, y, TEXT_WIDTH_QUARTER, 20, hwnd, (HMENU)ID_EDITONEYEAR, hInst, NULL);
			SendMessage(hwndRadioOneWeek, BM_SETCHECK, BST_CHECKED,0);

			break;
		case WM_COMMAND:	//pass any edits to the parent window that'll handle it
			SendMessage(GetParent(hwnd), msg, wParam, lParam);
			break;
		default:
			return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
}





int SignificantDecimals(double d)	//**NB: not a general algorithm, just for use in avoiding over-precise coords
{
	//this takes the degrees per pixel, and returns the amount to multiple by, then rounding, before dividing by the same number
	//so if every pixed was 0.04 degrees, we'd mulitply by 1000, round, then divide by 1000 to get a sane number of sig figs

	if (d<0)	d*=-1;

	if (d>1)	return 10;
	if (d>0.1)	return 100;
	if (d>0.01) return 1000;
	if (d>0.001) return 10000;
	if (d>0.0001) return 100000;
	if (d>0.00001) return 1000000;
	if (d>0.000001) return 10000000;

	return 100000000;
}

double TruncateByDegreesPerPixel(double d, double spp)
{
	long l;
	int roundfactor;

	roundfactor=SignificantDecimals(spp);

	d*=roundfactor;
	d+=0.5;
	l=(long)d;

	d=(double)l / roundfactor;

	return d;
}

void ConstrainNSWE(NSWE * d)
{

	if (d->north > 90)	d->north=90;
	if (d->south < -90)	d->south=-90;

	//I might allow W/E wrapping, so will change to -180 to 360
	if (d->west <-180)	d->west=-180;
	if (d->east >180)	d->east=180;

	return;
}

int BltNsweFromBackground(HDC hdc, NSWE * d, int height, int width, OPTIONS * oSrc)
{
	HDC memDC;
	HGDIOBJ hOldBitmap;

	int destX, destY, destWidth, destHeight;
	int srcX, srcY, srcWidth, srcHeight;

	double srcLongSpan, srcLatSpan;
	double srcPpdX, srcPpdY;	//pixels per degree in x axis (should be the same)

	NSWE *s;	//the src (aka background) nswe

	s=&oSrc->nswe;

	srcLongSpan = oSrc->nswe.east - oSrc->nswe.west;
	srcLatSpan = oSrc->nswe.south - oSrc->nswe.north;

	srcPpdX = oSrc->width / srcLongSpan;
	srcPpdY = oSrc->height / srcLatSpan;


	srcX = (d->west - s->west) * srcPpdX;
	srcY = (d->south - s->south) * srcPpdY;

	srcWidth = srcLongSpan * srcPpdX;
	srcHeight = srcLatSpan * srcPpdY;
	destX =0; destY = 0;
	destWidth = width;	destHeight = height;

	memDC = CreateCompatibleDC(hdc);
	hOldBitmap=SelectObject(memDC, hbmBackground);


	SetStretchBltMode(hdc, HALFTONE);
	StretchBlt(hdc, destX, destY, destWidth, destHeight, memDC, srcX, srcY, srcWidth, srcHeight, SRCCOPY);
	SelectObject(memDC, hOldBitmap);

	DeleteDC(memDC);
	return 0;
}


int CreateBackground(HBITMAP * hbm, OPTIONS *oP, OPTIONS *oB, LOCATIONHISTORY * lh)
{
	BM backgroundBM;

	bitmapInit(&backgroundBM, oB, lh);
	PlotPaths(&backgroundBM, lh, oB);


	return 0;
}


int DialogHBMtoPNG(HWND hwnd, BM * bm, char *recommendedname)
{
	char filename[MAX_PATH];
	OPENFILENAME ofn;

	sprintf(filename, recommendedname);

	memset(&ofn,0,sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = hInst;
	filename[0]=0;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = "Export PNG to...";
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = "png";
	ofn.Flags =OFN_OVERWRITEPROMPT;
	ofn.lpstrFilter = "PNG files (*.png)\0*.png\0\0";


	if (GetSaveFileName(&ofn)==0)
		return 0;

	printf(recommendedname);
	int error = lodepng_encode32_file(filename, bm->bitmap, bm->width, bm->height);
	if(error) fprintf(stderr, "LodePNG error %u: %s\n", error, lodepng_error_text(error));


	return 0;
}
