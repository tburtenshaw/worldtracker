//typedef struct sRasterFont RASTERFONT;
//typedef struct sRasterChar RASTERCHAR;


struct sRasterFont	{
	RASTERCHAR * c[128];
	int approxheight;
};

struct sRasterChar	{
	int width;
	int height;
	unsigned char *pixelarray;
};


int LoadRasterFont(RASTERFONT * rasterfont);
RASTERCHAR * plotChar(BM * bm, RASTERFONT * rasterfont, int x, int y, unsigned char character, COLOUR * colourText);
int DestroyRasterFont(RASTERFONT * rasterfont);
int fontGetStringWidth(RASTERFONT *rasterfont, char *szText);
