/*

DKB Raytracer for OS/2 2.0 Presentation Manager
by Michael Caldwell (mcaldwel@netcom.com)

*/

#define INCL_PM
#define INCL_WINMLE
#define INCL_WINSTDFILE
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tracepm.h"
#include "trace.h"


/* memory is allocated in 4K pages in OS/2 2.0 */
#define BUFFER_SIZE 4096
#define TRACE_STACK_SIZE 8192
#define QUEUE_SIZE 256
#define WM_INIT_OPTIONS WM_USER
#define WM_PRINT WM_USER + 1
#define WM_STATS WM_USER + 2
#define WM_PLOT WM_USER + 3
#define WM_MENUS WM_USER + 4
/* these defines are copied from frame.h */
#define DISPLAY 1
#define VERBOSE 2
#define DISKWRITE 4
#define ANTIALIAS 16
/* ------------------------------------- */


extern unsigned int Options;
extern char Output_File_Name [];
extern char OutputFormat;
extern int Screen_Width;
extern int Screen_Height;
extern double Antialias_Threshold;
extern int Quality;
extern int Stop_Flag;


FILEDLG	fd;
HAB		hab;
HMQ		hmq;
HWND		hwndMain = (HWND) NULL;
HWND		hwndTranscript = (HWND) NULL;
HWND		hwndMenu = (HWND) NULL;
IPT		ipt = 0;
PSZ		pszPrintToTranscriptError = "Error: cannot print to transcript.\n";
PSZ		pszAppName = "Trace PM";
PSZ		pszImage = "Image";


static HWND hwndFrame = (HWND) NULL;


MRESULT EXPENTRY MainWndProc (HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY TraceWndProc (HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY ImageWndProc (HWND, ULONG, MPARAM, MPARAM);
VOID _System StartTracePM (PSZ pszInputFileName);
VOID SetPixel (SHORT x, SHORT y, LONG lColor);
VOID UpdateStats (SHORT y);
void text_main (int argc, char * * argv);
void StartTrace (char * Input_File_Name);
void print_line_stats (register int y);
void display_init (int x, int y);


int main (int argc, char * * argv) {
	QMSG		qmsg;
	ULONG		fl;

	hab = WinInitialize (0);
	if (! hab)
		DosExit (EXIT_PROCESS, 1);

	hmq = WinCreateMsgQueue (hab, QUEUE_SIZE);
	if (! hmq) {
		WinTerminate (hab);
		DosExit (EXIT_PROCESS, 1);
	}
	if (! WinRegisterClass (hab, pszAppName, MainWndProc, 0L, 0)) {
		WinDestroyMsgQueue (hmq);
		WinTerminate (hab);
		DosExit (EXIT_PROCESS, 1);
	}
	if (! WinRegisterClass (hab, pszImage, ImageWndProc, CS_SIZEREDRAW, 0)) {
		WinDestroyMsgQueue (hmq);
		WinTerminate (hab);
		DosExit (EXIT_PROCESS, 1);
	}
	fl = FCF_STANDARD & ~ FCF_ACCELTABLE;
	hwndFrame = WinCreateStdWindow (HWND_DESKTOP, WS_VISIBLE, & fl, pszAppName,
			pszAppName, WS_VISIBLE, (HMODULE) NULL, ID_MAIN, & hwndMain);
	if (! hwndFrame) {
		WinDestroyMsgQueue (hmq);
		WinTerminate (hab);
		DosExit (EXIT_PROCESS, 1);
	}
	WinPostMsg (hwndMain, WM_INIT_OPTIONS, (MPARAM) argc, (MPARAM) argv);
	while (WinGetMsg (hmq, & qmsg, 0, 0, 0))
		WinDispatchMsg (hmq, & qmsg);

	WinDestroyMsgQueue (hmq);
	WinTerminate (hab);
	DosExit (EXIT_PROCESS, 0);
	return (0);
}/* end main */


VOID PrintToTranscript (PSZ pszFormat, ...) {
	PSZ		pszString;
	SHORT		sLength;
	va_list	valist;

	if (DosAllocMem ((PPVOID) & pszString, BUFFER_SIZE, fALLOC)) {
	/* zeros indicate an error */
		WinPostMsg (hwndMain, WM_PRINT, 0L, 0L);
		return;
	}
	va_start (valist, pszFormat);
	sLength = vsprintf (pszString, pszFormat, valist);
	va_end (valist);
	WinPostMsg (hwndMain, WM_PRINT, (MPARAM) pszString, (MPARAM) sLength);
}/* end PrintToTranscript */


VOID AddStringToTranscript (PSZ pszString, SHORT sLength) {
	if (! pszString) {
		sLength = strlen (pszPrintToTranscriptError);
		WinSendMsg (hwndTranscript, MLM_SETIMPORTEXPORT,
				(MPARAM) pszPrintToTranscriptError, (MPARAM) sLength);
		WinSendMsg (hwndTranscript, MLM_IMPORT, (MPARAM) & ipt,
				(MPARAM) sLength);
		return;
	}
	WinSendMsg (hwndTranscript, MLM_SETIMPORTEXPORT, (MPARAM) pszString,
			(MPARAM) sLength);
	WinSendMsg (hwndTranscript, MLM_IMPORT, (MPARAM) & ipt, (MPARAM) sLength);
	DosFreeMem (pszString);
}/* end AddStringToTranscript */


VOID DisableMenus (SHORT sDisable) {
	if (sDisable) sDisable = MIA_DISABLED;
	WinSendMsg (hwndMenu, MM_SETITEMATTR, MPFROM2SHORT (ID_TRACE, TRUE),
			MPFROM2SHORT (MIA_DISABLED, sDisable));
	WinSendMsg (hwndMenu, MM_SETITEMATTR, MPFROM2SHORT (ID_ABORT, TRUE),
			MPFROM2SHORT (MIA_DISABLED, sDisable == FALSE ? MIA_DISABLED : TRUE));
	WinSendMsg (hwndMenu, MM_SETITEMATTR, MPFROM2SHORT (ID_DISPLAY, TRUE),
			MPFROM2SHORT (MIA_DISABLED, sDisable));
	WinSendMsg (hwndMenu, MM_SETITEMATTR, MPFROM2SHORT (ID_OUTPUT, TRUE),
			MPFROM2SHORT (MIA_DISABLED, sDisable));
	WinSendMsg (hwndMenu, MM_SETITEMATTR, MPFROM2SHORT (ID_QUALITY, TRUE),
			MPFROM2SHORT (MIA_DISABLED, sDisable));
}/* end DisableMenus */


VOID TracePM () {
	TID			tid;

	memset (& fd, 0, sizeof (FILEDLG));
	fd.cbSize = sizeof (FILEDLG);
	fd.fl = FDS_CENTER | FDS_OPEN_DIALOG;
	fd.pszTitle = pszAppName;
	strcpy (fd.szFullFile, "*.dat");
	if (WinFileDlg (HWND_DESKTOP, hwndMain, & fd) && fd.lReturn == DID_OK) {
   	if (Options & DISKWRITE) {
			PSZ	pszS;
			PSZ	pszE;

			pszS = strrchr (fd.szFullFile, '\\') + 1;
			pszE = strchr (pszS, '.');
			if (pszE) {
				strncpy (Output_File_Name, pszS, pszE - pszS);
				Output_File_Name [pszE - pszS] = 0;
			} else {
				strcpy (Output_File_Name, pszS);
			}
      	switch (OutputFormat) {
         	case 'd':
					strcat (Output_File_Name, ".dis");
            	break;

         	case 't':
					strcat (Output_File_Name, ".tga");
            	break;
			}
		} else {
			Output_File_Name [0] = 0;
		}
		if (WinDlgBox (HWND_DESKTOP, hwndMain, TraceWndProc, (HMODULE) NULL,
				ID_TRACEDIALOG, fd.szFullFile)) {
			DisableMenus (TRUE);

		/* open display windows from thread 1 */
			if (Options & VERBOSE)
				print_line_stats (-2);
  			if (Options & DISPLAY)
    			display_init (Screen_Width, Screen_Height);

			if (DosCreateThread (& tid, (PFNTHREAD) StartTracePM,
					(ULONG) fd.szFullFile, 0, TRACE_STACK_SIZE)) {
				PrintToTranscript ("Error: unable to start trace thread.\n");
			}
		}
	}
}/* end TracePM */


VOID PostMessage (HWND hwnd, ULONG ulMessage, MPARAM mp1, MPARAM mp2) {
/* this function is for thread 2 (the trace thread) */
	while (! WinPostMsg (hwnd, ulMessage, mp1, mp2)) {
	/* give up the rest of this time slice and try the post message again */
		DosSleep (0L);
	}/*endWhile*/
}/* end PostMessage */


VOID _System StartTracePM (PSZ pszInputFileName) {
	Stop_Flag = FALSE;
	StartTrace (pszInputFileName);
	PostMessage (hwndMain, WM_MENUS, 0L, 0L);
}/* end StartTracePM */


void print_line_stats (register int y) {
	PostMessage (hwndMain, WM_STATS, (MPARAM) y, 0L);
}/* end print_line_stats */


#define RGB(r,g,b) ((LONG)(BYTE)(r)<<16|(LONG)(BYTE)(g)<<8|(BYTE)(b))


void display_plot (int x, int y, char Red, char Green, char Blue) {
	PostMessage (hwndMain, WM_PLOT, MPFROM2SHORT ((SHORT) x, (SHORT) y),
			(MPARAM) RGB (Red, Green, Blue));
}/* end display_plot */


MRESULT EXPENTRY MainWndProc (HWND hwnd, ULONG ulMessage, MPARAM mp1,
		MPARAM mp2) {
	switch (ulMessage) {
		case WM_COMMAND: {
			if (SHORT1FROMMP (mp2) == CMDSRC_MENU) {
				switch (SHORT1FROMMP (mp1)) {
					case ID_TRACE: {
						TracePM ();
						break;
					}
					case ID_ABORT: {
						Stop_Flag = TRUE;
						break;
					}
					case ID_EXIT: {
						WinPostMsg (hwndMain, WM_QUIT, 0L, 0L);
						break;
					}
					case ID_IMAGE: {
						if (Options & DISPLAY) {
							Options &= ~ DISPLAY;
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_IMAGE, TRUE),
									MPFROM2SHORT (MIA_CHECKED, FALSE));
						} else {
							Options |= DISPLAY;
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_IMAGE, TRUE),
									MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));
						}
						break;
					}
					case ID_STATISTICS: {
						if (Options & VERBOSE) {
							Options &= ~ VERBOSE;
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_STATISTICS, TRUE),
									MPFROM2SHORT (MIA_CHECKED, FALSE));
						} else {
							Options |= VERBOSE;
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_STATISTICS, TRUE),
									MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));
						}
						break;
					}
					case ID_TARGA: {
						if (Options & DISKWRITE && OutputFormat == 't') {
							Options &= ~ DISKWRITE;
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_TARGA, TRUE),
									MPFROM2SHORT (MIA_CHECKED, FALSE));
						} else {
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_DUMP, TRUE),
									MPFROM2SHORT (MIA_CHECKED, FALSE));
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_RAW, TRUE),
									MPFROM2SHORT (MIA_CHECKED, FALSE));
							Options |= DISKWRITE;
							OutputFormat = 't';
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_TARGA, TRUE),
									MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));
						}
						break;
					}
					case ID_DUMP: {
						if (Options & DISKWRITE && OutputFormat == 'd') {
							Options &= ~ DISKWRITE;
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_DUMP, TRUE),
									MPFROM2SHORT (MIA_CHECKED, FALSE));
						} else {
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_TARGA, TRUE),
									MPFROM2SHORT (MIA_CHECKED, FALSE));
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_RAW, TRUE),
									MPFROM2SHORT (MIA_CHECKED, FALSE));
							Options |= DISKWRITE;
							OutputFormat = 'd';
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_DUMP, TRUE),
									MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));
						}
						break;
					}
					case ID_RAW: {
						if (Options & DISKWRITE && OutputFormat == 'r') {
							Options &= ~ DISKWRITE;
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_RAW, TRUE),
									MPFROM2SHORT (MIA_CHECKED, FALSE));
						} else {
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_TARGA, TRUE),
									MPFROM2SHORT (MIA_CHECKED, FALSE));
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_DUMP, TRUE),
									MPFROM2SHORT (MIA_CHECKED, FALSE));
							Options |= DISKWRITE;
							OutputFormat = 'r';
							WinSendMsg (hwndMenu, MM_SETITEMATTR,
									MPFROM2SHORT (ID_RAW, TRUE),
									MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));
						}
						break;
					}
					case ID_0:
					case ID_1:
					case ID_2:
					case ID_3:
					case ID_4:
					case ID_5:
					case ID_6:
					case ID_7:
					case ID_8:
					case ID_9: {
						WinSendMsg (hwndMenu, MM_SETITEMATTR,
								MPFROM2SHORT (ID_0 + Quality, TRUE),
								MPFROM2SHORT (MIA_CHECKED, FALSE));
						Quality = SHORT1FROMMP (mp1) - ID_0;
						WinSendMsg (hwndMenu, MM_SETITEMATTR,
								MPFROM2SHORT (ID_0 + Quality, TRUE),
								MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));
						break;
					}
				}
				return (0L);
			}
			break;
		}
		case WM_SIZE: {
			if (! hwndTranscript) {
				hwndTranscript = WinCreateWindow (hwnd, WC_MLE, "", WS_VISIBLE
						| MLS_VSCROLL | MLS_HSCROLL | MLS_READONLY, 0, 0,
						SHORT1FROMMP (mp2), SHORT2FROMMP (mp2), hwnd, HWND_TOP, 0,
						NULL, NULL);
				if (hwndTranscript) {
					WinSendMsg (hwndTranscript, MLM_FORMAT, (MPARAM) MLFIE_NOTRANS,
							0L);
				}
			} else {
				WinSetWindowPos (hwndTranscript, HWND_TOP, 0, 0,
						SHORT1FROMMP (mp2), SHORT2FROMMP (mp2), SWP_MOVE | SWP_SIZE);
			}
			return (0L);
		}
		case WM_INIT_OPTIONS: {
			text_main ((int) mp1, (char * *) mp2);
			hwndMenu = WinWindowFromID (hwndFrame, FID_MENU);
			WinSendMsg (hwndMenu, MM_SETITEMATTR, MPFROM2SHORT (ID_ABORT, TRUE),
					MPFROM2SHORT (MIA_DISABLED, MIA_DISABLED));
			if (Options & DISKWRITE) {
				SHORT		sID;

				switch (OutputFormat) {
					case 'd': {
						sID = ID_DUMP;
						break;
					}
					case 't': {
						sID = ID_TARGA;
						break;
					}
					case 'r': {
						sID = ID_RAW;
						break;
					}
				}
				WinSendMsg (hwndMenu, MM_SETITEMATTR, MPFROM2SHORT (sID, TRUE),
						MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));
			}
			if (Options & DISPLAY) {
				WinSendMsg (hwndMenu, MM_SETITEMATTR,
						MPFROM2SHORT (ID_IMAGE, TRUE),
						MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));
			}
			if (Options & VERBOSE) {
				WinSendMsg (hwndMenu, MM_SETITEMATTR,
						MPFROM2SHORT (ID_STATISTICS, TRUE),
						MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));
			}
			WinSendMsg (hwndMenu, MM_SETITEMATTR,
					MPFROM2SHORT (ID_0 + Quality, TRUE),
					MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));
			return (0L);
		}
		case WM_PRINT: {
			AddStringToTranscript ((PSZ) mp1, (SHORT) mp2);
			return (0L);
		}
		case WM_STATS: {
			UpdateStats ((SHORT) mp1);
			return (0L);
		}
		case WM_PLOT: {
			SetPixel (SHORT1FROMMP (mp1), SHORT2FROMMP (mp1), (LONG) mp2);
			return (0L);
		}
		case WM_MENUS: {
			DisableMenus (FALSE);
			return (0L);
		}
	}/*endSwitch*/
	return (WinDefWindowProc (hwnd, ulMessage, mp1, mp2));
}/* end MainWndProc */


MRESULT EXPENTRY TraceWndProc (HWND hwnd, ULONG ulMessage, MPARAM mp1,
		MPARAM mp2) {
	switch (ulMessage) {
		case WM_INITDLG: {
			PSZ		pszThreshold [32];

			WinSendMsg (WinWindowFromID (hwnd, ID_IFILENAME), EM_SETTEXTLIMIT,
					(MPARAM) CCHMAXPATH, 0L);
			WinSetDlgItemText (hwnd, ID_IFILENAME, mp2);
			if (Options & DISKWRITE) {
				WinSendMsg (WinWindowFromID (hwnd, ID_OFILENAME), EM_SETTEXTLIMIT,
						(MPARAM) CCHMAXPATH, 0L);
				WinSetDlgItemText (hwnd, ID_OFILENAME, Output_File_Name);
				WinSetFocus (HWND_DESKTOP, WinWindowFromID (hwnd, ID_OFILENAME));
			} else {
				WinEnableWindow (WinWindowFromID (hwnd, ID_OFILENAME), FALSE);
				WinSetFocus (HWND_DESKTOP, WinWindowFromID (hwnd, ID_WIDTH));
			}
			WinSetDlgItemShort (hwnd, ID_WIDTH, Screen_Width, FALSE);
			WinSetDlgItemShort (hwnd, ID_HEIGHT, Screen_Height, FALSE);
			if (Options & ANTIALIAS) {
				WinSendMsg (WinWindowFromID (hwnd, ID_ANTIALIAS), BM_SETCHECK,
						(MPARAM) 1L, 0L);
			}
		/* why isn't there a WinSetDlgItemDouble */
			sprintf ((PSZ) pszThreshold, "%lf", Antialias_Threshold);
			WinSetDlgItemText (hwnd, ID_THRESHOLD, (PSZ) pszThreshold);
			return ((MPARAM) 1L);
		}
		case WM_COMMAND: {
			if (SHORT1FROMMP (mp2) == CMDSRC_PUSHBUTTON) {
				switch (SHORT1FROMMP (mp1)) {
					case ID_OK: {
						PSZ		pszThreshold [32];
						SHORT		sValue;

						if (Options & DISKWRITE) {
							WinQueryDlgItemText (hwnd, ID_OFILENAME, CCHMAXPATH,
									Output_File_Name);
						}
					/* why isn't there a WinQueryDlgItemInt */
						WinQueryDlgItemShort (hwnd, ID_WIDTH, & sValue, FALSE);
						Screen_Width = sValue;
						WinQueryDlgItemShort (hwnd, ID_HEIGHT, & sValue, FALSE);
						Screen_Height = sValue;
						if (WinSendMsg (WinWindowFromID (hwnd, ID_ANTIALIAS),
								BM_QUERYCHECK, 0L, 0L)) {
							Options |= ANTIALIAS;
						} else {
							Options &= ~ ANTIALIAS;
						}
						WinQueryDlgItemText (hwnd, ID_THRESHOLD, 32,
								(PSZ) pszThreshold);
						Antialias_Threshold = atof ((PSZ) pszThreshold);
						WinDismissDlg (hwnd, 1);
						break;
					}
					case ID_CANCEL: {
						WinDismissDlg (hwnd, 0);
						break;
					}
	  			}
				return (0L);
	  		}
			break;
		}
		case WM_CLOSE: {
			WinDismissDlg (hwnd, 0);
			return (0L);
		}
	}/*endSwitch*/
	return (WinDefDlgProc (hwnd, ulMessage, mp1, mp2));
}/* end TraceWndProc */
