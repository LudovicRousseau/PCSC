#!/bin/sh

set -e

# do not use configfile.c since it is a lex file from configfile.l
pcscd_files="src/atrhandler.c src/auth.c src/debuglog.c src/dyn_unix.c src/eventhandler.c src/hotplug_generic.c src/hotplug_libudev.c src/hotplug_libusb.c src/ifdwrapper.c src/pcscdaemon.c src/prothandler.c src/readerfactory.c src/simclist.c src/sys_unix.c src/utils.c src/winscard.c src/winscard_msg.c src/winscard_msg_srv.c src/winscard_svc.c"
lib_files="src/debug.c src/winscard_clnt.c src/sys_unix.c src/utils.c src/winscard_msg.c"

inc="-Ibuilddir -Isrc -Isrc/PCSC -I/usr/include/hal -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include -I/usr/include/polkit-1 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include"
opt="--analyze"

clang $inc -DPCSCD $opt $pcscd_files
clang $inc $opt $lib_files
