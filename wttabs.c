#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include "mytrips.h"
#include "wttabs.h"
#include "wt_messages.h"
#include "worldtracker.h"

extern HWND hwndTab;
extern HINSTANCE hInst;
extern WORLDREGION * regionFirst;
extern OPTIONS optionsPreview;

HWND hwndTabExport;

HWND hwndTabExportHeightEdit;
HWND hwndTabExportWidthEdit;



HWND hwndTabRegions;

int CreateTabsAndTabWindows(HWND hwnd)
{

	TCITEM tab1Data;

	RECT rectTab;
	RECT rectWnd;

	int windowleft, windowbottom, windowwidth, windowheight;

    tab1Data.mask=TCIF_TEXT;
    tab1Data.pszText="Import";
	TabCtrl_InsertItem(hwndTab, TAB_IMPORT, &tab1Data);


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
	hwndTabRegions = CreateWindow("TabRegions", NULL, WS_CHILD, rectWnd.left, rectTab.bottom, rectWnd.right, rectWnd.bottom-rectTab.bottom, hwnd, NULL, hInst, NULL);
	return 0;
}


LRESULT CALLBACK TabExportWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	char buffer[255];

	switch (msg) {
	case WM_CREATE:
		int x,y;
		const int margin=10;
		const int height=20;
		x=margin;y=margin;
		CreateWindow("Static","Width:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 100, height, hwnd, 0, hInst, NULL);
		x+=100+margin;
		hwndTabExportWidthEdit = CreateWindow("Edit","2048", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 100, height, hwnd, 0, hInst, NULL);
		y+=margin+height;

		x=margin;
		CreateWindow("Static","Height:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 100, height, hwnd, 0, hInst, NULL);
		x+=100+margin;
		hwndTabExportHeightEdit = CreateWindow("Edit","600", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 100, height, hwnd, 0, hInst, NULL);
		x=margin;
		y+=margin+height;

		CreateWindow("BUTTON","Export KML...", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 100, height+2, hwnd, 0, hInst, NULL);

		x=200+3*margin;
		y=margin;
		CreateWindow("Edit","Title", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 200, height, hwnd, 0, hInst, NULL);
		y+=margin+height;
		CreateWindow("Edit","Description", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 200, height*4, hwnd, 0, hInst, NULL);
		break;

	case WT_WM_TAB_GETEXPORTHEIGHT:
		GetWindowText(hwndTabExportHeightEdit, buffer, 255);
		return atol(&buffer[0]);
		break;
	case WT_WM_TAB_GETEXPORTWIDTH:
		GetWindowText(hwndTabExportWidthEdit, buffer, 255);
		return atol(&buffer[0]);
		break;
	case WT_WM_TAB_SETEXPORTHEIGHT:
		sprintf(buffer, "%i", wParam);
		SetWindowText(hwndTabExportHeightEdit, buffer);
		break;
	case WT_WM_TAB_SETEXPORTWIDTH:
		sprintf(buffer, "%i", wParam);
		SetWindowText(hwndTabExportWidthEdit, buffer);
		break;

	case WM_COMMAND:
		HANDLE_WM_COMMAND(hwnd,wParam,lParam,TabExportWndProc_OnCommand);
		break;

	default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}

		return 0;
}

LRESULT CALLBACK TabExportWndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	switch (codeNotify)	{
		case EN_CHANGE:
			if (hwndCtl!=GetFocus())
				return 0;

			if (hwndCtl==hwndTabExportWidthEdit) optionsPreview.forceheight=0;
			else if (hwndCtl==hwndTabExportHeightEdit) optionsPreview.forceheight=1;
			else return 0;

			UpdateExportAspectRatioFromOptions(&optionsPreview, optionsPreview.forceheight);
			break;
		case BN_CLICKED:
			ExportKMLDialogAndComplete(hwnd, &optionsPreview);
			return 0;

		default:
			return 0;
	}

	return 0;
}

int UpdateExportAspectRatioFromOptions(OPTIONS * o, int forceHeight)
{
	int exportHeight;
	int exportWidth;

	if ((o->width<1)||(o->height<1))
		return 0;

	if (forceHeight)	{
		exportHeight=SendMessage(hwndTabExport, WT_WM_TAB_GETEXPORTHEIGHT, 0, 0);

		exportWidth=exportHeight*o->width/o->height;
		printf("height: %i, ",exportHeight);
		printf("width: %i\n",exportWidth);
		SendMessage(hwndTabExport, WT_WM_TAB_SETEXPORTWIDTH, exportWidth, 0);

	}
	else	{	//we'll be forcing the width static
		exportWidth=SendMessage(hwndTabExport, WT_WM_TAB_GETEXPORTWIDTH, 0, 0);
		exportHeight=exportWidth*o->height/o->width;
		printf("height: %i, ",exportHeight);
		printf("width: %i\n",exportWidth);

		SendMessage(hwndTabExport, WT_WM_TAB_SETEXPORTHEIGHT, exportHeight, 0);
	}
	return 1;
}



LRESULT CALLBACK TabRegionsWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {
	case WM_CREATE:
		int x,y;
		const int margin=10;
		const int height=20;
		x=margin;y=margin;
//		CreateWindow("Static","Regions:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 100, height, hwnd, 0, hInst, NULL);
//		y+=height+margin;
		HWND hwndRegionList = CreateWindow(WC_LISTBOX,"Regions:", WS_CHILD | WS_VISIBLE | WS_BORDER|LVS_LIST, x, y, 100, height*8, hwnd, 0, hInst, NULL);

		WORLDREGION * r;
		char szRegionName[256];

		r=regionFirst;
		while (r)	{
			if (r->title)	{
				sprintf(szRegionName, r->title);
			}
			else if (r->type == REGIONTYPE_EXCLUSION)	{
				sprintf(szRegionName, "Exclusion");
			}
			SendMessage(hwndRegionList, LB_ADDSTRING, 0, (LPARAM)szRegionName);
			r=r->next;
		}


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
					if (n==TAB_REGIONS)	{
						ShowWindow(hwndTabRegions, SW_SHOW);
					}	else	{
						ShowWindow(hwndTabRegions, SW_HIDE);
					}

                    break;
                }
        }
        return TRUE;

}

