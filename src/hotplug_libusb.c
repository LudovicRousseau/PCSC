/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	Title  : hotplug_linux.c
	Package: pcsc lite
	Author : David Corcoran, Ludovic Rousseau
	Date   : 02/28/01, last update 4/6/2003
	License: Copyright (C) 2001,2003 David Corcoran, Ludovic Rousseau
			<corcoran@linuxnet.com>
	Purpose: This provides a search API for hot pluggble devices.
	Credits: The USB code was based partly on Johannes Erdfelt
		libusb code found at libusb.sourceforge.org

$Id$

********************************************************************/

#include <string.h>
#include <sys/types.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <usb.h>

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "readerfactory.h"
#include "winscard_msg.h"
#include "debuglog.h"
#include "sys_generic.h"
#include "parser.h"

// PCSCLITE_HP_DROPDIR is defined using ./configure --enable-usbdropdir=foobar
#define PCSCLITE_MANUKEY_NAME                   "ifdVendorID"
#define PCSCLITE_PRODKEY_NAME                   "ifdProductID"
#define PCSCLITE_NAMEKEY_NAME                   "ifdFriendlyName"
#define PCSCLITE_LIBRKEY_NAME                   "CFBundleExecutable"
#define PCSCLITE_HP_MAX_IDENTICAL_READERS	 	16
#define PCSCLITE_HP_MAX_SIMUL_READERS           04
#define PCSCLITE_HP_MAX_DRIVERS					20
#define BUS_DEVICE_STRSIZE						256

extern int LTPBundleFindValueWithKey(char *, char *, char *, int);
extern PCSCLITE_MUTEX usbNotifierMutex;

LONG HPAddHotPluggable(int, unsigned long);
LONG HPRemoveHotPluggable(int, unsigned long);
LONG HPReadBundleValues();

static PCSCLITE_THREAD_T usbNotifyThread;
static int bundleSize = 0;

/*
 * A list to keep track of 20 simultaneous readers
 */

static struct _bundleTracker
{
	long  manuID;
	long  productID;

	struct _deviceNumber {
		short plugged;
		char status;
		char bus_device[BUS_DEVICE_STRSIZE];
	} deviceNumber[PCSCLITE_HP_MAX_SIMUL_READERS];

	char *bundleName;
	char *libraryPath;
	char *readerName;
}
bundleTracker[PCSCLITE_HP_MAX_DRIVERS];

// static LONG hpManu_id, hpProd_id;
// static int bundleArraySize;

LONG HPReadBundleValues()
{

	LONG rv;
	DIR *hpDir;
	struct dirent *currFP = 0;
	char fullPath[FILENAME_MAX];
	char fullLibPath[FILENAME_MAX];
	char keyValue[TOKEN_MAX_VALUE_SIZE];
	int listCount = 0;

	hpDir = opendir(PCSCLITE_HP_DROPDIR);

	if (hpDir == NULL)
	{
		DebugLogA("Cannot open PC/SC drivers directory: " PCSCLITE_HP_DROPDIR);
		DebugLogA("Disabling USB support for pcscd.");
		return -1;
	}

	while ((currFP = readdir(hpDir)) != 0)
	{
		if (strstr(currFP->d_name, ".bundle") != 0)
		{
			int alias = 0;

			/*
			 * The bundle exists - let's form a full path name and get the
			 * vendor and product ID's for this particular bundle
			 */
			snprintf(fullPath, FILENAME_MAX, "%s%s%s", PCSCLITE_HP_DROPDIR,
				currFP->d_name, "/Contents/Info.plist");
			fullPath[FILENAME_MAX - 1] = '\0';

			// while we find a nth ifdVendorID in Info.plist
			while (LTPBundleFindValueWithKey(fullPath, PCSCLITE_MANUKEY_NAME,
				keyValue, alias) == 0)
			{
				bundleTracker[listCount].bundleName = strdup(currFP->d_name);

				// Get ifdVendorID
				rv = LTPBundleFindValueWithKey(fullPath, PCSCLITE_MANUKEY_NAME,
					keyValue, alias);
				if (rv == 0)
					bundleTracker[listCount].manuID = strtol(keyValue, 0, 16);

				// get ifdProductID
				rv = LTPBundleFindValueWithKey(fullPath, PCSCLITE_PRODKEY_NAME,
					keyValue, alias);
				if (rv == 0)
					bundleTracker[listCount].productID =
						strtol(keyValue, 0, 16);

				// get ifdFriendlyName
				rv = LTPBundleFindValueWithKey(fullPath, PCSCLITE_NAMEKEY_NAME,
					keyValue, alias);
				if (rv == 0)
					bundleTracker[listCount].readerName = strdup(keyValue);

				// get CFBundleExecutable
				rv = LTPBundleFindValueWithKey(fullPath, PCSCLITE_LIBRKEY_NAME,
					keyValue, 0);
				if (rv == 0)
				{
					snprintf(fullLibPath, FILENAME_MAX, "%s%s%s%s", PCSCLITE_HP_DROPDIR,
						currFP->d_name, "/Contents/Linux/", keyValue);
					fullLibPath[FILENAME_MAX - 1] = '\0';
					bundleTracker[listCount].libraryPath = strdup(fullLibPath);
				}

				listCount++;
				alias++;
			}
		}
	}

	bundleSize = listCount;

	if (bundleSize == 0)
	{
		DebugLogA("No bundle files in pcsc drivers directory: " PCSCLITE_HP_DROPDIR);
		DebugLogA("Disabling USB support for pcscd");
	}

	closedir(hpDir);
	return 0;
}

void HPEstablishUSBNotifications()
{

	int i, j, usbDeviceStatus;
#ifdef MSC_TARGET_BSD
	int fd;
	char filename[BUS_DEVICE_STRSIZE];
#endif
	struct usb_bus *bus;
	struct usb_device *dev;
	char bus_device[BUS_DEVICE_STRSIZE];

	usbDeviceStatus = 0;

	usb_init();
	while (1)
	{
		usb_find_busses();
		usb_find_devices();
		for (i = 0; i < bundleSize; i++)
		{
			usbDeviceStatus     = 0;

			for (j=0; j < PCSCLITE_HP_MAX_SIMUL_READERS; j++)
			{
				bundleTracker[i].deviceNumber[j].status = 0; /* clear rollcall */
			}

			for (bus = usb_get_busses(); bus; bus = bus->next)
			{
				for (dev = bus->devices; dev; dev = dev->next)
				{
					if (dev->descriptor.idVendor == bundleTracker[i].manuID &&
						dev->descriptor.idProduct == bundleTracker[i].productID &&
						dev->descriptor.idVendor !=0 &&
						dev->descriptor.idProduct != 0)
					{
						/* A known device has been found */
						snprintf(bus_device, BUS_DEVICE_STRSIZE, "%s:%s",
							bus->dirname, dev->filename);
						bus_device[BUS_DEVICE_STRSIZE - 1] = '\0';
						DebugLogB("Found matching USB device %s", bus_device);
						for (j=0; j < PCSCLITE_HP_MAX_SIMUL_READERS; j++)
						{
							if (strncmp(bundleTracker[i].deviceNumber[j].bus_device,
								bus_device, BUS_DEVICE_STRSIZE) == 0 &&
								bundleTracker[i].deviceNumber[j].plugged)
							{
								bundleTracker[i].deviceNumber[j].status = 1; /* i'm here */
								DebugLogB("Refresh USB device %s status", bus_device);
								break;
							}
						}

						if (j == PCSCLITE_HP_MAX_SIMUL_READERS)
						{
							usbDeviceStatus = 1;
						}
					}

				} /* End of for..loop */


			} /* End of for..loop */


			if (usbDeviceStatus == 1)
			{
				SYS_MutexLock(&usbNotifierMutex);

				for (j=0; j < PCSCLITE_HP_MAX_SIMUL_READERS; j++)
				{
					if (!bundleTracker[i].deviceNumber[j].plugged)
						break;
				}

				if (j == PCSCLITE_HP_MAX_SIMUL_READERS)
				{
					DebugLogA("Too many identical readers plugged in");
				}
				else
				{
					DebugLogB("Adding USB device %s", bus_device);
					HPAddHotPluggable(i, j+1);
					strncpy(bundleTracker[i].deviceNumber[j].bus_device,
						bus_device, BUS_DEVICE_STRSIZE);
					bundleTracker[i].deviceNumber[j].bus_device[BUS_DEVICE_STRSIZE - 1] = '\0';
					bundleTracker[i].deviceNumber[j].plugged = 1;
				}

				SYS_MutexUnLock(&usbNotifierMutex);

			}
			else
				if (usbDeviceStatus == 0)
				{

					for (j=0; j < PCSCLITE_HP_MAX_SIMUL_READERS; j++)
					{
#ifdef MSC_TARGET_BSD
						/*	BSD workaround:
						 *	ugenopen() in sys/dev/usb/ugen.c returns EBUSY
						 *	when the character device file is already open.
						 *	Because of this, open usb devices will not be
						 *	detected by usb_find_devices(), so we have to
						 *	check for this explicitly.
						 */
						if (bundleTracker[i].deviceNumber[j].plugged)
						{
							sscanf(bundleTracker[i].deviceNumber[j].bus_device,
								"%*[^:]%*[:]%s", filename);
							fd = open(filename, O_RDONLY);
							if (fd == -1)
							{
								if (errno == EBUSY)
								{
									/* The device is present */
									DebugLogB("BSD: EBUSY on %s", filename);
									bundleTracker[i].deviceNumber[j].status = 1;
								}
								else
								{
									DebugLogC("BSD: %s error: %s", filename,
										strerror(errno));
								}
							}
							else
							{
								DebugLogB("BSD: OK %s", filename);
								bundleTracker[i].deviceNumber[j].status = 1;
								close(fd);
							}
						}
#endif
						if (bundleTracker[i].deviceNumber[j].plugged &&
							bundleTracker[i].deviceNumber[j].status == 0)
						{
							DebugLogB("Removing USB device %s", bus_device);
							SYS_MutexLock(&usbNotifierMutex);
							HPRemoveHotPluggable(i, j+1);
							bundleTracker[i].deviceNumber[j].plugged = 0;
							bundleTracker[i].deviceNumber[j].bus_device[0] = '\0';
							SYS_MutexUnLock(&usbNotifierMutex);

						}
					}
				}
				else
				{
					/*
					 * Do nothing - no USB devices found
					 */
				}


		}	/* End of for..loop */

		SYS_Sleep(1);

	}	/* End of while loop */

}

LONG HPSearchHotPluggables()
{
	int i, j;

	for (i = 0; i < PCSCLITE_HP_MAX_DRIVERS; i++)
	{
		bundleTracker[i].productID  = 0;
		bundleTracker[i].manuID     = 0;

		for (j=0; j < PCSCLITE_HP_MAX_SIMUL_READERS; j++)
		{
			 bundleTracker[i].deviceNumber[j].plugged = 0;
			 bundleTracker[i].deviceNumber[j].status = 0;
			 bundleTracker[i].deviceNumber[j].bus_device[0] = '\0';
		}
	}

	HPReadBundleValues();

	SYS_ThreadCreate(&usbNotifyThread, NULL,
		(LPVOID) HPEstablishUSBNotifications, 0);

	return 0;
}

LONG HPAddHotPluggable(int i, unsigned long usbAddr)
{
	RFAddReader(bundleTracker[i].readerName, 0x200000 + usbAddr,
		bundleTracker[i].libraryPath);

	return 1;
}	/* End of function */

LONG HPRemoveHotPluggable(int i, unsigned long usbAddr)
{
	RFRemoveReader(bundleTracker[i].readerName, 0x200000 + usbAddr);

	return 1;
}	/* End of function */

