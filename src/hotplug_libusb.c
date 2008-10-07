/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *  Toni Andjelkovic <toni@soth.at>
 *  Damien Sauveron <damien.sauveron@labri.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This provides a search API for hot pluggble devices.
 */

#include "config.h"
#ifdef HAVE_LIBUSB

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

#include "misc.h"
#include "wintypes.h"
#include "pcscd.h"
#include "debuglog.h"
#include "parser.h"
#include "readerfactory.h"
#include "winscard_msg.h"
#include "sys_generic.h"
#include "hotplug.h"
#include "utils.h"

#undef DEBUG_HOTPLUG
#define ADD_SERIAL_NUMBER

#define BUS_DEVICE_STRSIZE	256

#define READER_ABSENT		0
#define READER_PRESENT		1
#define READER_FAILED		2

#define FALSE			0
#define TRUE			1

extern PCSCLITE_MUTEX usbNotifierMutex;

static PCSCLITE_THREAD_T usbNotifyThread;
static int driverSize = -1;
static char AraKiriHotPlug = FALSE;
static int rescan_pipe[] = { -1, -1 };
extern int HPForceReaderPolling;

/* values of ifdCapabilities bits */
#define IFD_GENERATE_HOTPLUG 1

/**
 * keep track of drivers in a dynamically allocated array
 */
static struct _driverTracker
{
	long manuID;
	long productID;

	char *bundleName;
	char *libraryPath;
	char *readerName;
	int ifdCapabilities;
} *driverTracker = NULL;
#define DRIVER_TRACKER_SIZE_STEP 8

/**
 * keep track of PCSCLITE_MAX_READERS_CONTEXTS simultaneous readers
 */
static struct _readerTracker
{
	char status;
	char bus_device[BUS_DEVICE_STRSIZE];	/**< device name */
	char *fullName;	/**< full reader name (including serial number) */
} readerTracker[PCSCLITE_MAX_READERS_CONTEXTS];

static LONG HPReadBundleValues(void);
static LONG HPAddHotPluggable(struct usb_device *dev, const char bus_device[],
	struct _driverTracker *driver);
static LONG HPRemoveHotPluggable(int reader_index);
static void HPRescanUsbBus(void);
static void HPEstablishUSBNotifications(void);

static LONG HPReadBundleValues(void)
{
	LONG rv;
	DIR *hpDir;
	struct dirent *currFP = NULL;
	char fullPath[FILENAME_MAX];
	char fullLibPath[FILENAME_MAX];
	char keyValue[TOKEN_MAX_VALUE_SIZE];
	int listCount = 0;

	hpDir = opendir(PCSCLITE_HP_DROPDIR);

	if (hpDir == NULL)
	{
		Log1(PCSC_LOG_ERROR, "Cannot open PC/SC drivers directory: " PCSCLITE_HP_DROPDIR);
		Log1(PCSC_LOG_ERROR, "Disabling USB support for pcscd.");
		return -1;
	}

	/* allocate a first array */
	driverTracker = calloc(DRIVER_TRACKER_SIZE_STEP, sizeof(*driverTracker));
	if (NULL == driverTracker)
	{
		Log1(PCSC_LOG_CRITICAL, "Not enough memory");
		return -1;
	}
	driverSize = DRIVER_TRACKER_SIZE_STEP;

	while ((currFP = readdir(hpDir)) != 0)
	{
		if (strstr(currFP->d_name, ".bundle") != 0)
		{
			int alias = 0;

			/*
			 * The bundle exists - let's form a full path name and get the
			 * vendor and product ID's for this particular bundle
			 */
			snprintf(fullPath, sizeof(fullPath), "%s/%s/Contents/Info.plist",
				PCSCLITE_HP_DROPDIR, currFP->d_name);
			fullPath[sizeof(fullPath) - 1] = '\0';

			/* while we find a nth ifdVendorID in Info.plist */
			while (LTPBundleFindValueWithKey(fullPath, PCSCLITE_HP_MANUKEY_NAME,
				keyValue, alias) == 0)
			{
				driverTracker[listCount].bundleName = strdup(currFP->d_name);

				/* Get ifdVendorID */
				rv = LTPBundleFindValueWithKey(fullPath,
					PCSCLITE_HP_MANUKEY_NAME, keyValue, alias);
				if (rv == 0)
					driverTracker[listCount].manuID = strtol(keyValue, NULL, 16);

				/* get ifdProductID */
				rv = LTPBundleFindValueWithKey(fullPath,
					PCSCLITE_HP_PRODKEY_NAME, keyValue, alias);
				if (rv == 0)
					driverTracker[listCount].productID =
						strtol(keyValue, NULL, 16);

				/* get ifdFriendlyName */
				rv = LTPBundleFindValueWithKey(fullPath,
					PCSCLITE_HP_NAMEKEY_NAME, keyValue, alias);
				if (rv == 0)
					driverTracker[listCount].readerName = strdup(keyValue);

				/* get CFBundleExecutable */
				rv = LTPBundleFindValueWithKey(fullPath,
					PCSCLITE_HP_LIBRKEY_NAME, keyValue, 0);
				if (rv == 0)
				{
					snprintf(fullLibPath, sizeof(fullLibPath),
						"%s/%s/Contents/%s/%s",
						PCSCLITE_HP_DROPDIR, currFP->d_name, PCSC_ARCH,
						keyValue);
					fullLibPath[sizeof(fullLibPath) - 1] = '\0';
					driverTracker[listCount].libraryPath = strdup(fullLibPath);
				}

				/* Get ifdCapabilities */
				rv = LTPBundleFindValueWithKey(fullPath,
					PCSCLITE_HP_CPCTKEY_NAME, keyValue, 0);
				if (rv == 0)
					driverTracker[listCount].ifdCapabilities = strtol(keyValue,
						NULL, 16);

#ifdef DEBUG_HOTPLUG
				Log2(PCSC_LOG_INFO, "Found driver for: %s",
					driverTracker[listCount].readerName);
#endif
				alias++;

				if (NULL == driverTracker[listCount].readerName)
					continue;

				listCount++;
				if (listCount >= driverSize)
				{
					int i;

					/* increase the array size */
					driverSize += DRIVER_TRACKER_SIZE_STEP;
#ifdef DEBUG_HOTPLUG
					Log2(PCSC_LOG_INFO,
						"Increase driverTracker to %d entries", driverSize);
#endif
					driverTracker = realloc(driverTracker,
						driverSize * sizeof(*driverTracker));
					if (NULL == driverTracker)
					{
						Log1(PCSC_LOG_CRITICAL, "Not enough memory");
						driverSize = -1;
						return -1;
					}

					/* clean the newly allocated entries */
					for (i=driverSize-DRIVER_TRACKER_SIZE_STEP; i<driverSize; i++)
					{
						driverTracker[i].manuID = 0;
						driverTracker[i].productID = 0;
						driverTracker[i].bundleName = NULL;
						driverTracker[i].libraryPath = NULL;
						driverTracker[i].readerName = NULL;
						driverTracker[i].ifdCapabilities = 0;
					}
				}
			}
		}
	}

	driverSize = listCount;
	closedir(hpDir);

	rv = TRUE;
	if (driverSize == 0)
	{
		Log1(PCSC_LOG_INFO, "No bundle files in pcsc drivers directory: " PCSCLITE_HP_DROPDIR);
		Log1(PCSC_LOG_INFO, "Disabling USB support for pcscd");
		rv = FALSE;
	}
#ifdef DEBUG_HOTPLUG
	else
		Log2(PCSC_LOG_INFO, "Found drivers for %d readers", listCount);
#endif

	return rv;
}

static void HPRescanUsbBus(void)
{
	int i, j;
	struct usb_bus *bus;
	struct usb_device *dev;
	char bus_device[BUS_DEVICE_STRSIZE];

	usb_find_busses();
	usb_find_devices();

	for (i=0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		/* clear rollcall */
		readerTracker[i].status = READER_ABSENT;

	/* For each USB bus */
	for (bus = usb_get_busses(); bus; bus = bus->next)
	{
		/* For each USB device */
		for (dev = bus->devices; dev; dev = dev->next)
		{
			/* check if the device is supported by one driver */
			for (i=0; i<driverSize; i++)
			{
				if (driverTracker[i].libraryPath != NULL &&
					dev->descriptor.idVendor == driverTracker[i].manuID &&
					dev->descriptor.idProduct == driverTracker[i].productID)
				{
					int newreader;

					/* A known device has been found */
					snprintf(bus_device, BUS_DEVICE_STRSIZE, "%s:%s",
						bus->dirname, dev->filename);
					bus_device[BUS_DEVICE_STRSIZE - 1] = '\0';
#ifdef DEBUG_HOTPLUG
					Log2(PCSC_LOG_DEBUG, "Found matching USB device: %s",
						bus_device);
#endif
					newreader = TRUE;

					/* Check if the reader is a new one */
					for (j=0; j<PCSCLITE_MAX_READERS_CONTEXTS; j++)
					{
						if (strncmp(readerTracker[j].bus_device,
							bus_device, BUS_DEVICE_STRSIZE) == 0)
						{
							/* The reader is already known */
							readerTracker[j].status = READER_PRESENT;
							newreader = FALSE;
#ifdef DEBUG_HOTPLUG
							Log2(PCSC_LOG_DEBUG, "Refresh USB device: %s",
								bus_device);
#endif
							break;
						}
					}

					/* New reader found */
					if (newreader)
						HPAddHotPluggable(dev, bus_device, &driverTracker[i]);
				}
			}
		} /* End of USB device for..loop */

	} /* End of USB bus for..loop */

	/*
	 * check if all the previously found readers are still present
	 */
	for (i=0; i<PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
		int fd;
		char filename[BUS_DEVICE_STRSIZE];

		/*	BSD workaround:
		 *	ugenopen() in sys/dev/usb/ugen.c returns EBUSY
		 *	when the character device file is already open.
		 *	Because of this, open usb devices will not be
		 *	detected by usb_find_devices(), so we have to
		 *	check for this explicitly.
		 */
		if (readerTracker[i].status == READER_PRESENT ||
			 readerTracker[i].fullName == NULL)
			continue;

		sscanf(readerTracker[i].bus_device, "%*[^:]%*[:]%s", filename);
		fd = open(filename, O_RDONLY);
		if (fd == -1)
		{
			if (errno == EBUSY)
			{
				/* The device is present */
#ifdef DEBUG_HOTPLUG
				Log2(PCSC_LOG_DEBUG, "BSD: EBUSY on %s", filename);
#endif
				readerTracker[i].status = READER_PRESENT;
			}
#ifdef DEBUG_HOTPLUG
			else
				Log3(PCSC_LOG_DEBUG, "BSD: %s error: %s", filename,
					strerror(errno));
#endif
		}
		else
		{
#ifdef DEBUG_HOTPLUG
			Log2(PCSC_LOG_DEBUG, "BSD: %s still present", filename);
#endif
			readerTracker[i].status = READER_PRESENT;
			close(fd);
		}
#endif
		if ((readerTracker[i].status == READER_ABSENT) &&
			(readerTracker[i].fullName != NULL))
			HPRemoveHotPluggable(i);
	}

	if (AraKiriHotPlug)
	{
		int retval;

		for (i=0; i<driverSize; i++)
		{
			/* free strings allocated by strdup() */
			free(driverTracker[i].bundleName);
			free(driverTracker[i].libraryPath);
			free(driverTracker[i].readerName);
		}
		free(driverTracker);

		Log1(PCSC_LOG_INFO, "Hotplug stopped");
		pthread_exit(&retval);
	}
}

static void HPEstablishUSBNotifications(void)
{
	int i, do_polling;

	/* libusb default is /dev/bus/usb but the devices are not yet visible there
	 * when a hotplug is requested */
	setenv("USB_DEVFS_PATH", "/proc/bus/usb", 0);

	usb_init();

	/* scan the USB bus for devices at startup */
	HPRescanUsbBus();

	/* if at least one driver do not have IFD_GENERATE_HOTPLUG */
	do_polling = FALSE;
	for (i=0; i<driverSize; i++)
		if (driverTracker[i].libraryPath)
			if ((driverTracker[i].ifdCapabilities & IFD_GENERATE_HOTPLUG) == 0)
			{
				Log2(PCSC_LOG_INFO,
					"Driver %s does not support IFD_GENERATE_HOTPLUG. Using active polling instead.",
					driverTracker[i].bundleName);
				if (HPForceReaderPolling < 1)
					HPForceReaderPolling = 1;
				break;
			}

	if (HPForceReaderPolling)
	{
		Log2(PCSC_LOG_INFO,
				"Polling forced every %d second(s)", HPForceReaderPolling);
		do_polling = TRUE;
	}

	if (do_polling)
	{
		while (!AraKiriHotPlug)
		{
			SYS_Sleep(HPForceReaderPolling);
			HPRescanUsbBus();
		}
	}
	else
	{
		char dummy;

	  	pipe(rescan_pipe);
		while (read(rescan_pipe[0], &dummy, sizeof(dummy)) > 0)
		{
			Log1(PCSC_LOG_INFO, "Reload serial configuration");
			HPRescanUsbBus();
			RFReCheckReaderConf();
			Log1(PCSC_LOG_INFO, "End reload serial configuration");
		}
		close(rescan_pipe[0]);
		rescan_pipe[0] = -1;
	}
}

LONG HPSearchHotPluggables(void)
{
	int i;

	for (i=0; i<PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		readerTracker[i].status = READER_ABSENT;
		readerTracker[i].bus_device[0] = '\0';
		readerTracker[i].fullName = NULL;
	}

	if (HPReadBundleValues())
		SYS_ThreadCreate(&usbNotifyThread, THREAD_ATTR_DETACHED,
			(PCSCLITE_THREAD_FUNCTION( )) HPEstablishUSBNotifications, NULL);

	return 0;
}

LONG HPStopHotPluggables(void)
{
	AraKiriHotPlug = TRUE;
	if (rescan_pipe[1] >= 0)
	{
	  	close(rescan_pipe[1]);
		rescan_pipe[1] = -1;
	}

	return 0;
}

static LONG HPAddHotPluggable(struct usb_device *dev, const char bus_device[],
	struct _driverTracker *driver)
{
	int i;
	char deviceName[MAX_DEVICENAME];

	Log2(PCSC_LOG_INFO, "Adding USB device: %s", bus_device);

	snprintf(deviceName, sizeof(deviceName), "usb:%04x/%04x:libusb:%s",
		dev->descriptor.idVendor, dev->descriptor.idProduct, bus_device);
	deviceName[sizeof(deviceName) -1] = '\0';

	SYS_MutexLock(&usbNotifierMutex);

	/* find a free entry */
	for (i=0; i<PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if (readerTracker[i].fullName == NULL)
			break;
	}

	if (i==PCSCLITE_MAX_READERS_CONTEXTS)
	{
		Log2(PCSC_LOG_ERROR,
			"Not enough reader entries. Already found %d readers", i);
		SYS_MutexUnLock(&usbNotifierMutex);
		return 0;
	}

	strncpy(readerTracker[i].bus_device, bus_device,
		sizeof(readerTracker[i].bus_device));
	readerTracker[i].bus_device[sizeof(readerTracker[i].bus_device) - 1] = '\0';

#ifdef ADD_SERIAL_NUMBER
	if (dev->descriptor.iSerialNumber)
	{
		usb_dev_handle *device;
		char serialNumber[MAX_READERNAME];
		char fullname[MAX_READERNAME];

		device = usb_open(dev);
		usb_get_string_simple(device, dev->descriptor.iSerialNumber,
			serialNumber, MAX_READERNAME);
		usb_close(device);

		snprintf(fullname, sizeof(fullname), "%s (%s)",
			driver->readerName, serialNumber);
		readerTracker[i].fullName = strdup(fullname);
	}
	else
#endif
		readerTracker[i].fullName = strdup(driver->readerName);

	if (RFAddReader(readerTracker[i].fullName, PCSCLITE_HP_BASE_PORT + i,
		driver->libraryPath, deviceName) == SCARD_S_SUCCESS)
		readerTracker[i].status = READER_PRESENT;
	else
	{
		readerTracker[i].status = READER_FAILED;

		(void)CheckForOpenCT();
	}

	SYS_MutexUnLock(&usbNotifierMutex);

	return 1;
}	/* End of function */

static LONG HPRemoveHotPluggable(int reader_index)
{
	SYS_MutexLock(&usbNotifierMutex);

	Log3(PCSC_LOG_INFO, "Removing USB device[%d]: %s", reader_index,
		readerTracker[reader_index].bus_device);

	RFRemoveReader(readerTracker[reader_index].fullName,
		PCSCLITE_HP_BASE_PORT + reader_index);
	free(readerTracker[reader_index].fullName);
	readerTracker[reader_index].status = READER_ABSENT;
	readerTracker[reader_index].bus_device[0] = '\0';
	readerTracker[reader_index].fullName = NULL;

	SYS_MutexUnLock(&usbNotifierMutex);

	return 1;
}	/* End of function */

/**
 * Sets up callbacks for device hotplug events.
 */
ULONG HPRegisterForHotplugEvents(void)
{
	return 0;
}

void HPReCheckSerialReaders(void)
{
  	if (rescan_pipe[1] >= 0)
	{
		char dummy = 0;
		write(rescan_pipe[1], &dummy, sizeof(dummy));
	}
}

#endif

