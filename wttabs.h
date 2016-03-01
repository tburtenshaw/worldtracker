#define TAB_EXPORT 0
#define TAB_REGIONS 1
#define TAB_STATISTICS 2
#define TAB_GRAPHS 3


int CreateTabsAndTabWindows(HWND hwnd);
LRESULT CALLBACK TabExportWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK MainWndProc_OnTabNotify(HWND hwnd, int id, NMHDR * nmh);
