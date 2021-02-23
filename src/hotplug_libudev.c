/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2011
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 * Copyright (C) 2014
 *  Stefani Seibold <stefani@seibold.net>
 *
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @brief This provides a search API for hot plugable devices using libudev
 */

#include "config.h"
#if defined(HAVE_LIBUDEV) && defined(USE_USB)

#define _GNU_SOURCE		/* for asprintf(3) */
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <pthread.h>
#include <libudev.h>
#include <poll.h>
#include <ctype.h>

#include "debuglog.h"
#include "parser.h"
#include "readerfactory.h"
#include "sys_generic.h"
#include "hotplug.h"
#include "utils.h"

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression) \
  (__extension__							      \
    ({ long int __result;						      \
       do __result = (long int) (expression);				      \
       while (__result == -1L && errno == EINTR);			      \
       __result; }))
#endif

#undef DEBUG_HOTPLUG

#define FALSE			0
#define TRUE			1

extern char Add_Interface_In_Name;
extern char Add_Serial_In_Name;

static pthread_t usbNotifyThread;
static int driverSize = -1;
static struct udev *Udev;


/**
 * keep track of drivers in a dynamically allocated array
 */
static struct _driverTracker
{
	unsigned int manuID;
	unsigned int productID;

	char *bundleName;
	char *libraryPath;
	char *readerName;
	char *CFBundleName;
} *driverTracker = NULL;
#define DRIVER_TRACKER_SIZE_STEP 10

/* The CCID driver already supports 176 readers.
 * We start with a big array size to avoid reallocation. */
#define DRIVER_TRACKER_INITIAL_SIZE 200

/**
 * keep track of PCSCLITE_MAX_READERS_CONTEXTS simultaneous readers
 */
static struct _readerTracker
{
	char *devpath;	/**< device name seen by udev */
	char *fullName;	/**< full reader name (including serial number) */
	char *sysname;	/**< sysfs path */
} readerTracker[PCSCLITE_MAX_READERS_CONTEXTS];


static LONG HPReadBundleValues(void)
{
	LONG rv;
	DIR *hpDir;
	struct dirent *currFP = NULL;
	char fullPath[FILENAME_MAX];
	char fullLibPath[FILENAME_MAX];
	int listCount = 0;

	hpDir = opendir(PCSCLITE_HP_DROPDIR);

	if (NULL == hpDir)
	{
		Log1(PCSC_LOG_ERROR, "Cannot open PC/SC drivers directory: " PCSCLITE_HP_DROPDIR);
		Log1(PCSC_LOG_ERROR, "Disabling USB support for pcscd.");
		return -1;
	}

	/* allocate a first array */
	driverSize = DRIVER_TRACKER_INITIAL_SIZE;
	driverTracker = calloc(driverSize, sizeof(*driverTracker));
	if (NULL == driverTracker)
	{
		Log1(PCSC_LOG_CRITICAL, "Not enough memory");
		(void)closedir(hpDir);
		return -1;
	}

#define GET_KEY(key, values) \
	rv = LTPBundleFindValueWithKey(&plist, key, values); \
	if (rv) \
	{ \
		Log2(PCSC_LOG_ERROR, "Value/Key not defined for " key " in %s", \
			fullPath); \
		continue; \
	}

	while ((currFP = readdir(hpDir)) != 0)
	{
		if (strstr(currFP->d_name, ".bundle") != 0)
		{
			unsigned int alias;
			list_t plist, *values;
			list_t *manuIDs, *productIDs, *readerNames;
			char *CFBundleName;
			char *libraryPath;

			/*
			 * The bundle exists - let's form a full path name and get the
			 * vendor and product ID's for this particular bundle
			 */
			(void)snprintf(fullPath, sizeof(fullPath), "%s/%s/Contents/Info.plist",
				PCSCLITE_HP_DROPDIR, currFP->d_name);
			fullPath[sizeof(fullPath) - 1] = '\0';

			rv = bundleParse(fullPath, &plist);
			if (rv)
				continue;

			/* get CFBundleExecutable */
			GET_KEY(PCSCLITE_HP_LIBRKEY_NAME, &values)
			libraryPath = list_get_at(values, 0);
			(void)snprintf(fullLibPath, sizeof(fullLibPath),
				"%s/%s/Contents/%s/%s",
				PCSCLITE_HP_DROPDIR, currFP->d_name, PCSC_ARCH,
				libraryPath);
			fullLibPath[sizeof(fullLibPath) - 1] = '\0';

			GET_KEY(PCSCLITE_HP_MANUKEY_NAME, &manuIDs)
			GET_KEY(PCSCLITE_HP_PRODKEY_NAME, &productIDs)
			GET_KEY(PCSCLITE_HP_NAMEKEY_NAME, &readerNames)

			if  ((list_size(manuIDs) != list_size(productIDs))
				|| (list_size(manuIDs) != list_size(readerNames)))
			{
				Log2(PCSC_LOG_CRITICAL, "Error parsing %s", fullPath);
				(void)closedir(hpDir);
				return -1;
			}

			/* Get CFBundleName */
			rv = LTPBundleFindValueWithKey(&plist, PCSCLITE_HP_CFBUNDLE_NAME,
				&values);
			if (rv)
				CFBundleName = NULL;
			else
				CFBundleName = strdup(list_get_at(values, 0));

			/* while we find a nth ifdVendorID in Info.plist */
			for (alias=0; alias<list_size(manuIDs); alias++)
			{
				char *value;

				/* variables entries */
				value = list_get_at(manuIDs, alias);
				driverTracker[listCount].manuID = strtol(value, NULL, 16);

				value = list_get_at(productIDs, alias);
				driverTracker[listCount].productID = strtol(value, NULL, 16);

				driverTracker[listCount].readerName = strdup(list_get_at(readerNames, alias));

				/* constant entries for a same driver */
				driverTracker[listCount].bundleName = strdup(currFP->d_name);
				driverTracker[listCount].libraryPath = strdup(fullLibPath);
				driverTracker[listCount].CFBundleName = CFBundleName;

#ifdef DEBUG_HOTPLUG
				Log2(PCSC_LOG_INFO, "Found driver for: %s",
					driverTracker[listCount].readerName);
#endif
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

					void* tmp = realloc(driverTracker,
						driverSize * sizeof(*driverTracker));

					if (NULL == tmp)
					{
						free(driverTracker);
						Log1(PCSC_LOG_CRITICAL, "Not enough memory");
						driverSize = -1;
						(void)closedir(hpDir);
						return -1;
					}
					driverTracker = tmp;

					/* clean the newly allocated entries */
					for (i=driverSize-DRIVER_TRACKER_SIZE_STEP; i<driverSize; i++)
					{
						driverTracker[i].manuID = 0;
						driverTracker[i].productID = 0;
						driverTracker[i].bundleName = NULL;
						driverTracker[i].libraryPath = NULL;
						driverTracker[i].readerName = NULL;
						driverTracker[i].CFBundleName = NULL;
					}
				}
			}
			bundleRelease(&plist);
		}
	}

	driverSize = listCount;
	(void)closedir(hpDir);

#ifdef DEBUG_HOTPLUG
	Log2(PCSC_LOG_INFO, "Found drivers for %d readers", listCount);
#endif

	return 0;
} /* HPReadBundleValues */


/*@null@*/ static struct _driverTracker *get_driver(struct udev_device *dev,
	const char *devpath, struct _driverTracker **classdriver)
{
	int i;
	unsigned int idVendor, idProduct;
	static struct _driverTracker *driver;
	const char *str;

	str = udev_device_get_sysattr_value(dev, "idVendor");
	if (!str)
	{
		Log1(PCSC_LOG_ERROR, "udev_device_get_sysattr_value() failed");
		return NULL;
	}
	idVendor = strtol(str, NULL, 16);

	str = udev_device_get_sysattr_value(dev, "idProduct");
	if (!str)
	{
		Log1(PCSC_LOG_ERROR, "udev_device_get_sysattr_value() failed");
		return NULL;
	}
	idProduct = strtol(str, NULL, 16);

#ifdef NO_LOG
	(void)devpath;
#endif
	Log4(PCSC_LOG_DEBUG,
		"Looking for a driver for VID: 0x%04X, PID: 0x%04X, path: %s",
		idVendor, idProduct, devpath);

	*classdriver = NULL;
	driver = NULL;
	/* check if the device is supported by one driver */
	for (i=0; i<driverSize; i++)
	{
		if (driverTracker[i].libraryPath != NULL &&
			idVendor == driverTracker[i].manuID &&
			idProduct == driverTracker[i].productID)
		{
			if ((driverTracker[i].CFBundleName != NULL)
				&& (0 == strcmp(driverTracker[i].CFBundleName, "CCIDCLASSDRIVER")))
				*classdriver = &driverTracker[i];
			else
				/* it is not a CCID Class driver */
				driver = &driverTracker[i];
		}
	}

	/* if we found a specific driver */
	if (driver)
		return driver;

	/* else return the Class driver (if any) */
	return *classdriver;
}


static void HPRemoveDevice(struct udev_device *dev)
{
	int i;
	const char *devpath;
	struct udev_device *parent;
	const char *sysname;

	/* The device pointed to by dev contains information about
	   the interface. In order to get information about the USB
	   device, get the parent device with the subsystem/devtype pair
	   of "usb"/"usb_device". This will be several levels up the
	   tree, but the function will find it.*/
	parent = udev_device_get_parent_with_subsystem_devtype(dev, "usb",
		"usb_device");
	if (!parent)
		return;

	devpath = udev_device_get_devnode(parent);
	if (!devpath)
	{
		/* the device disapeared? */
		Log1(PCSC_LOG_ERROR, "udev_device_get_devnode() failed");
		return;
	}

	sysname = udev_device_get_sysname(dev);
	if (!sysname)
	{
		Log1(PCSC_LOG_ERROR, "udev_device_get_sysname() failed");
		return;
	}

	for (i=0; i<PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if (readerTracker[i].fullName && !strcmp(sysname, readerTracker[i].sysname))
		{
			Log4(PCSC_LOG_INFO, "Removing USB device[%d]: %s at %s", i,
				readerTracker[i].fullName, readerTracker[i].devpath);

			RFRemoveReader(readerTracker[i].fullName, PCSCLITE_HP_BASE_PORT + i);

			free(readerTracker[i].devpath);
			readerTracker[i].devpath = NULL;
			free(readerTracker[i].fullName);
			readerTracker[i].fullName = NULL;
			free(readerTracker[i].sysname);
			readerTracker[i].sysname = NULL;
			break;
		}
	}
}


static void HPAddDevice(struct udev_device *dev)
{
	int index, a;
	char *deviceName = NULL;
	char *fullname = NULL;
	struct _driverTracker *driver, *classdriver;
	const char *sSerialNumber = NULL, *sInterfaceName = NULL;
	const char *sInterfaceNumber;
	LONG ret;
	int bInterfaceNumber;
	const char *devpath;
	struct udev_device *parent;
	const char *sysname;

	/* The device pointed to by dev contains information about
	   the interface. In order to get information about the USB
	   device, get the parent device with the subsystem/devtype pair
	   of "usb"/"usb_device". This will be several levels up the
	   tree, but the function will find it.*/
	parent = udev_device_get_parent_with_subsystem_devtype(dev, "usb",
		"usb_device");
	if (!parent)
		return;

	devpath = udev_device_get_devnode(parent);
	if (!devpath)
	{
		/* the device disapeared? */
		Log1(PCSC_LOG_ERROR, "udev_device_get_devnode() failed");
		return;
	}

	driver = get_driver(parent, devpath, &classdriver);
	if (NULL == driver)
	{
		/* not a smart card reader */
#ifdef DEBUG_HOTPLUG
		Log2(PCSC_LOG_DEBUG, "%s is not a supported smart card reader",
			devpath);
#endif
		return;
	}

	sysname = udev_device_get_sysname(dev);
	if (!sysname)
	{
		Log1(PCSC_LOG_ERROR, "udev_device_get_sysname() failed");
		return;
	}

	/* check for duplicated add */
	for (index=0; index<PCSCLITE_MAX_READERS_CONTEXTS; index++)
	{
		if (readerTracker[index].fullName && !strcmp(sysname, readerTracker[index].sysname))
			return;
	}

	Log2(PCSC_LOG_INFO, "Adding USB device: %s", driver->readerName);

	sInterfaceNumber = udev_device_get_sysattr_value(dev, "bInterfaceNumber");
	if (sInterfaceNumber)
		bInterfaceNumber = atoi(sInterfaceNumber);
	else
		bInterfaceNumber = 0;

	a = asprintf(&deviceName, "usb:%04x/%04x:libudev:%d:%s",
		driver->manuID, driver->productID, bInterfaceNumber, devpath);
	if (-1 ==  a)
	{
		Log1(PCSC_LOG_ERROR, "asprintf() failed");
		return;
	}

	/* find a free entry */
	for (index=0; index<PCSCLITE_MAX_READERS_CONTEXTS; index++)
	{
		if (NULL == readerTracker[index].fullName)
			break;
	}

	if (PCSCLITE_MAX_READERS_CONTEXTS == index)
	{
		Log2(PCSC_LOG_ERROR,
			"Not enough reader entries. Already found %d readers", index);
		return;
	}

	if (Add_Interface_In_Name)
		sInterfaceName = udev_device_get_sysattr_value(dev, "interface");

	if (Add_Serial_In_Name)
		sSerialNumber = udev_device_get_sysattr_value(parent, "serial");

	/* name from the Info.plist file */
	fullname = strdup(driver->readerName);

	/* interface name from the device (if any) */
	if (sInterfaceName)
	{
		char *result;

		char *tmpInterfaceName = strdup(sInterfaceName);

		/* check the interface name contains only valid ASCII codes */
		for (size_t i=0; i<strlen(tmpInterfaceName); i++)
		{
			if (! isascii(tmpInterfaceName[i]))
				tmpInterfaceName[i] = '.';
		}

		/* create a new name */
		a = asprintf(&result, "%s [%s]", fullname, tmpInterfaceName);
		if (-1 ==  a)
		{
			Log1(PCSC_LOG_ERROR, "asprintf() failed");
			free(tmpInterfaceName);
			goto exit;
		}

		free(fullname);
		free(tmpInterfaceName);
		fullname = result;
	}

	/* serial number from the device (if any) */
	if (sSerialNumber)
	{
		/* only add the serial number if it is not already present in the
		 * interface name */
		if (!sInterfaceName || NULL == strstr(sInterfaceName, sSerialNumber))
		{
			char *result;

			/* create a new name */
			a = asprintf(&result, "%s (%s)", fullname, sSerialNumber);
			if (-1 ==  a)
			{
				Log1(PCSC_LOG_ERROR, "asprintf() failed");
				goto exit;
			}

			free(fullname);
			fullname = result;
		}
	}

	readerTracker[index].fullName = strdup(fullname);
	readerTracker[index].devpath = strdup(devpath);
	readerTracker[index].sysname = strdup(sysname);

	ret = RFAddReader(fullname, PCSCLITE_HP_BASE_PORT + index,
		driver->libraryPath, deviceName);
	if ((SCARD_S_SUCCESS != ret) && (SCARD_E_UNKNOWN_READER != ret))
	{
		Log2(PCSC_LOG_ERROR, "Failed adding USB device: %s",
			driver->readerName);

		if (classdriver && driver != classdriver)
		{
			/* the reader can also be used by the a class driver */
			ret = RFAddReader(fullname, PCSCLITE_HP_BASE_PORT + index,
				classdriver->libraryPath, deviceName);
			if ((SCARD_S_SUCCESS != ret) && (SCARD_E_UNKNOWN_READER != ret))
			{
				Log2(PCSC_LOG_ERROR, "Failed adding USB device: %s",
						driver->readerName);
				(void)CheckForOpenCT();
			}
		}
		else
		{
			(void)CheckForOpenCT();
		}
	}

	if (SCARD_S_SUCCESS != ret)
	{
		/* adding the reader failed */
		free(readerTracker[index].devpath);
		readerTracker[index].devpath = NULL;
		free(readerTracker[index].fullName);
		readerTracker[index].fullName = NULL;
		free(readerTracker[index].sysname);
		readerTracker[index].sysname = NULL;
	}

exit:
	free(fullname);
	free(deviceName);
} /* HPAddDevice */


static void HPScanUSB(struct udev *udev)
{
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;

	/* Create a list of the devices in the 'usb' subsystem. */
	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "usb");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	/* For each item enumerated */
	udev_list_entry_foreach(dev_list_entry, devices)
	{
		struct udev_device *dev;
		const char *devpath;

		/* Get the filename of the /sys entry for the device
		   and create a udev_device object (dev) representing it */
		devpath = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, devpath);

#ifdef DEBUG_HOTPLUG
		Log2(PCSC_LOG_DEBUG, "Found matching USB device: %s", devpath);
#endif
		HPAddDevice(dev);

		/* free device */
		udev_device_unref(dev);
	}

	/* Free the enumerator object */
	udev_enumerate_unref(enumerate);
}


static void * HPEstablishUSBNotifications(void *arg)
{
	struct udev_monitor *udev_monitor = arg;
	int r;
	int fd;
	struct pollfd pfd;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	/* udev monitor file descriptor */
	fd = udev_monitor_get_fd(udev_monitor);
	if (fd < 0)
	{
		Log2(PCSC_LOG_ERROR, "udev_monitor_get_fd() error: %d", fd);
		pthread_exit(NULL);
	}

	pfd.fd = fd;
	pfd.events = POLLIN;

	for (;;)
	{
		struct udev_device *dev;

#ifdef DEBUG_HOTPLUG
		Log0(PCSC_LOG_INFO);
#endif
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		/* wait for a udev event */
		r = TEMP_FAILURE_RETRY(poll(&pfd, 1, -1));
		if (r < 0)
		{
			Log2(PCSC_LOG_ERROR, "select(): %s", strerror(errno));
			pthread_exit(NULL);
		}

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		dev = udev_monitor_receive_device(udev_monitor);
		if (dev)
		{
			const char *action = udev_device_get_action(dev);

			if (action)
			{
				if (!strcmp("remove", action))
				{
					Log1(PCSC_LOG_INFO, "USB Device removed");
					HPRemoveDevice(dev);
				}
				else
				if (!strcmp("add", action))
				{
					Log1(PCSC_LOG_INFO, "USB Device add");
					HPAddDevice(dev);
				}
			}

			/* free device */
			udev_device_unref(dev);
		}
	}

	pthread_exit(NULL);
} /* HPEstablishUSBNotifications */


/***
 * Start a thread waiting for hotplug events
 */
LONG HPSearchHotPluggables(void)
{
	int i;

	for (i=0; i<PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		readerTracker[i].devpath = NULL;
		readerTracker[i].fullName = NULL;
		readerTracker[i].sysname = NULL;
	}

	return HPReadBundleValues();
} /* HPSearchHotPluggables */


/**
 * Stop the hotplug thread
 */
LONG HPStopHotPluggables(void)
{
	int i;

	if (driverSize <= 0)
		return 0;

	if (!Udev)
		return 0;

	pthread_cancel(usbNotifyThread);
	pthread_join(usbNotifyThread, NULL);

	for (i=0; i<driverSize; i++)
	{
		/* free strings allocated by strdup() */
		free(driverTracker[i].bundleName);
		free(driverTracker[i].libraryPath);
		free(driverTracker[i].readerName);
	}
	free(driverTracker);

	udev_unref(Udev);

	Udev = NULL;
	driverSize = -1;

	Log1(PCSC_LOG_INFO, "Hotplug stopped");
	return 0;
} /* HPStopHotPluggables */


/**
 * Sets up callbacks for device hotplug events.
 */
ULONG HPRegisterForHotplugEvents(void)
{
	struct udev_monitor *udev_monitor;
	int r;

	if (driverSize <= 0)
	{
		Log1(PCSC_LOG_INFO, "No bundle files in pcsc drivers directory: "
			PCSCLITE_HP_DROPDIR);
		Log1(PCSC_LOG_INFO, "Disabling USB support for pcscd");
		return 0;
	}

	/* Create the udev object */
	Udev = udev_new();
	if (!Udev)
	{
		Log1(PCSC_LOG_ERROR, "udev_new() failed");
		return SCARD_F_INTERNAL_ERROR;
	}

	udev_monitor = udev_monitor_new_from_netlink(Udev, "udev");
	if (NULL == udev_monitor)
	{
		Log1(PCSC_LOG_ERROR, "udev_monitor_new_from_netlink() error");
		pthread_exit(NULL);
	}

	/* filter only the interfaces */
	r = udev_monitor_filter_add_match_subsystem_devtype(udev_monitor, "usb",
		"usb_interface");
	if (r)
	{
		Log2(PCSC_LOG_ERROR, "udev_monitor_filter_add_match_subsystem_devtype() error: %d\n", r);
		pthread_exit(NULL);
	}

	r = udev_monitor_enable_receiving(udev_monitor);
	if (r)
	{
		Log2(PCSC_LOG_ERROR, "udev_monitor_enable_receiving() error: %d\n", r);
		pthread_exit(NULL);
	}

	/* scan the USB bus at least once before accepting client connections */
	HPScanUSB(Udev);

	if (ThreadCreate(&usbNotifyThread, 0,
		(PCSCLITE_THREAD_FUNCTION( )) HPEstablishUSBNotifications, udev_monitor))
	{
		Log1(PCSC_LOG_ERROR, "ThreadCreate() failed");
		return SCARD_F_INTERNAL_ERROR;
	}

	return 0;
} /* HPRegisterForHotplugEvents */


void HPReCheckSerialReaders(void)
{
	/* nothing to do here */
#ifdef DEBUG_HOTPLUG
	Log0(PCSC_LOG_ERROR);
#endif
} /* HPReCheckSerialReaders */

#endif

