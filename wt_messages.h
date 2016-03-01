#define WT_WM_RECALCBITMAP WM_APP+0	//Redraw the map from the loaded list of locations - this will do it as soon as message received
#define WT_WM_QUEUERECALC WM_APP+1	//Signal that the map needs to be replotted (essentially this starts a timer) so we don't waste time
#define WT_WM_SIGNALMOUSEWHEEL WM_APP+2	//We shouldn't send WM_MOUSEWHEEL between functions, but send this to down propagate to child window if required
//I've got up +49, the graph program starts at +100
#define WT_WM_TAB_GETEXPORTHEIGHT


#define WT_WM_GRAPH_REDRAW (WM_APP+100)
#define WT_WM_GRAPH_RECALCDATA (WM_APP+101)

#define WT_WM_GRAPH_SETSERIESCOLOR (WM_APP+102)
#define WT_WM_GRAPH_SETSERIESX (WM_APP+103)
#define WT_WM_GRAPH_SETSERIESY (WM_APP+104)


#define WT_WM_GRAPH_SETREGION (WM_APP+110)

