DKB 2.12 ported to OS/2 2.0 PM by Michael Caldwell (mcaldwel@netcom.com).

Tracepm is an integrated graphical interface to the DKB ray tracing
engine.  Unzip the dkbos2.zoo file into a subdirectory called data
and you can begin to trace the examples.  I have only included the
source for this version of dkb and none of the documentation.

Notes:

Tracepm starts out with it's internal defaults, it will then look for
any options in a file called trace.def, it will then look for any
options specified by the environment variable DKBOPT, and lastly it
will parse the command line for any options.  You can then change the
options with the menus and the dialog box that actually starts the
trace.

OS/2 bitmaps cannot be greater than 64K.  If your display driver
resolution and the size you specify for the trace image is greater than
64K then you get a message in the transcript window and the image window
is not used.  Some combinations of values caused the PMGRE.DLL to trap D
and hang OS/2 2.0, so I decided to put this check in.  However, this does
not effect the file output, you just can't see it on the screen while it's
tracing.

I replaced all the C run time exit calls with returns, so you'll get
the error displayed in the transcript window and tracepm won't exit. 
This will probably cause even more errors since dkb doesn't expect
functions to return if an error occured.

I added an Abort Trace menu option.

The trace thread now posts messages to thread 1 which makes it more
yielding.

If you have any problems, let me know.

Have fun!

Michael Caldwell (mcaldwel@netcom.com)
