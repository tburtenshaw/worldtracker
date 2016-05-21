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
extern LOCATIONHISTORY locationHistory;

HWND hwndTabImport;
HWND hwndTabExport;
HWND hwndTabStatistics;
HWND hwndTabRegions;

//Only ever local
HWND hwndTabExportHeightEdit;
HWND hwndTabExportWidthEdit;
HWND hwndImportList;
HWND hwndImportDetails;
HWND hwndRegionList;


int InitTabWindowClasses(void)
{
	WNDCLASS wc;

	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)TabImportWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "TabImport";
	wc.lpszMenuName = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;

	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)TabExportWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "TabExport";
	wc.lpszMenuName = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;

	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)TabRegionsWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "TabRegions";
	wc.lpszMenuName = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;

	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)TabStatisticsWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "TabStatistics";
	wc.lpszMenuName = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;


	return 1;
}

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


	hwndTabImport = CreateWindow("TabImport", NULL, WS_CHILD|WS_VISIBLE, rectWnd.left, rectTab.bottom, rectWnd.right, rectWnd.bottom-rectTab.bottom, hwnd, NULL, hInst, NULL);
	hwndTabExport = CreateWindow("TabExport", NULL, WS_CHILD, rectWnd.left, rectTab.bottom, rectWnd.right, rectWnd.bottom-rectTab.bottom, hwnd, NULL, hInst, NULL);
	hwndTabRegions = CreateWindow("TabRegions", NULL, WS_CHILD, rectWnd.left, rectTab.bottom, rectWnd.right, rectWnd.bottom-rectTab.bottom, hwnd, NULL, hInst, NULL);
	hwndTabStatistics = CreateWindow("TabStatistics", NULL, WS_CHILD, rectWnd.left, rectTab.bottom, rectWnd.right, rectWnd.bottom-rectTab.bottom, hwnd, NULL, hInst, NULL);
	return 0;
}

LRESULT CALLBACK TabImportWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {
		case WM_CREATE:
			int x,y;
			const int margin=10;
			const int height=20;
			x=margin;y=margin;
			CreateWindow("Static","Imported files:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 250, height, hwnd, 0, hInst, NULL);
			y+=margin+height;
			hwndImportList = CreateWindow(WC_LISTBOX,"Imported", WS_CHILD | WS_VISIBLE | WS_BORDER|LBS_NOTIFY|LBS_WANTKEYBOARDINPUT, x, y, 250, height*6, hwnd, 0, hInst, NULL);

			//onto the right hand side
			x=250+margin+margin;
			y=margin;
			CreateWindow("Static","Details:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 250, height, hwnd, 0, hInst, NULL);
			y+=margin+height;
			hwndImportDetails = CreateWindow("Edit","Information", ES_READONLY|ES_MULTILINE|WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 250, height *6, hwnd, 0, hInst, NULL);

			break;
		case WT_WM_TAB_ADDLASTIMPORTEDFILE:
			LOCATIONHISTORY *lh;
			IMPORTEDFILE *importedFile;
			lh=(void *)lParam;
			importedFile=lh->firstImportedFile;
			if (importedFile)	{
				while (importedFile->next)	{
					importedFile=importedFile->next;	//while there's still a next...
				}

				char ext[MAX_PATH];
				char fname[MAX_PATH];
				_splitpath(importedFile->fullFilename, NULL, NULL, fname, ext);
				sprintf(importedFile->displayFilename, "%s%s", fname, ext);
				SendMessage(hwndImportList, LB_INSERTSTRING, -1, (LPARAM)importedFile->displayFilename);
			}

			printf("\nFile:%s",importedFile->fullFilename);

			break;
		case WT_WM_TAB_UPDATEINFO:
			{
			char importDetails[1024];
			IMPORTEDFILE * importedFile;
			importedFile = GetInputFileByIndex(&locationHistory, wParam);
			sprintf(importDetails, "Filename: %s\r\nSize: %i bytes", importedFile->fullFilename, importedFile->filesize);
			Edit_SetText(hwndImportDetails, importDetails);
			}
			break;

		case WM_COMMAND:
			HANDLE_WM_COMMAND(hwnd,wParam,lParam, TabImportWndProc_OnCommand);
			break;
		case WM_VKEYTOITEM:
			//printf("\nChar: %i", LOWORD(wParam));
			switch (LOWORD(wParam))	{
				case VK_DELETE:
					int id =SendMessage((HWND)lParam, LB_GETCURSEL, 0,0);
					printf("\nDELETE %i", id);
					if (id<0)	{	//if nothing is selected, don't do anything
						return -1;
					}
					DeleteInputFile(&locationHistory, id);
					SendMessage(hwndImportList, LB_DELETESTRING, min(id,locationHistory.numinputfiles-1), 0);
					SendMessage(hwndImportList, LB_SETCURSEL, id, 0);
					SendMessage(hwnd, WT_WM_TAB_UPDATEINFO, id, 0);
					SendMessage(GetParent(hwndTab), WT_WM_RECALCBITMAP, 0,0);
					return -1;
					break;
				default:
					return DefWindowProc(hwnd,msg,wParam,lParam);
			}

			break;
		default:
			return DefWindowProc(hwnd,msg,wParam,lParam);

	}
	return 0;
}

LRESULT CALLBACK TabImportWndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	printf("\nImport Command %i %i %i", id, hwndCtl, codeNotify);
	switch (codeNotify)	{
		case LBN_SELCHANGE:
			//printf("\nChange %i", SendMessage(hwndCtl, LB_GETCURSEL, 0,0));
			int index;
			index=SendMessage(hwndCtl, LB_GETCURSEL, 0, 0);
			SendMessage(hwnd, WT_WM_TAB_UPDATEINFO, index, 0);

			break;
	}

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
		hwndRegionList = CreateWindow(WC_LISTBOX,"Regions:", WS_CHILD | WS_VISIBLE | WS_BORDER|LVS_LIST|LBS_NOTIFY|LBS_WANTKEYBOARDINPUT, x, y, 100, height*8, hwnd, 0, hInst, NULL);

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
			printf("\n%s",szRegionName);
			SendMessage(hwndRegionList, LB_INSERTSTRING, -1, (LPARAM)szRegionName);
			r=r->next;
		}
		break;

	case WM_VKEYTOITEM:
		//printf("\nChar: %i", LOWORD(wParam));
		switch (LOWORD(wParam))	{
			case VK_DELETE:
				int id = SendMessage((HWND)lParam, LB_GETCURSEL, 0,0);

				printf("\nDELETE %i", id);
				if (id<0)	{
					return -1;
				}
				int result = DeleteRegionByIndex(&regionFirst, id);
				SendMessage(hwndRegionList, LB_DELETESTRING, id, 0);
				SendMessage(hwndRegionList, LB_SETCURSEL, id-result, 0);
				SendMessage(GetParent(hwndTab), WT_WM_RECALCBITMAP, 0,0);
				return -1;
			case VK_INSERT:
				r=regionFirst;
				int i;
			i=0;
		while (r)	{
			printf("\n%i",i);
			i++;
			if (r->title)	{
				printf(r->title);
			}
			else if (r->type == REGIONTYPE_EXCLUSION)	{
				printf("Exclusion");
			}
			printf("%f",r->nswe.north);
			r=r->next;
		}

				break;

			break;
				default:
					return DefWindowProc(hwnd,msg,wParam,lParam);
			}

			break;



	default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}

		return DefWindowProc(hwnd,msg,wParam,lParam);
}

LRESULT CALLBACK TabStatisticsWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {
	case WM_CREATE:
		int x,y;
		const int margin=10;
		const int height=20;
		x=margin;y=margin;
		HWND points = CreateWindow("Static","Points:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 120, height, hwnd, 0, hInst, NULL);
		SendMessage(points, WM_SETFONT, (WPARAM)hFontDialog, TRUE);
		y+=height+margin;
		CreateWindow("Static","Total distance:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 120, height, hwnd, 0, hInst, NULL);
		y+=height+margin;

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
					if (n==TAB_IMPORT)	{
						ShowWindow(hwndTabImport, SW_SHOW);
					}	else	{
						ShowWindow(hwndTabImport, SW_HIDE);
					}


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

					if (n==TAB_STATISTICS)	{
						ShowWindow(hwndTabStatistics, SW_SHOW);
					}	else	{
						ShowWindow(hwndTabStatistics, SW_HIDE);
					}


                    break;
                }
        }
        return TRUE;

}

