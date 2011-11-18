==============
PCSC spy HOWTO
==============

To be able to spy the PC/SC layer, the application flow must be modified
so that all PC/SC calls are redirected. Two options are available:
- the application is linked with libpcsclite.so.1
- the application loads the libpcsclite.so.1 library using dlopen(3)

Applications linked with libpcsclite.so.1
=========================================

We will use the standard LD_PRELOAD loader option to load our spying
library.

Example:

LD_PRELOAD=/usr/lib/libpcscspy.so opensc-tool -a


Application loading libpcsclite.so.1
====================================

This is the case for the PC/SC wrappers like pyscard (for Python) and
pcsc-perl (for Perl). The LD_PRELOAD mechanism can't be used. Instead we
replace the libpcsclite.so.1 library by the spying one.

Use install_spy.sh and uninstall_spy.sh to install and uninstall the
spying library.

Using the spying library without pcsc-spy.py is not a problem but has
side effects:
- a line "libpcsclite_nospy.so.1: cannot open shared object file: No
  such file or directory" will be displayed
- some CPU time will be lost because of the PC/SC calls redirection


Starting the spy tool
=====================

pcsc-spy.py


If a command argument is passed we use it instead of the default
~/pcsc-spy FIFO file. It is then possible to record an execution log and
use pcsc-spy.py multiple times on the same log.

To create the log file just do:

mkfifo ~/pcsc-spy
cat ~/pcsc-spy > logfile

and run your PC/SC application


Ludovic Rousseau
$Id$
