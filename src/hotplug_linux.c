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

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "readerfactory.h"
#include "winscard_msg.h"
#include "debuglog.h"
#include "sys_generic.h"

// PCSCLITE_HP_DROPDIR is defined using ./configure --enable-usbdropdir=foobar
#define PCSCLITE_USB_PATH                       "/proc/bus/usb"
#define PCSCLITE_MANUKEY_NAME                   "ifdVendorID"
#define PCSCLITE_PRODKEY_NAME                   "ifdProductID"
#define PCSCLITE_NAMEKEY_NAME                   "ifdFriendlyName"
#define PCSCLITE_LIBRKEY_NAME                   "CFBundleExecutable"
#define PCSCLITE_HP_MAX_IDENTICAL_READERS	 	16
#define PCSCLITE_HP_MAX_SIMUL_READERS           04
#define PCSCLITE_HP_MAX_DRIVERS					20

extern int LCFBundleFindValueWithKey(char *, char *, char *);
extern PCSCLITE_MUTEX usbNotifierMutex;

struct usb_device_descriptor
{
	u_int8_t bLength;
	u_int8_t bDescriptorType;
	u_int16_t bcdUSB;
	u_int8_t bDeviceClass;
	u_int8_t bDeviceSubClass;
	u_int8_t bDeviceProtocol;
	u_int8_t bMaxPacketSize0;
	u_int16_t idVendor;
	u_int16_t idProduct;
	u_int16_t bcdDevice;
	u_int8_t iManufacturer;
	u_int8_t iProduct;
	u_int8_t iSerialNumber;
	u_int8_t bNumConfigurations;
}
__attribute__ ((packed));

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
		int  id;
		char status;
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
	char fullPath[200];
	char fullLibPath[250];
	char keyValue[200];
	int listCount = 0;

	hpDir = NULL;
	rv = 0;

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
			bundleTracker[listCount].bundleName = strdup(currFP->d_name);

			/*
			 * The bundle exists - let's form a full path name and get the
			 * vendor and product ID's for this particular bundle
			 */

			sprintf(fullPath, "%s%s%s", PCSCLITE_HP_DROPDIR,
				currFP->d_name, "/Contents/Info.plist");

			rv = LCFBundleFindValueWithKey(fullPath, PCSCLITE_MANUKEY_NAME,
				keyValue);
			if (rv == 0)
			{
				bundleTracker[listCount].manuID = strtol(keyValue, 0, 16);
			}

			rv = LCFBundleFindValueWithKey(fullPath, PCSCLITE_PRODKEY_NAME,
				keyValue);
			if (rv == 0)
			{
				bundleTracker[listCount].productID =
					strtol(keyValue, 0, 16);
			}

			rv = LCFBundleFindValueWithKey(fullPath, PCSCLITE_NAMEKEY_NAME,
				keyValue);
			if (rv == 0)
			{
				bundleTracker[listCount].readerName = strdup(keyValue);
			}

			rv = LCFBundleFindValueWithKey(fullPath, PCSCLITE_LIBRKEY_NAME,
				keyValue);
			if (rv == 0)
			{
				sprintf(fullLibPath, "%s%s%s%s", PCSCLITE_HP_DROPDIR,
					currFP->d_name, "/Contents/Linux/", keyValue);
				bundleTracker[listCount].libraryPath = strdup(fullLibPath);
			}

			listCount += 1;
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
	DIR *dir, *dirB;
	struct dirent *entry, *entryB;
	int deviceNumber;
	int suspectDeviceNumber;
	char dirpath[150];
	char filename[150];
	int fd, ret;
	struct usb_device_descriptor usbDescriptor;

	usbDeviceStatus = 0;
	suspectDeviceNumber = 0;

	while (1)
	{
		for (i = 0; i < bundleSize; i++)
		{
			usbDeviceStatus     = 0;
			suspectDeviceNumber = 0;

			for (j=0; j < PCSCLITE_HP_MAX_SIMUL_READERS; j++)
			{
				bundleTracker[i].deviceNumber[j].status = 0; /* clear rollcall */
			}

			dir = NULL;
			dir = opendir(PCSCLITE_USB_PATH);
			if (dir == NULL)
			{
				DebugLogA("Cannot open USB path directory: " PCSCLITE_USB_PATH);
				return;
			}

			entry = NULL;
			while ((entry = readdir(dir)) != 0)
			{

				/*
				 * Skip anything starting with a
				 */
				if (entry->d_name[0] == '.')
					continue;
				if (!strchr("0123456789",
						entry->d_name[strlen(entry->d_name) - 1]))
				{
					continue;
				}

				sprintf(dirpath, "%s/%s", PCSCLITE_USB_PATH, entry->d_name);

				dirB = opendir(dirpath);

				if (dirB == NULL)
				{
					DebugLogB("USB path seems to have disappeared %s",
						dirpath);
					closedir(dir);
					return;
				}

				while ((entryB = readdir(dirB)) != NULL)
				{
					/*
					 * Skip anything starting with a
					 */
					if (entryB->d_name[0] == '.')
						continue;

					/* Get the device number so we can distinguish
					   multiple readers */
					sprintf(filename, "%s/%s", dirpath, entryB->d_name);
					sscanf(entryB->d_name, "%d", &deviceNumber);

					fd = open(filename, O_RDONLY);
					if (fd < 0)
						continue;

					ret = read(fd, (void *) &usbDescriptor,
						sizeof(usbDescriptor));

					close(fd);

					if (ret < 0)
						continue;

					/*
					 * Device is found and we don't know about it
					 */

					if (usbDescriptor.idVendor == bundleTracker[i].manuID &&
						usbDescriptor.idProduct == bundleTracker[i].productID &&
						usbDescriptor.idVendor !=0 &&
						usbDescriptor.idProduct != 0)
					{
						for (j=0; j < PCSCLITE_HP_MAX_SIMUL_READERS; j++)
						{
							if (bundleTracker[i].deviceNumber[j].id == deviceNumber &&
								bundleTracker[i].deviceNumber[j].id != 0)
							{
								bundleTracker[i].deviceNumber[j].status = 1; /* i'm here */
								break;
							}
						}

						if (j == PCSCLITE_HP_MAX_SIMUL_READERS)
						{
							usbDeviceStatus = 1;
							suspectDeviceNumber = deviceNumber;
						}
					}

				} /* End of while */

				closedir(dirB);

			} /* End of while */


			if (usbDeviceStatus == 1)
			{
				SYS_MutexLock(&usbNotifierMutex);

				for (j=0; j < PCSCLITE_HP_MAX_SIMUL_READERS; j++)
				{
					if (bundleTracker[i].deviceNumber[j].id == 0)
						break;
				}

				if (j == PCSCLITE_HP_MAX_SIMUL_READERS)
				{
					DebugLogA("Too many identical readers plugged in");
				}
				else
				{
					HPAddHotPluggable(i, j+1);
					bundleTracker[i].deviceNumber[j].id = suspectDeviceNumber;
				}

				SYS_MutexUnLock(&usbNotifierMutex);

			}
			else
				if (usbDeviceStatus == 0)
				{

					for (j=0; j < PCSCLITE_HP_MAX_SIMUL_READERS; j++)
					{
						if (bundleTracker[i].deviceNumber[j].id != 0 &&
							bundleTracker[i].deviceNumber[j].status == 0)
						{
							SYS_MutexLock(&usbNotifierMutex);
							HPRemoveHotPluggable(i, j+1);
							bundleTracker[i].deviceNumber[j].id = 0;
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

			if (dir)
				closedir(dir);

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
			 bundleTracker[i].deviceNumber[j].id = 0;
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

