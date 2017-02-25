#define TAB_IMPORT 0
#define TAB_EXPORT 1
#define TAB_REGIONS 2
#define TAB_STATISTICS 3
#define TAB_GRAPHS 4

int InitTabWindowClasses(void);
int CreateTabsAndTabWindows(HWND hwnd);
LRESULT CALLBACK MainWndProc_OnTabNotify(HWND hwnd, int id, NMHDR * nmh);

LRESULT CALLBACK TabImportWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK TabImportWndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);

LRESULT CALLBACK TabExportWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK TabExportWndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);

int UpdateExportAspectRatioFromOptions(OPTIONS * o, int forceHeight);

LRESULT CALLBACK TabRegionsWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);

LRESULT CALLBACK TabStatisticsWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);

LRESULT CALLBACK TabGraphsWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
