/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2011
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This provides a search API for hot pluggble devices using libudev
 */

#include "config.h"
#if defined(HAVE_LIBUDEV) && defined(USE_USB)

#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <pthread.h>
#include <libudev.h>

#include "debuglog.h"
#include "parser.h"
#include "readerfactory.h"
#include "sys_generic.h"
#include "hotplug.h"
#include "utils.h"
#include "strlcpycat.h"

#undef DEBUG_HOTPLUG
#define ADD_SERIAL_NUMBER
#define ADD_INTERFACE_NAME

#define FALSE			0
#define TRUE			1

pthread_mutex_t usbNotifierMutex;

static pthread_t usbNotifyThread;
static int driverSize = -1;
static char AraKiriHotPlug = FALSE;

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

typedef enum {
 READER_ABSENT,
 READER_PRESENT,
 READER_FAILED
} readerState_t;

/**
 * keep track of PCSCLITE_MAX_READERS_CONTEXTS simultaneous readers
 */
static struct _readerTracker
{
	readerState_t status;	/** reader state */
	char bInterfaceNumber;	/** interface number on the device */
	char *devpath;	/**< device name seen by udev */
	char *fullName;	/**< full reader name (including serial number) */
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
					driverTracker = realloc(driverTracker,
						driverSize * sizeof(*driverTracker));
					if (NULL == driverTracker)
					{
						Log1(PCSC_LOG_CRITICAL, "Not enough memory");
						driverSize = -1;
						(void)closedir(hpDir);
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
						driverTracker[i].CFBundleName = NULL;
					}
				}
			}
			bundleRelease(&plist);
			free(CFBundleName);
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
	sscanf(str, "%X", &idVendor);

	str = udev_device_get_sysattr_value(dev, "idProduct");
	if (!str)
	{
		Log1(PCSC_LOG_ERROR, "udev_device_get_sysattr_value() failed");
		return NULL;
	}
	sscanf(str, "%X", &idProduct);

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


static void HPAddDevice(struct udev_device *dev, struct udev_device *parent,
	const char *devpath)
{
	int i;
	char deviceName[MAX_DEVICENAME];
	char fullname[MAX_READERNAME];
	struct _driverTracker *driver, *classdriver;
	const char *sSerialNumber = NULL, *sInterfaceName = NULL;
	LONG ret;
	int bInterfaceNumber;

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

	Log2(PCSC_LOG_INFO, "Adding USB device: %s", driver->readerName);

	bInterfaceNumber = atoi(udev_device_get_sysattr_value(dev,
		"bInterfaceNumber"));
	(void)snprintf(deviceName, sizeof(deviceName),
		"usb:%04x/%04x:libudev:%d:%s", driver->manuID, driver->productID,
		bInterfaceNumber, devpath);
	deviceName[sizeof(deviceName) -1] = '\0';

	(void)pthread_mutex_lock(&usbNotifierMutex);

	/* find a free entry */
	for (i=0; i<PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if (NULL == readerTracker[i].fullName)
			break;
	}

	if (PCSCLITE_MAX_READERS_CONTEXTS == i)
	{
		Log2(PCSC_LOG_ERROR,
			"Not enough reader entries. Already found %d readers", i);
		(void)pthread_mutex_unlock(&usbNotifierMutex);
		return;
	}

#ifdef ADD_INTERFACE_NAME
	sInterfaceName = udev_device_get_sysattr_value(dev, "interface");
#endif

#ifdef ADD_SERIAL_NUMBER
	sSerialNumber = udev_device_get_sysattr_value(parent, "serial");
#endif

	/* name from the Info.plist file */
	strlcpy(fullname, driver->readerName, sizeof(fullname));

	/* interface name from the device (if any) */
	if (sInterfaceName)
	{
		strlcat(fullname, " [", sizeof(fullname));
		strlcat(fullname, sInterfaceName, sizeof(fullname));
		strlcat(fullname, "]", sizeof(fullname));
	}

	/* serial number from the device (if any) */
	if (sSerialNumber)
	{
		/* only add the serial number if it is not already present in the
		 * interface name */
		if (!sInterfaceName || NULL == strstr(sInterfaceName, sSerialNumber))
		{
			strlcat(fullname, " (", sizeof(fullname));
			strlcat(fullname, sSerialNumber, sizeof(fullname));
			strlcat(fullname, ")", sizeof(fullname));
		}
	}

	readerTracker[i].fullName = strdup(fullname);
	readerTracker[i].devpath = strdup(devpath);
	readerTracker[i].status = READER_PRESENT;
	readerTracker[i].bInterfaceNumber = bInterfaceNumber;

	ret = RFAddReader(fullname, PCSCLITE_HP_BASE_PORT + i,
		driver->libraryPath, deviceName);
	if ((SCARD_S_SUCCESS != ret) && (SCARD_E_UNKNOWN_READER != ret))
	{
		Log2(PCSC_LOG_ERROR, "Failed adding USB device: %s",
			driver->readerName);

		if (classdriver && driver != classdriver)
		{
			/* the reader can also be used by the a class driver */
			ret = RFAddReader(fullname, PCSCLITE_HP_BASE_PORT + i,
				classdriver->libraryPath, deviceName);
			if ((SCARD_S_SUCCESS != ret) && (SCARD_E_UNKNOWN_READER != ret))
			{
				Log2(PCSC_LOG_ERROR, "Failed adding USB device: %s",
						driver->readerName);

				readerTracker[i].status = READER_FAILED;

				(void)CheckForOpenCT();
			}
		}
		else
		{
			readerTracker[i].status = READER_FAILED;

			(void)CheckForOpenCT();
		}
	}

	(void)pthread_mutex_unlock(&usbNotifierMutex);
} /* HPAddDevice */


static void HPRescanUsbBus(struct udev *udev)
{
	int i, j;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;

	/* all reader are marked absent */
	for (i=0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		readerTracker[i].status = READER_ABSENT;

	/* Create a list of the devices in the 'usb' subsystem. */
	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "usb");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	/* For each item enumerated */
	udev_list_entry_foreach(dev_list_entry, devices)
	{
		const char *devpath;
		struct udev_device *dev, *parent;
		struct _driverTracker *driver, *classdriver;
		int newreader;
		int bInterfaceNumber;
		const char *interface;

		/* Get the filename of the /sys entry for the device
		   and create a udev_device object (dev) representing it */
		devpath = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, devpath);

		/* The device pointed to by dev contains information about
		   the interface. In order to get information about the USB
		   device, get the parent device with the subsystem/devtype pair
		   of "usb"/"usb_device". This will be several levels up the
		   tree, but the function will find it.*/
		parent = udev_device_get_parent_with_subsystem_devtype(dev, "usb",
			"usb_device");
		if (!parent)
			continue;

		devpath = udev_device_get_devnode(parent);
		if (!devpath)
		{
			/* the device disapeared? */
			Log1(PCSC_LOG_ERROR, "udev_device_get_devnode() failed");
			continue;
		}

		driver = get_driver(parent, devpath, &classdriver);
		if (NULL == driver)
			/* no driver known for this device */
			continue;

#ifdef DEBUG_HOTPLUG
		Log2(PCSC_LOG_DEBUG, "Found matching USB device: %s", devpath);
#endif

		newreader = TRUE;
		bInterfaceNumber = 0;
		interface = udev_device_get_sysattr_value(dev, "bInterfaceNumber");
		if (interface)
			bInterfaceNumber = atoi(interface);

		/* Check if the reader is a new one */
		for (j=0; j<PCSCLITE_MAX_READERS_CONTEXTS; j++)
		{
			if (readerTracker[j].devpath
				&& (strcmp(readerTracker[j].devpath, devpath) == 0)
				&& (bInterfaceNumber == readerTracker[j].bInterfaceNumber))
			{
				/* The reader is already known */
				readerTracker[j].status = READER_PRESENT;
				newreader = FALSE;
#ifdef DEBUG_HOTPLUG
				Log2(PCSC_LOG_DEBUG, "Refresh USB device: %s", devpath);
#endif
				break;
			}
		}

		/* New reader found */
		if (newreader)
			HPAddDevice(dev, parent, devpath);

		/* free device */
		udev_device_unref(dev);
	}

	/* Free the enumerator object */
	udev_enumerate_unref(enumerate);

	pthread_mutex_lock(&usbNotifierMutex);
	/* check if all the previously found readers are still present */
	for (i=0; i<PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((READER_ABSENT == readerTracker[i].status)
			&& (readerTracker[i].fullName != NULL))
		{

			Log3(PCSC_LOG_INFO, "Removing USB device[%d]: %s", i,
				readerTracker[i].devpath);

			RFRemoveReader(readerTracker[i].fullName,
				PCSCLITE_HP_BASE_PORT + i);

			readerTracker[i].status = READER_ABSENT;
			free(readerTracker[i].devpath);
			readerTracker[i].devpath = NULL;
			free(readerTracker[i].fullName);
			readerTracker[i].fullName = NULL;

		}
	}
	pthread_mutex_unlock(&usbNotifierMutex);
}

static void HPEstablishUSBNotifications(struct udev *udev)
{
	struct udev_monitor *udev_monitor;
	int r, i;
	int fd;
	fd_set fds;

	udev_monitor = udev_monitor_new_from_netlink(udev, "udev");

	/* filter only the interfaces */
	r = udev_monitor_filter_add_match_subsystem_devtype(udev_monitor, "usb",
		"usb_interface");
	if (r)
	{
		Log2(PCSC_LOG_ERROR, "udev_monitor_filter_add_match_subsystem_devtype() error: %d\n", r);
		return;
	}

	r = udev_monitor_enable_receiving(udev_monitor);
	if (r)
	{
		Log2(PCSC_LOG_ERROR, "udev_monitor_enable_receiving() error: %d\n", r);
		return;
	}

	/* udev monitor file descriptor */
	fd = udev_monitor_get_fd(udev_monitor);

	while (!AraKiriHotPlug)
	{
		struct udev_device *dev, *parent;
		const char *action, *devpath;

#ifdef DEBUG_HOTPLUG
		Log0(PCSC_LOG_INFO);
#endif

		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		/* wait for a udev event */
		r = select(fd+1, &fds, NULL, NULL, NULL);
		if (r < 0)
		{
			Log2(PCSC_LOG_ERROR, "select(): %s", strerror(errno));
			return;
		}

		dev = udev_monitor_receive_device(udev_monitor);
		if (!dev)
		{
			Log1(PCSC_LOG_ERROR, "udev_monitor_receive_device() error\n");
			return;
		}

		action = udev_device_get_action(dev);
		if (0 == strcmp("remove", action))
		{
			Log1(PCSC_LOG_INFO, "Device removed");
			HPRescanUsbBus(udev);
			continue;
		}

		if (strcmp("add", action))
			continue;

		parent = udev_device_get_parent_with_subsystem_devtype(dev, "usb",
			"usb_device");
		devpath = udev_device_get_devnode(parent);
		if (!devpath)
		{
			/* the device disapeared? */
			Log1(PCSC_LOG_ERROR, "udev_device_get_devnode() failed");
			continue;
		}

		HPAddDevice(dev, parent, devpath);

		/* free device */
		udev_device_unref(dev);

	}

	for (i=0; i<driverSize; i++)
	{
		/* free strings allocated by strdup() */
		free(driverTracker[i].bundleName);
		free(driverTracker[i].libraryPath);
		free(driverTracker[i].readerName);
	}
	free(driverTracker);

	Log1(PCSC_LOG_INFO, "Hotplug stopped");
} /* HPEstablishUSBNotifications */


/***
 * Start a thread waiting for hotplug events
 */
LONG HPSearchHotPluggables(void)
{
	int i;

	for (i=0; i<PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		readerTracker[i].status = READER_ABSENT;
		readerTracker[i].bInterfaceNumber = 0;
		readerTracker[i].devpath = NULL;
		readerTracker[i].fullName = NULL;
	}

	return HPReadBundleValues();
} /* HPSearchHotPluggables */


/**
 * Stop the hotplug thread
 */
LONG HPStopHotPluggables(void)
{
	AraKiriHotPlug = TRUE;

	return 0;
} /* HPStopHotPluggables */


/**
 * Sets up callbacks for device hotplug events.
 */
ULONG HPRegisterForHotplugEvents(void)
{
	struct udev *udev;

	(void)pthread_mutex_init(&usbNotifierMutex, NULL);

	if (driverSize <= 0)
	{
		Log1(PCSC_LOG_INFO, "No bundle files in pcsc drivers directory: "
			PCSCLITE_HP_DROPDIR);
		Log1(PCSC_LOG_INFO, "Disabling USB support for pcscd");
		return 0;
	}

	/* Create the udev object */
	udev = udev_new();
	if (!udev)
	{
		Log1(PCSC_LOG_ERROR, "udev_new() failed");
		return 0;
	}

	HPRescanUsbBus(udev);

	(void)ThreadCreate(&usbNotifyThread, THREAD_ATTR_DETACHED,
		(PCSCLITE_THREAD_FUNCTION( )) HPEstablishUSBNotifications, udev);

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

