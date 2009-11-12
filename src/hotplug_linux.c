/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2003
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2002-2009
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * The USB code was based partly on Johannes Erdfelt
 * libusb code found at libusb.sourceforge.net
 *
 * $Id$
 */

/**
 * @file
 * @brief This provides a search API for hot pluggble devices.
 */

#include "config.h"
#include <string.h>

#if defined(__linux__) && !defined(HAVE_LIBUSB) && !defined(HAVE_LIBHAL)
#include <sys/types.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#include "misc.h"
#include "pcsclite.h"
#include "pcscd.h"
#include "debuglog.h"
#include "parser.h"
#include "readerfactory.h"
#include "winscard_msg.h"
#include "sys_generic.h"
#include "hotplug.h"

#define PCSCLITE_USB_PATH		"/proc/bus/usb"

#define FALSE			0
#define TRUE			1

char ReCheckSerialReaders = FALSE;
PCSCLITE_MUTEX usbNotifierMutex;

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

static LONG HPAddHotPluggable(int, unsigned long);
static LONG HPRemoveHotPluggable(int, unsigned long);
static LONG HPReadBundleValues(void);
static void HPEstablishUSBNotifications(void);

static PCSCLITE_THREAD_T usbNotifyThread;
static int AraKiriHotPlug = FALSE;
static int bundleSize = 0;

/**
 * A list to keep track of 20 simultaneous readers
 */
static struct _bundleTracker
{
	long  manuID;
	long  productID;

	struct _deviceNumber {
		int  id;
		char status;
	} deviceNumber[PCSCLITE_MAX_READERS_CONTEXTS];

	char *bundleName;
	char *libraryPath;
	char *readerName;
}
bundleTracker[PCSCLITE_MAX_READERS_CONTEXTS];

static LONG HPReadBundleValues(void)
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
		Log1(PCSC_LOG_INFO,
			"Cannot open PC/SC drivers directory: " PCSCLITE_HP_DROPDIR);
		Log1(PCSC_LOG_INFO, "Disabling USB support for pcscd.");
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
			snprintf(fullPath, FILENAME_MAX, "%s/%s/Contents/Info.plist",
				PCSCLITE_HP_DROPDIR, currFP->d_name);
			fullPath[FILENAME_MAX - 1] = '\0';

			/* while we find a nth ifdVendorID in Info.plist */
			while (LTPBundleFindValueWithKey(fullPath, PCSCLITE_HP_MANUKEY_NAME,
				keyValue, alias) == 0)
			{
				bundleTracker[listCount].bundleName = strdup(currFP->d_name);

				/* Get ifdVendorID */
				rv = LTPBundleFindValueWithKey(fullPath, PCSCLITE_HP_MANUKEY_NAME,
					keyValue, alias);
				if (rv == 0)
					bundleTracker[listCount].manuID = strtol(keyValue, 0, 16);

				/* get ifdProductID */
				rv = LTPBundleFindValueWithKey(fullPath, PCSCLITE_HP_PRODKEY_NAME,
					keyValue, alias);
				if (rv == 0)
					bundleTracker[listCount].productID =
						strtol(keyValue, 0, 16);

				/* get ifdFriendlyName */
				rv = LTPBundleFindValueWithKey(fullPath, PCSCLITE_HP_NAMEKEY_NAME,
					keyValue, alias);
				if (rv == 0)
					bundleTracker[listCount].readerName = strdup(keyValue);

				/* get CFBundleExecutable */
				rv = LTPBundleFindValueWithKey(fullPath, PCSCLITE_HP_LIBRKEY_NAME,
					keyValue, 0);
				if (rv == 0)
				{
					snprintf(fullLibPath, sizeof(fullLibPath),
						"%s/%s/Contents/%s/%s",
						PCSCLITE_HP_DROPDIR, currFP->d_name, PCSC_ARCH, keyValue);
					fullLibPath[sizeof(fullLibPath) - 1] = '\0';
					bundleTracker[listCount].libraryPath = strdup(fullLibPath);
				}

				listCount++;
				alias++;

				if (listCount >= sizeof(bundleTracker)/sizeof(bundleTracker[0]))
				{
					Log2(PCSC_LOG_CRITICAL, "Too many readers declared. Maximum is %d", sizeof(bundleTracker)/sizeof(bundleTracker[0]));
					goto end;
				}
			}
		}
	}

end:
	bundleSize = listCount;

	if (bundleSize == 0)
	{
		Log1(PCSC_LOG_INFO,
			"No bundle files in pcsc drivers directory: " PCSCLITE_HP_DROPDIR);
		Log1(PCSC_LOG_INFO, "Disabling USB support for pcscd");
	}

	closedir(hpDir);
	return 0;
}

static void HPEstablishUSBNotifications(void)
{

	int i, j, usbDeviceStatus;
	DIR *dir, *dirB;
	struct dirent *entry, *entryB;
	int deviceNumber;
	int suspectDeviceNumber;
	char dirpath[FILENAME_MAX];
	char filename[FILENAME_MAX];
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

			for (j=0; j < PCSCLITE_MAX_READERS_CONTEXTS; j++)
				/* clear rollcall */
				bundleTracker[i].deviceNumber[j].status = 0;

			dir = NULL;
			dir = opendir(PCSCLITE_USB_PATH);
			if (dir == NULL)
			{
				Log1(PCSC_LOG_ERROR,
					"Cannot open USB path directory: " PCSCLITE_USB_PATH);
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
					Log2(PCSC_LOG_ERROR,
						"USB path seems to have disappeared %s", dirpath);
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
						for (j=0; j < PCSCLITE_MAX_READERS_CONTEXTS; j++)
						{
							if (bundleTracker[i].deviceNumber[j].id == deviceNumber &&
								bundleTracker[i].deviceNumber[j].id != 0)
							{
								bundleTracker[i].deviceNumber[j].status = 1; /* i'm here */
								break;
							}
						}

						if (j == PCSCLITE_MAX_READERS_CONTEXTS)
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

				for (j=0; j < PCSCLITE_MAX_READERS_CONTEXTS; j++)
				{
					if (bundleTracker[i].deviceNumber[j].id == 0)
						break;
				}

				if (j == PCSCLITE_MAX_READERS_CONTEXTS)
					Log1(PCSC_LOG_ERROR,
						"Too many identical readers plugged in");
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

					for (j=0; j < PCSCLITE_MAX_READERS_CONTEXTS; j++)
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
		if (AraKiriHotPlug)
		{
			int retval;

			Log1(PCSC_LOG_INFO, "Hotplug stopped");
			pthread_exit(&retval);
		}

	}	/* End of while loop */
}

LONG HPSearchHotPluggables(void)
{
	int i, j;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		bundleTracker[i].productID  = 0;
		bundleTracker[i].manuID     = 0;

		for (j=0; j < PCSCLITE_MAX_READERS_CONTEXTS; j++)
			bundleTracker[i].deviceNumber[j].id = 0;
	}

	HPReadBundleValues();

	SYS_ThreadCreate(&usbNotifyThread, THREAD_ATTR_DETACHED,
		(PCSCLITE_THREAD_FUNCTION( )) HPEstablishUSBNotifications, 0);

	return 0;
}

LONG HPStopHotPluggables(void)
{
	AraKiriHotPlug = TRUE;

	return 0;
}

static LONG HPAddHotPluggable(int i, unsigned long usbAddr)
{
	/* NOTE: The deviceName is an empty string "" until someone implements
	 * the code to get it */
	RFAddReader(bundleTracker[i].readerName, PCSCLITE_HP_BASE_PORT + usbAddr,
		bundleTracker[i].libraryPath, "");

	return 1;
}	/* End of function */

static LONG HPRemoveHotPluggable(int i, unsigned long usbAddr)
{
	RFRemoveReader(bundleTracker[i].readerName, PCSCLITE_HP_BASE_PORT + usbAddr);

	return 1;
}	/* End of function */

/**
 * Sets up callbacks for device hotplug events.
 */
ULONG HPRegisterForHotplugEvents(void)
{
	(void)SYS_MutexInit(&usbNotifierMutex);
	return 0;
}

void HPReCheckSerialReaders(void)
{
}

#endif	/* __linux__ && !HAVE_LIBUSB */
