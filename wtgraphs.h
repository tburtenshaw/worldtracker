//This does the windows part of the graph display
//Much of the bit-work happens in the shared library which can graph for the command line
//#include <windows.h>
//#include "mytrips.h"
//#include "worldtracker.h"

typedef struct sGraphInfo GRAPHINFO;

struct sGraphInfo	{
	HBITMAP hbmGraph;
	BM	bmGraph;
	int graphtype;
	int xsource;
	int ysource;
	int csource;

	unsigned long	fromtimestamp;
	unsigned long	totimestamp;

};



LRESULT CALLBACK GraphWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
int PaintMainGraph(HWND hwnd, GRAPHINFO* gi);
HBITMAP MakeHBitmapGraph(HWND hwnd, HDC hdc, GRAPHINFO * gi, LOCATIONHISTORY *lh);
