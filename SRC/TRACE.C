/*****************************************************************************
*
*                                     trace.c
*
*   from DKBTrace (c) 1990  David Buck
*
*  This module contains the entry routine for the raytracer and the code to
*  parse the parameters on the command line.
*
*
* This software is freely distributable. The source and/or object code may be
* copied or uploaded to communications services so long as this notice remains
* at the top of each file.  If any changes are made to the program, you must
* clearly indicate in the documentation and in the programs startup message
* who it was who made the changes. The documentation should also describe what
* those changes were. This software may not be included in whole or in
* part into any commercial package without the express written consent of the
* author.  It may, however, be included in other public domain or freely
* distributed software so long as the proper credit for the software is given.
*
* This software is provided as is without any guarantees or warranty. Although
* the author has attempted to find and correct any bugs in the software, he
* is not responsible for any damage caused by the use of the software.  The
* author is under no obligation to provide service, corrections, or upgrades
* to this package.
*
* Despite all the legal stuff above, if you do find bugs, I would like to hear
* about them.  Also, if you have any comments or questions, you may contact me
* at the following address:
*
*     David Buck
*     22C Sonnet Cres.
*     Nepean Ontario
*     Canada, K2H 8W7
*
*  I can also be reached on the following bulleton boards:
*
*     OMX              (613) 731-3419
*     Mystic           (613) 596-4249  or  (613) 596-4772
*
*  Fidonet:   1:163/109.9
*  Internet:  dbuck@ccs.carleton.ca
*  The "You Can Call Me RAY" BBS    (708) 358-5611
*
*  IBM Port by Aaron A. Collins. Aaron may be reached on the following BBS'es:
*
*     The "You Can Call Me RAY" BBS (708) 358-5611
*     The Information Exchange BBS  (708) 945-5575
*
*****************************************************************************/

#include <ctype.h>
#include <time.h>
#include "frame.h"		/* common to ALL modules in this program */
#include "dkbproto.h"

#define MAX_FILE_NAMES 1
unsigned int Options;
int Quality;

FILE *bfp;

extern FRAME Frame;

/* this define is copied from bsedos.h */
#define CCHMAXPATH 260
char Output_File_Name [CCHMAXPATH];

#define MAX_LIBRARIES 10
char *Library_Paths [MAX_LIBRARIES];
int Library_Path_Index;

FILE_HANDLE *Output_File_Handle;
int File_Buffer_Size;
static int Number_Of_Files;
DBL VTemp;
DBL Antialias_Threshold;
int First_Line, Last_Line;
int Screen_Width, Screen_Height;

/* Stats kept by the ray tracer: */
long Number_Of_Pixels, Number_Of_Rays, Number_Of_Pixels_Supersampled;
long Ray_Sphere_Tests, Ray_Sphere_Tests_Succeeded;
long Ray_Plane_Tests, Ray_Plane_Tests_Succeeded;
long Ray_Triangle_Tests, Ray_Triangle_Tests_Succeeded;
long Ray_Quadric_Tests, Ray_Quadric_Tests_Succeeded;
long Ray_Quartic_Tests, Ray_Quartic_Tests_Succeeded;
long Bounding_Region_Tests, Bounding_Region_Tests_Succeeded;
long Calls_To_Noise, Calls_To_DNoise;
long Shadow_Ray_Tests, Shadow_Rays_Succeeded;
long Reflected_Rays_Traced, Refracted_Rays_Traced;
long Transmitted_Rays_Traced;

char OutputFormat;

extern time_t  Start_Time, Previous_Time;
extern int Stop_Flag;

void text_main (argc, argv)
  int argc;
  char **argv;
  {
  register int i;

	PRINT ("\n");
  PRINT ("DKB Ray Trace Version 2.12 Copyright (c) 1991 David K. Buck\n");
	PRINT ("\n");
  PRINT ("Written by: David K. Buck (dbuck@ccs.carleton.ca) (CIS: 70521, 1371)\n");
  PRINT ("     22C Sonnet Cr. Nepean, Ontario Canada, K2H 8W7\n");
  PRINT ("Conversion to IBM PC with TARGA output and other improvements by Aaron A. Collins\n");
  PRINT ("GIF format file reader by Steve A. Bennett\n");
  PRINT ("Noise and DNoise functions by Robert Skinner\n");
  PRINT ("Quartic (4th order) Shapes by Alexander Enzmann\n");
	PRINT ("Conversion to OS/2 2.0 PM by Michael Caldwell (mcaldwel@netcom.com).\n");
  PRINT ("This program is freely distributable.\n");
	PRINT ("\n");

  PRINT_OTHER_CREDITS

/* initialize defaults */
  init_options();

/* Read the default parameters from trace.def */
  get_defaults();

/* Parse the command line parameters */
  for (i = 1 ; i < argc ; i++ )
    if ((*argv[i] == '+') || (*argv[i] == '-'))
      parse_option(argv[i]);
    else
      parse_file_name(argv[i]);
}

void StartTrace (char * Input_File_Name) {

	init_vars ();

	PRINT ("Input File: %s\n", Input_File_Name);
	if (Options & DISKWRITE)
		PRINT ("Output File: %s\n", Output_File_Name);
   Print_Options ();

   Initialize_Tokenizer (Input_File_Name);
   PRINT ("Parsing...");
   Parse (&Frame);
   Terminate_Tokenizer();

/* Get things ready for ray tracing */
	Output_File_Handle = NULL;
   if (Options & DISKWRITE) {
      switch (OutputFormat) {
         case 'd':
            Output_File_Handle = Get_Dump_File_Handle();
				if (Output_File_Handle == NULL) {
					PRINT ("Error: cannot get output file handle.\n");
				}
            break;

         case 'r':
            Output_File_Handle = Get_Raw_File_Handle();
				if (Output_File_Handle == NULL) {
					PRINT ("Error: cannot get output file handle.\n");
				}
            break;

         case 't':
            Output_File_Handle = Get_Targa_File_Handle();
				if (Output_File_Handle == NULL) {
					PRINT ("Error: cannot get output file handle.\n");
				}
            break;

         default:
            PRINT ("Error: unrecognized output file format %c.\n", OutputFormat);
      }
		if (Output_File_Handle) {
      	if (Open_File (Output_File_Handle, Output_File_Name,
               	&Frame.Screen_Width, &Frame.Screen_Height, File_Buffer_Size,
               	WRITE_MODE) != 1) {
			/* must do a Close_File to free dynamically allocated memory */
		      Close_File (Output_File_Handle);
				Output_File_Handle = NULL;
         	PRINT ("Error: cannot open output file.\n");
		   }
	   }
   }

	initialize_renderer();
	pq_init();
	Initialize_Noise();

/* Ok, go for it - trace the picture */
	PRINT ("Rendering...\n");
	Start_Tracing ();
	if (Stop_Flag) {
		PRINT ("Aborted.\n");
	} else {
		PRINT ("Done.\n");
	}
	PRINT ("\n");

	if (Output_File_Handle)
		Close_File (Output_File_Handle);
}

void usage ()
    {
    PRINT ("\n");
    PRINT ("Usage:\n");
    PRINT ("     tracepm [{+|-}Option]... [option file]\n");
    PRINT ("\n");
    PRINT ("Options:\n");
    PRINT ("     d  = display image\n");
    PRINT ("     v  = display statistics\n");
    PRINT ("     fx = write output file in format x\n");
    PRINT ("          ft - Uncompressed Targa-24  fd - DKB/QRT Dump  fr - 3 Raw Files\n");
    PRINT ("     a  = perform antialiasing\n");
    PRINT ("     qx = image quality 0=rough, 9=full\n");
    PRINT ("     lxxx = library path prefix\n");
    PRINT ("     wxxx = width of the screen\n");
    PRINT ("     hxxx = height of the screen\n");
    PRINT ("     bxxx = Use xxx kilobytes for output file buffer space\n");
    PRINT ("\n");
    }

init_options () {
	Options = 0;
	Quality = 9;
	Frame.Screen_Width = 160;
	Screen_Width = Frame.Screen_Width;
	Frame.Screen_Height = 120;
	Screen_Height = Frame.Screen_Height;
	Antialias_Threshold = 0.3;
	File_Buffer_Size = 0;
	Library_Paths [0] = NULL;
	Library_Path_Index = 0;
	return;
}

void init_vars() {
	Frame.Screen_Width = Screen_Width;
	Frame.Screen_Height = Screen_Height;
	Frame.Antialias_Threshold = Antialias_Threshold;
	First_Line = 0;
	Last_Line = Frame.Screen_Height;
	Number_Of_Pixels = 0L;
	Number_Of_Rays = 0L;
	Number_Of_Pixels_Supersampled = 0L;
	Ray_Sphere_Tests = 0L;
	Ray_Sphere_Tests_Succeeded = 0L;
	Ray_Plane_Tests = 0L;
	Ray_Plane_Tests_Succeeded = 0L;
	Ray_Triangle_Tests = 0L;
	Ray_Triangle_Tests_Succeeded = 0L;
	Ray_Quadric_Tests = 0L;
	Ray_Quadric_Tests_Succeeded = 0L;
	Ray_Quartic_Tests = 0L;
	Ray_Quartic_Tests_Succeeded = 0L;
	Bounding_Region_Tests = 0L;
	Bounding_Region_Tests_Succeeded = 0L;
	Calls_To_Noise = 0L;
	Calls_To_DNoise = 0L;
	Shadow_Ray_Tests = 0L;
	Shadow_Rays_Succeeded = 0L;
	Reflected_Rays_Traced = 0L;
	Refracted_Rays_Traced = 0L;
	Transmitted_Rays_Traced = 0L;
	return;
}

/* Read the default parameters from trace.def */
void get_defaults()
  {
  FILE *defaults_file;
  char Option_String[256], *Option_String_Ptr;

  if ((defaults_file = fopen("trace.def", "r")) != NULL) {
     while (fgets(Option_String, 256, defaults_file) != NULL)
        read_options(Option_String);
     fclose (defaults_file);
     }

  if ((Option_String_Ptr = getenv("DKBOPT")) != NULL)
     read_options(Option_String_Ptr);
  }

void read_options (Option_Line)
  char *Option_Line;
  {
  register int c, String_Index, Option_Started;
  short Option_Line_Index = 0;
  char Option_String[80];

  String_Index = 0;
  Option_Started = FALSE;
  while ((c = Option_Line[Option_Line_Index++]) != '\0')
    {
    if (Option_Started)
      if (isspace(c))
        {
        Option_String[String_Index] = '\0';
        parse_option (Option_String);
        Option_Started = FALSE;
	String_Index = 0;
        }
     else
       Option_String[String_Index++] = (char) c;

    else /* Option_Started */
      if ((c == (int) '-') || (c == (int) '+'))
        {
        String_Index = 0;
        Option_String[String_Index++] = (char) c;
        Option_Started = TRUE;
        }
      else
        if (!isspace(c))
          {
          PRINT ("Error: bad option format, char: (%c) val: %d.\n",
					(char) c, c);
			usage ();
			return;
          }
    }

  if (Option_Started)
    {
    Option_String[String_Index] = '\0';
    parse_option (Option_String);
    }
  }

/* parse the command line parameters */
void parse_option (Option_String)
  char *Option_String;
  {
  register int Add_Option;
  unsigned int Option_Number;
  DBL threshold;

  if (*(Option_String++) == '-')
    Add_Option = FALSE;
  else
    Add_Option = TRUE;

  switch (*Option_String)
    {
    case 'B':
    case 'b':  sscanf (&Option_String[1], "%d", &File_Buffer_Size);
               File_Buffer_Size *= 1024;
               if (File_Buffer_Size < BUFSIZ)
                  File_Buffer_Size = BUFSIZ;
               Option_Number = 0;
               break;

    case 'D':
    case 'd':  Option_Number = DISPLAY;
               break;

    case 'V':
    case 'v':  Option_Number = VERBOSE;
               break;

    case 'W':
    case 'w':  sscanf (&Option_String[1], "%d", &Frame.Screen_Width);
					Screen_Width = Frame.Screen_Width;
					Option_Number = 0;
               break;

    case 'H':
    case 'h':  sscanf (&Option_String[1], "%d", &Frame.Screen_Height);
					Screen_Height = Frame.Screen_Height;
	 				Option_Number = 0;
               break;

    case 'F':
    case 'f':  Option_Number = DISKWRITE;
               if (isupper(Option_String[1]))
                  OutputFormat = (char)tolower(Option_String[1]);
               else
                  OutputFormat = Option_String[1];

               if (OutputFormat == '\0')
                  OutputFormat = DEFAULT_OUTPUT_FORMAT;
               break;

    case 'A':
    case 'a':  Option_Number = ANTIALIAS;
               if (sscanf (&Option_String[1], DBL_FORMAT_STRING, &threshold) != EOF)
                   Antialias_Threshold = threshold;
               break;

    case 'L':
    case 'l':
		if (Library_Path_Index < MAX_LIBRARIES) {
	   	Library_Paths [Library_Path_Index] = malloc (strlen(Option_String));
	      if (Library_Paths [Library_Path_Index]) {
         	strcpy (Library_Paths [Library_Path_Index], &Option_String[1]);
         	Library_Path_Index++;
			} else {
			   PRINT ("Error: cannot allocate memory for library pathname %s.\n",
 						Option_String);
			}
		} else {
         PRINT ("Error: more than %d library directories specified.\n",
					MAX_LIBRARIES);
      }

	   Option_Number = 0;
      break;

    case 'Q':
    case 'q':  sscanf (&Option_String[1], "%d", &Quality);
	       Option_Number = 0;
               break;

               /* Turn on debugging print statements. */
    case 'Z':
    case 'z':  Option_Number = DEBUGGING;
               break;

    default:   PRINT ("Error: invalid option %s.\n", --Option_String);
			usage ();
			return;
    }

  if (Option_Number != 0)
      if (Add_Option)
           Options |= Option_Number;
      else Options &= ~Option_Number;
  }

void Print_Options()
   {
	char szOptions [128];
	char * pszOptions;
   int i;

	pszOptions = szOptions;

   pszOptions += sprintf (pszOptions, "Options in effect: ");

   if (Options & DISPLAY)
      pszOptions += sprintf (pszOptions, "+d ");
   else
      pszOptions += sprintf (pszOptions, "-d ");

   if (Options & VERBOSE)
      pszOptions += sprintf (pszOptions, "+v ");
   else
      pszOptions += sprintf (pszOptions, "-v ");

   if (Options & DISKWRITE)
      pszOptions += sprintf (pszOptions, "+f%c ", OutputFormat);
   else
      pszOptions += sprintf (pszOptions, "-f ");

   if (Options & ANTIALIAS)
      pszOptions += sprintf (pszOptions, "+a%f ", Antialias_Threshold);
   else
      pszOptions += sprintf (pszOptions, "-a ");

   if (Options & DEBUGGING)
      pszOptions += sprintf (pszOptions, "+z ");

   if (File_Buffer_Size != 0)
      pszOptions += sprintf (pszOptions, "-b%d ", File_Buffer_Size / 1024);

   pszOptions += sprintf (pszOptions, "-q%d -w%d -h%d", Quality,
			Frame.Screen_Width, Frame.Screen_Height);

   for (i = 0 ; i < Library_Path_Index ; i++)
      pszOptions += sprintf (pszOptions, " -l%s ", Library_Paths[i]);

	PRINT (szOptions);
   PRINT ("\n");
   }

void parse_file_name (File_Name)
  char *File_Name;
  {
  FILE *defaults_file;
  char Option_String[256];

  if (++Number_Of_Files > MAX_FILE_NAMES)
    {
    PRINT ("Only %d option file(s) are allowed in a command line.\n",
			MAX_FILE_NAMES);
	return;
    }

  if ((defaults_file = fopen(File_Name, "r")) != NULL) {
     while (fgets(Option_String, 256, defaults_file) != NULL)
        read_options(Option_String);
     fclose (defaults_file);
     }
  }

#ifdef TEXTMODE
void print_line_stats (register int y) {
	time_t      Current_Time;
	time_t      Time;
	time_t      Temp;

	Current_Time = time (NULL);
/* keep the number of "\n"s down to 25 */
	PRINT ("\n");
	PRINT ("Statistics for line %04d\n", y);
	PRINT ("------------------------\n\n");
	PRINT ("# Rays: %10ld   # Pixels: %10ld   # Pixels supersampled: %10ld\n\n", Number_Of_Rays, Number_Of_Pixels, Number_Of_Pixels_Supersampled);
	PRINT ("Intersection Tests:   Type          Tests    Succeeded\n");
	PRINT ("                      ----     ----------   ----------\n");
	PRINT ("                      Sphere   %10ld   %10ld\n", Ray_Sphere_Tests, Ray_Sphere_Tests_Succeeded);
	PRINT ("                      Plane    %10ld   %10ld\n", Ray_Plane_Tests, Ray_Plane_Tests_Succeeded);
	PRINT ("                      Triangle %10ld   %10ld\n", Ray_Triangle_Tests, Ray_Triangle_Tests_Succeeded);
	PRINT ("                      Quadric  %10ld   %10ld\n", Ray_Quadric_Tests, Ray_Quadric_Tests_Succeeded);
	PRINT ("                      Quartic  %10ld   %10ld\n", Ray_Quartic_Tests, Ray_Quartic_Tests_Succeeded);
	PRINT ("                      Bounds   %10ld   %10ld\n\n", Bounding_Region_Tests, Bounding_Region_Tests_Succeeded);
	PRINT ("   Calls to Noise:         %10ld\n", Calls_To_Noise);
	PRINT ("   Calls to DNoise:        %10ld\n", Calls_To_DNoise);
	PRINT ("   Shadow Ray Tests:       %10ld\n", Shadow_Ray_Tests);
	PRINT ("   Blocking Objects Found: %10ld\n", Shadow_Rays_Succeeded);
	PRINT ("   Reflected Rays:         %10ld\n", Reflected_Rays_Traced);
	PRINT ("   Refracted Rays:         %10ld\n", Refracted_Rays_Traced);
	PRINT ("   Transmitted Rays:       %10ld\n", Transmitted_Rays_Traced);
	Time = Current_Time - Previous_Time;
   PRINT ("   Time for this line:     %02dd", Temp = Time / 86400);
	PRINT (" %02dh", Temp = (Time -= Temp * 86400) / 3600);
	PRINT (" %02dm", Temp = (Time -= Temp * 3600) / 60);
	PRINT (" %02ds\n", Time - Temp * 60);
	Time = Current_Time - Start_Time;
   PRINT ("   Total elapsed time:     %02dd", Temp = Time / 86400);
	PRINT (" %02dh", Temp = (Time -= Temp * 86400) / 3600);
	PRINT (" %02dm", Temp = (Time -= Temp * 3600) / 60);
	PRINT (" %02ds\n", Time - Temp * 60);
	Time = (Current_Time - Start_Time) / (y + 1);
   PRINT ("   Average time per line:  %02dd", Temp = Time / 86400);
	PRINT (" %02dh", Temp = (Time -= Temp * 86400) / 3600);
	PRINT (" %02dm", Temp = (Time -= Temp * 3600) / 60);
	PRINT (" %02ds", Time - Temp * 60);
   Previous_Time = time (NULL);
	PRINT ("                    I/O %ds\n", Previous_Time - Current_Time);
}
#endif

/* Find a file in the search path. */
FILE *Locate_File (filename, mode)
   char *filename, *mode;
   {
   FILE *f;
   int i;
   char pathname[CCHMAXPATH];

   /* Check the current directory first. */
   if ((f = fopen (filename, mode)) != NULL)
      return (f);

   for (i = 0 ; i < Library_Path_Index ; i++) {
      strcpy (pathname, Library_Paths[i]);
      if (FILENAME_SEPARATOR != NULL)
         strcat (pathname, FILENAME_SEPARATOR);
      strcat (pathname, filename);
      if ((f = fopen (pathname, mode)) != NULL)
         return (f);
      }

   return (NULL);
   }
