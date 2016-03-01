#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include "wttabs.h"

extern HWND hwndTab;
extern HINSTANCE hInst;

HWND hwndTabExport;

int CreateTabsAndTabWindows(HWND hwnd)
{

	TCITEM tab1Data;

	RECT rectTab;
	RECT rectWnd;

	int windowleft, windowbottom, windowwidth, windowheight;


    tab1Data.mask=TCIF_TEXT;
    tab1Data.pszText="Export";
	TabCtrl_InsertItem(hwndTab, TAB_EXPORT, &tab1Data);

    tab1Data.mask=TCIF_TEXT;
    tab1Data.pszText="Regions";
	TabCtrl_InsertItem(hwndTab, TAB_REGIONS, &tab1Data);

    tab1Data.pszText="Statistics";
	TabCtrl_InsertItem(hwndTab, TAB_STATISTICS, &tab1Data);

    tab1Data.pszText="Graphs";
	TabCtrl_InsertItem(hwndTab, TAB_GRAPHS, &tab1Data);


	TabCtrl_GetItemRect(hwnd, 0, &rectTab);

	GetClientRect(hwnd, &rectWnd);

//	printf("\nTabs: %i %i %i %i, wnd %i %i%i %i", rectTab.left, rectTab.top, rectTab.right, rectTab.bottom, rectWnd.left, rectWnd.top, rectWnd.right, rectWnd.bottom);



	hwndTabExport = CreateWindow("TabExport", NULL, WS_CHILD|WS_VISIBLE, rectWnd.left, rectTab.bottom, rectWnd.right, rectWnd.bottom-rectTab.bottom, hwnd, NULL, hInst, NULL);

	return 0;
}


LRESULT CALLBACK TabExportWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {
	case WM_CREATE:
		int x,y;
		const int margin=10;
		const int height=20;
		x=margin;y=margin;
		CreateWindow("Static","Width:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 100, height, hwnd, 0, hInst, NULL);
		x+=100+margin;
		CreateWindow("Edit","800", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 100, height, hwnd, 0, hInst, NULL);
		y+=margin+height;

		x=margin;
		CreateWindow("Static","Height:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 100, height, hwnd, 0, hInst, NULL);
		x+=100+margin;
		CreateWindow("Edit","600", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 100, height, hwnd, 0, hInst, NULL);
		x=margin;
		y+=margin+height;

		CreateWindow("BUTTON","Export KML...", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 100, height, hwnd, 0, hInst, NULL);

		break;

	default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}

		return DefWindowProc(hwnd,msg,wParam,lParam);
}


LRESULT CALLBACK MainWndProc_OnTabNotify(HWND hwnd, int id, NMHDR * nmh)
{

	switch (nmh->code)
        {
            case TCN_SELCHANGING:
                {
                    // Return FALSE to allow the selection to change.
                    return FALSE;
                }

            case TCN_SELCHANGE:
                {
                    int n = TabCtrl_GetCurSel(hwndTab);
					printf("note %i", n);
					if (n==TAB_EXPORT)	{
						ShowWindow(hwndTabExport, SW_SHOW);
					}	else	{
						ShowWindow(hwndTabExport, SW_HIDE);
					}
                    break;
                }
        }
        return TRUE;

}

