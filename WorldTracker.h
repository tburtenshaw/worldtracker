/* This is the main Windows-based program */
LOCATIONHISTORY locationHistory;
HINSTANCE hInst;		// Instance handle
HWND hWndMain;		//Main window handle
HWND hwndTab;		//Tab control with settings, stats etc.

HWND  hWndStatusbar;
//The dialog hwnds
HWND hwndStaticFilename;

HWND hwndStaticNorth;
HWND hwndStaticSouth;
HWND hwndStaticWest;
HWND hwndStaticEast;

HWND hwndEditNorth;
HWND hwndEditSouth;
HWND hwndEditWest;
HWND hwndEditEast;

HWND hwndStaticPreset;
HWND hwndEditPreset;

HWND hwndStaticExportHeading;
HWND hwndStaticExportWidth;
HWND hwndEditExportWidth;
HWND hwndStaticExportHeight;
HWND hwndEditExportHeight;

HWND hwndEditDateTo;
HWND hwndEditDateFrom;
HWND hwndEditThickness;
HWND hwndEditColourCycle;
HWND hwndComboboxColourBy;

HWND hwndDateSlider;

HWND hwndColourByDate;
HWND hwndColourByWeekday;
HWND hwndColourByMonth;

HWND hwndColourSwatchDay[7];
HWND hwndColourSwatchMonth[12];
COLOUR cDaySwatch[7];
COLOUR cMonthSwatch[12];
COLOUR cBlack;
COLOUR cWhite;

OPTIONS optionsExport;

//Multithread stuff
CRITICAL_SECTION critAccessLocations;	//if we are using locations (perhaps I should distinguish between reading at writing)
long hbmQueueSize;	//this is increased when a thread is created, so the thread can check that nothing was created after it

CRITICAL_SECTION critAccessLocations;
CRITICAL_SECTION critAccessPreviewHBitmap;

DWORD WINAPI LoadKMLThread(void *LoadKMLThreadData);
DWORD WINAPI ThreadSaveKML(OPTIONS *info);
DWORD WINAPI ThreadSetHBitmap(long queuechit);	//we actually only take someone from the back of the queue

LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
void MainWndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
//LRESULT CALLBACK MainWndProc_OnNotify(HWND hwnd, int id, NMHDR * nmh); (currently this is only the tab window, so that handles everything


LRESULT CALLBACK DateSliderWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK ColourSwatchWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);

LRESULT CALLBACK ColourByDateWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK ColourByWeekdayWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK ColourByMonthWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);


int	mouseDragDateSlider;

int UpdateEditNSWEControls(NSWE * d);
int UpdateBarsFromNSWE(NSWE * d);
int UpdateExportAspectRatioFromOptions(OPTIONS * o, int forceHeight);
int UpdateDateControlsFromOptions(OPTIONS * o);

int PreviewWindowFitToAspectRatio(HWND hwnd, int mainheight, int mainwidth, double aspectratio);	//aspect ratio here is width/height, if 0, it doesn't fits to the screen

int PaintDateSlider(HWND hwnd, LOCATIONHISTORY * lh, OPTIONS *o);

int ExportKMLDialogAndComplete(HWND hwnd, OPTIONS * o, LOCATIONHISTORY *lh);
int HandleEditControls(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
int HandleEditDateControls(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
int HandleComboColourBy(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
int HandleColourCycleRadiobuttons(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);

void ConstrainNSWE(NSWE * d);
int SignificantDecimals(double d);
double TruncateByDegreesPerPixel(double d, double spp);

void InitiateColours(void);
void InitStrings(void);
int InitWindowClasses(void);

int CreateTabsAndTabWindows(HWND hwnd);
