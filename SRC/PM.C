/*

DKB Raytracer for OS/2 2.0 Presentation Manager
by Michael Caldwell (mcaldwel@netcom.com)

*/


#define INCL_PM
#include <os2.h>
#include <time.h>
#include "stats.h"


extern long Number_Of_Pixels, Number_Of_Rays, Number_Of_Pixels_Supersampled;
extern long Ray_Sphere_Tests, Ray_Sphere_Tests_Succeeded;
extern long Ray_Plane_Tests, Ray_Plane_Tests_Succeeded;
extern long Ray_Triangle_Tests, Ray_Triangle_Tests_Succeeded;
extern long Ray_Quadric_Tests, Ray_Quadric_Tests_Succeeded;
extern long Ray_Quartic_Tests, Ray_Quartic_Tests_Succeeded;
extern long Bounding_Region_Tests, Bounding_Region_Tests_Succeeded;
extern long Calls_To_Noise, Calls_To_DNoise;
extern long Shadow_Ray_Tests, Shadow_Rays_Succeeded;
extern long Reflected_Rays_Traced, Refracted_Rays_Traced;
extern long Transmitted_Rays_Traced;
extern time_t Start_Time;
extern int Screen_Width;
extern int Screen_Height;
extern HAB hab;
extern HWND hwndMain;
extern PSZ pszImage;


HDC hdc = (HDC) NULL;
HPS hps = (HPS) NULL;
HWND hwndStats = (HWND) NULL;
HWND hwndImage = (HWND) NULL;
LONG yLast;


static HWND hwndFrame = (HWND) NULL;


MRESULT EXPENTRY StatsWndProc (HWND, ULONG, MPARAM, MPARAM);
VOID PrintToTranscript (PSZ pszFormat, ...);


VOID SetStatisticItem (SHORT sID, LONG lValue) {
	PSZ pszValue [32];

	sprintf (pszValue, "%ld", lValue);
	WinSetDlgItemText (hwndStats, sID, (PSZ) pszValue);
}


VOID UpdateStats (SHORT y) {
	PSZ pszElapsedTime [32];
	time_t tElapsed;
	struct tm * ptmElapsed;

	if (! hwndStats) {
		hwndStats = WinLoadDlg (HWND_DESKTOP, (HWND) NULL, StatsWndProc,
				(HMODULE) NULL, ID_STATS, NULL);
	}
	if (y == -2) {
		WinShowWindow (hwndStats, TRUE);
		WinSetDlgItemText (hwndStats, ID_LINE, "");
		SetStatisticItem (ID_PIXELS, 0L);
		SetStatisticItem (ID_RAYS, 0L);
		SetStatisticItem (ID_SUPERPIXELS, 0L);
		SetStatisticItem (ID_SPHERE_TESTS, 0L);
		SetStatisticItem (ID_SPHERE_HITS, 0L);
		SetStatisticItem (ID_PLANE_TESTS, 0L);
		SetStatisticItem (ID_PLANE_HITS, 0L);
		SetStatisticItem (ID_TRIANGLE_TESTS, 0L);
		SetStatisticItem (ID_TRIANGLE_HITS, 0L);
		SetStatisticItem (ID_QUADRIC_TESTS, 0L);
		SetStatisticItem (ID_QUADRIC_HITS, 0L);
		SetStatisticItem (ID_QUARTIC_TESTS, 0L);
		SetStatisticItem (ID_QUARTIC_HITS, 0L);
		SetStatisticItem (ID_BOUNDS_TESTS, 0L);
		SetStatisticItem (ID_BOUNDS_HITS, 0L);
		SetStatisticItem (ID_NOISE, 0L);
		SetStatisticItem (ID_DNOISE, 0L);
		SetStatisticItem (ID_SRAY_TESTS, 0L);
		SetStatisticItem (ID_BOBJECTS, 0L);
		SetStatisticItem (ID_REFLECTED_RAYS, 0L);
		SetStatisticItem (ID_REFRACTED_RAYS, 0L);
		SetStatisticItem (ID_TRANSMITTED_RAYS, 0L);
		WinSetDlgItemText (hwndStats, ID_ELAPSED_TIME, "");
		return;
	}
	SetStatisticItem (ID_LINE, y);
	SetStatisticItem (ID_PIXELS, Number_Of_Pixels);
	SetStatisticItem (ID_RAYS, Number_Of_Rays);
	SetStatisticItem (ID_SUPERPIXELS, Number_Of_Pixels_Supersampled);
	SetStatisticItem (ID_SPHERE_TESTS, Ray_Sphere_Tests);
	SetStatisticItem (ID_SPHERE_HITS, Ray_Sphere_Tests_Succeeded);
	SetStatisticItem (ID_PLANE_TESTS, Ray_Plane_Tests);
	SetStatisticItem (ID_PLANE_HITS, Ray_Plane_Tests_Succeeded);
	SetStatisticItem (ID_TRIANGLE_TESTS, Ray_Triangle_Tests);
	SetStatisticItem (ID_TRIANGLE_HITS, Ray_Triangle_Tests_Succeeded);
	SetStatisticItem (ID_QUADRIC_TESTS, Ray_Quadric_Tests);
	SetStatisticItem (ID_QUADRIC_HITS, Ray_Quadric_Tests_Succeeded);
	SetStatisticItem (ID_QUARTIC_TESTS, Ray_Quartic_Tests);
	SetStatisticItem (ID_QUARTIC_HITS, Ray_Quartic_Tests_Succeeded);
	SetStatisticItem (ID_BOUNDS_TESTS, Bounding_Region_Tests);
	SetStatisticItem (ID_BOUNDS_HITS, Bounding_Region_Tests_Succeeded);
	SetStatisticItem (ID_NOISE, Calls_To_Noise);
	SetStatisticItem (ID_DNOISE, Calls_To_DNoise);
	SetStatisticItem (ID_SRAY_TESTS, Shadow_Ray_Tests);
	SetStatisticItem (ID_BOBJECTS, Shadow_Rays_Succeeded);
	SetStatisticItem (ID_REFLECTED_RAYS, Reflected_Rays_Traced);
	SetStatisticItem (ID_REFRACTED_RAYS, Refracted_Rays_Traced);
	SetStatisticItem (ID_TRANSMITTED_RAYS, Transmitted_Rays_Traced);
	tElapsed = time (NULL) - Start_Time;
	ptmElapsed = gmtime (& tElapsed);
	sprintf (pszElapsedTime, "%dd %02d:%02d:%02d", ptmElapsed->tm_yday,
			ptmElapsed->tm_hour, ptmElapsed->tm_min, ptmElapsed->tm_sec);
	WinSetDlgItemText (hwndStats, ID_ELAPSED_TIME, (PSZ) pszElapsedTime);
}


MRESULT EXPENTRY StatsWndProc (HWND hwnd, ULONG ulMessage, MPARAM mp1,
		MPARAM mp2) {
	switch (ulMessage) {
		case WM_CLOSE: {
		/* use dismiss dialog to just hide the window */
			WinDismissDlg (hwnd, 0);
			return (0L);
		}
	}/*endSwitch*/
	return (WinDefDlgProc (hwnd, ulMessage, mp1, mp2));
}/* end StatsWndProc */


void display_init (int x, int y) {
	BITMAPINFOHEADER2 bmih;
	DEVOPENSTRUC dos [5] = {NULL, "DISPLAY", NULL, NULL, NULL};
	HBITMAP hbm;
	LONG alData [2];
	SIZEL sizel;
	ULONG fl;

/* destroy any previous display */
	if (hps) {
		hbm = GpiSetBitmap (hps, (HBITMAP) NULL);
		if (hbm == BMB_ERROR) {
			PrintToTranscript ("GpiSetBitmap returned an error.\n");
		}
		if (GpiDeleteBitmap (hbm) == GPI_ERROR) {
			PrintToTranscript ("GpiDeleteBitmap failed.\n");
		}
	/* no need to do the associate
		if (GpiAssociate (hps, (HDC) NULL) == GPI_ERROR) {
			PrintToTranscript ("GpiAssociate failed.\n");
		}
	*/
		if (GpiDestroyPS (hps) == GPI_ERROR) {
			PrintToTranscript ("GpiDestroyPS failed.\n");
		}
		if (DevCloseDC (hdc) == DEV_ERROR) {
			PrintToTranscript ("DevCloseDC failed.\n");
		}
	}
	hdc = DevOpenDC (hab, OD_MEMORY, "*", 5L, (PDEVOPENDATA) & dos, (HDC) NULL);
	if (hdc == DEV_ERROR) {
		PrintToTranscript ("DevOpenDC failed.\n");
		return;
	}
	sizel.cx = x;
	sizel.cy = y;
	hps = GpiCreatePS (hab, hdc, & sizel, GPIA_ASSOC | GPIT_MICRO | PU_PELS);
	if (hps == GPI_ERROR) {
		PrintToTranscript ("GpiCreatePS failed.\n");
		DevCloseDC (hdc);
		return;
	}
	if (GpiQueryDeviceBitmapFormats (hps, 2L, alData) == GPI_ERROR) {
		PrintToTranscript ("GpiQueryDeviceBitmapFormats failed.\n");
		return;
	}
	if (x * y * alData [0] * alData [1] / 8 > 65535) {
		PrintToTranscript ("Image bitmap cannot be > 64K.\n");
		GpiDestroyPS (hps);
		DevCloseDC (hdc);
		hps = (HPS) NULL;
		hdc = (HDC) NULL;
		if (hwndFrame) {
			WinSendMsg (hwndFrame, WM_CLOSE, 0L, 0L);
		}
		return;
	}
	memset (& bmih, 0, sizeof (BITMAPINFOHEADER2));
	bmih.cbFix = sizeof (BITMAPINFOHEADER2);
	bmih.cx = x;
	bmih.cy = y;
	bmih.cPlanes = alData [0];
	bmih.cBitCount = alData [1];
	hbm = GpiCreateBitmap (hps, & bmih, 0L, (PBYTE) NULL, (PBITMAPINFO2) NULL);
	if (hbm == GPI_ERROR) {
		PrintToTranscript ("GpiCreateBitmap failed.\n");
		return;
	}
	if (GpiSetBitmap (hps, hbm) == BMB_ERROR) {
		PrintToTranscript ("GpiSetBitmap failed.\n");
		return;
	}
	if (GpiErase (hps) == GPI_ERROR) {
		PrintToTranscript ("GpiErase failed.\n");
		return;
	}
	if (GpiCreateLogColorTable (hps, 0L, LCOLF_RGB, 0L, 0L, (PLONG) NULL)
			== GPI_ERROR) {
		PrintToTranscript ("GpiCreateLogColorTable failed.\n");
		return;
	}
/* create window */
	x += WinQuerySysValue (HWND_DESKTOP, SV_CXDLGFRAME) * 2;
	y += WinQuerySysValue (HWND_DESKTOP, SV_CYDLGFRAME) * 2
			+ WinQuerySysValue (HWND_DESKTOP, SV_CYTITLEBAR);
	if (! hwndFrame) {
		fl = FCF_DLGBORDER | FCF_TITLEBAR | FCF_SYSMENU | FCF_MINBUTTON
				| FCF_ICON;
		hwndFrame = WinCreateStdWindow (HWND_DESKTOP, 0, & fl, pszImage,
				pszImage, 0, (HMODULE) NULL, ID_STATS, & hwndImage);
		WinSetWindowPos (hwndFrame, (HWND) NULL, 0, 0, x, y, SWP_MOVE
				| SWP_SIZE | SWP_SHOW);
	} else {
		WinSetWindowPos (hwndFrame, (HWND) NULL, 0, 0, x, y, SWP_SIZE);
	}
	WinInvalidateRect (hwndImage, NULL, FALSE);
	yLast = Screen_Height - 1;
}


VOID SetPixel (SHORT x, SHORT y, LONG lColor) {
	POINTL ptl;
	RECTL rcl;

/* if there's no bitmap or no window then forget it */
	if (! hps || ! hwndImage) {
		return;
	}
	y = Screen_Height - y - 1;
	ptl.x = x;
	ptl.y = y;
	if (GpiSetColor (hps, lColor) == GPI_ERROR) {
		PrintToTranscript ("GpiSetColor failed.\n");
	}
	if (GpiSetPel (hps, & ptl) == GPI_ERROR) {
		PrintToTranscript ("GpiSetPel failed.\n");
	}
	if (y != yLast || y == 0 && x == Screen_Width - 1) {
		rcl.xLeft = 0;
		rcl.yBottom = yLast;
		rcl.xRight = Screen_Width;
		rcl.yTop = yLast + 1;
		WinInvalidateRect (hwndImage, & rcl, FALSE);
		yLast = y;
	}
}


MRESULT EXPENTRY ImageWndProc (HWND hwnd, ULONG ulMessage, MPARAM mp1,
		MPARAM mp2) {
	switch (ulMessage) {
		case WM_PAINT: {
			HPS hpsScreen;
			POINTL aptl [3];
			RECTL rcl;

			hpsScreen = WinBeginPaint (hwnd, (HPS) NULL, & rcl);
			aptl [0].x = rcl.xLeft;
			aptl [0].y = rcl.yBottom;
			aptl [1].x = rcl.xRight;
			aptl [1].y = rcl.yTop;
			aptl [2].x = rcl.xLeft;
			aptl [2].y = rcl.yBottom;
			if (GpiBitBlt (hpsScreen, hps, 3L, aptl, ROP_SRCCOPY, 0L)
					== GPI_ERROR) {
				PrintToTranscript ("GpiBitBlt failed.\n");
			}
			WinEndPaint (hpsScreen);
			return (0L);
		}
		case WM_CLOSE: {
			WinDestroyWindow (hwndFrame);
			hwndFrame = (HWND) NULL;
			hwndImage = (HWND) NULL;
			return (0L);
		}
	}/*endSwitch*/
	return (WinDefWindowProc (hwnd, ulMessage, mp1, mp2));
}/* end ImageWndProc */
