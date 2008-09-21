/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2008
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This provides a search API for hot pluggble devices using HAL/DBus
 */

#include "config.h"
#ifdef HAVE_LIBHAL

#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <libhal.h>

#include "misc.h"
#include "wintypes.h"
#include "pcscd.h"
#include "debuglog.h"
#include "parser.h"
#include "readerfactory.h"
#include "sys_generic.h"
#include "hotplug.h"
#include "thread_generic.h"

#undef DEBUG_HOTPLUG
#define ADD_SERIAL_NUMBER

#define FALSE			0
#define TRUE			1

#define UDI_BASE "/org/freedesktop/Hal/devices/"

extern PCSCLITE_MUTEX usbNotifierMutex;

static PCSCLITE_THREAD_T usbNotifyThread;
static int driverSize = -1;
static char AraKiriHotPlug = FALSE;

static DBusConnection *conn;
static LibHalContext *hal_ctx;

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
	int ifdCapabilities;
} *driverTracker = NULL;
#define DRIVER_TRACKER_SIZE_STEP 8

/**
 * keep track of PCSCLITE_MAX_READERS_CONTEXTS simultaneous readers
 */
static struct _readerTracker
{
	char *udi;	/**< device name seen by HAL */
	char *fullName;	/**< full reader name (including serial number) */
} readerTracker[PCSCLITE_MAX_READERS_CONTEXTS];

static LONG HPReadBundleValues(void);
static void HPAddDevice(LibHalContext *ctx, const char *udi);
static void HPRemoveDevice(LibHalContext *ctx, const char *udi);
static void HPEstablishUSBNotifications(void);

/**
 * Generate a short name for a device
 *
 * @param  udi                 Universal Device Id
 */
static const char *short_name(const char *udi)
{
	return &udi[sizeof(UDI_BASE) - 1];
} /* short_name */


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

	if (NULL == hpDir)
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
				if (0 == rv)
					driverTracker[listCount].manuID = strtol(keyValue, NULL, 16);

				/* get ifdProductID */
				rv = LTPBundleFindValueWithKey(fullPath,
					PCSCLITE_HP_PRODKEY_NAME, keyValue, alias);
				if (0 == rv)
					driverTracker[listCount].productID =
						strtol(keyValue, NULL, 16);

				/* get ifdFriendlyName */
				rv = LTPBundleFindValueWithKey(fullPath,
					PCSCLITE_HP_NAMEKEY_NAME, keyValue, alias);
				if (0 == rv)
					driverTracker[listCount].readerName = strdup(keyValue);

				/* get CFBundleExecutable */
				rv = LTPBundleFindValueWithKey(fullPath,
					PCSCLITE_HP_LIBRKEY_NAME, keyValue, 0);
				if (0 == rv)
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
				if (0 == rv)
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

#ifdef DEBUG_HOTPLUG
	Log2(PCSC_LOG_INFO, "Found drivers for %d readers", listCount);
#endif

	return 0;
} /* HPReadBundleValues */


void HPEstablishUSBNotifications(void)
{
	while (!AraKiriHotPlug && dbus_connection_read_write_dispatch(conn, -1))
	{
#ifdef DEBUG_HOTPLUG
		Log0(PCSC_LOG_INFO);
#endif
	}
} /* HPEstablishUSBNotifications */


/***
 * Start a thread waiting for hotplug events
 */
LONG HPSearchHotPluggables(void)
{
	int i;

	for (i=0; i<PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		readerTracker[i].udi = NULL;
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


static struct _driverTracker *get_driver(LibHalContext *ctx, const char *udi)
{
	DBusError error;
	int i;
	unsigned int idVendor, idProduct;

	dbus_error_init(&error);

	if (!libhal_device_property_exists(ctx, udi, "usb.vendor_id", NULL))
		return NULL;

	/* Vendor ID */
	idVendor = libhal_device_get_property_int(ctx, udi,
		"usb.vendor_id", &error);
	if (dbus_error_is_set(&error))
	{
		Log3(PCSC_LOG_ERROR, "libhal_device_get_property_int %s: %d",
			error.name, error.message);
		dbus_error_free(&error);
		return NULL;
	}

	/* Product ID */
	idProduct = libhal_device_get_property_int(ctx, udi,
		"usb.product_id", &error);
	if (dbus_error_is_set(&error))
	{
		Log3(PCSC_LOG_ERROR, "libhal_device_get_property_int %s: %d",
			error.name, error.message);
		dbus_error_free(&error);
		return NULL;
	}

	Log3(PCSC_LOG_DEBUG, "Looking a driver for VID: 0x%04X, PID: 0x%04X", idVendor, idProduct);

	/* check if the device is supported by one driver */
	for (i=0; i<driverSize; i++)
	{
		if (driverTracker[i].libraryPath != NULL &&
			idVendor == driverTracker[i].manuID &&
			idProduct == driverTracker[i].productID)
		{
			return &driverTracker[i];
		}
	}

	return NULL;
}


static void HPAddDevice(LibHalContext *ctx, const char *udi)
{
	int i;
	char deviceName[MAX_DEVICENAME];
	DBusError error;
	struct _driverTracker *driver;
	LONG ret;

	dbus_error_init (&error);

	driver = get_driver(ctx, udi);
	if (NULL == driver)
	{
		/* not a smart card reader */
#ifdef DEBUG_HOTPLUG
		Log2(PCSC_LOG_DEBUG, "%s is not a reader", short_name(udi));
#endif
		return;
	}

	Log2(PCSC_LOG_INFO, "Adding USB device: %s", short_name(udi));

	snprintf(deviceName, sizeof(deviceName), "usb:%04x/%04x:libhal:%s",
		driver->manuID, driver->productID, udi);
	deviceName[sizeof(deviceName) -1] = '\0';

	/* wait until the device is visible by libusb/etc.  */
	SYS_Sleep(1);

	SYS_MutexLock(&usbNotifierMutex);

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
		SYS_MutexUnLock(&usbNotifierMutex);
		return;
	}

	readerTracker[i].udi = strdup(udi);

#ifdef ADD_SERIAL_NUMBER
	if (libhal_device_property_exists(ctx, udi, "usb.serial", &error))
	{
		char fullname[MAX_READERNAME];
		char *sSerialNumber;

		sSerialNumber = libhal_device_get_property_string(ctx, udi,
			"usb.serial", &error);

		snprintf(fullname, sizeof(fullname), "%s (%s)",
			driver->readerName, sSerialNumber);
		readerTracker[i].fullName = strdup(fullname);
	}
	else
#endif
		readerTracker[i].fullName = strdup(driver->readerName);
#ifdef ADD_SERIAL_NUMBER
	if (dbus_error_is_set(&error))
		dbus_error_free(&error);
#endif

	ret = RFAddReader(readerTracker[i].fullName, PCSCLITE_HP_BASE_PORT + i,
		driver->libraryPath, deviceName);
	if (SCARD_S_SUCCESS != ret)
	{
		Log2(PCSC_LOG_ERROR, "Failed adding USB device: %s", short_name(udi));
		free(readerTracker[i].fullName);
		readerTracker[i].fullName = NULL;
		free(readerTracker[i].udi);
		readerTracker[i].udi = NULL;
	}

	SYS_MutexUnLock(&usbNotifierMutex);
} /* HPAddDevice */


static void HPRemoveDevice(LibHalContext *ctx, const char *udi)
{
	int i;

	for (i=0; i<PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if (readerTracker[i].udi && strcmp(readerTracker[i].udi, udi) == 0)
			break;
	}
	if (PCSCLITE_MAX_READERS_CONTEXTS == i)
	{
#ifdef DEBUG_HOTPLUG
		Log2(PCSC_LOG_DEBUG, "USB device %s not already used", short_name(udi));
#endif
		return;
	}
	Log3(PCSC_LOG_INFO, "Removing USB device[%d]: %s", i,
		short_name(readerTracker[i].udi));

	SYS_MutexLock(&usbNotifierMutex);

	RFRemoveReader(readerTracker[i].fullName, PCSCLITE_HP_BASE_PORT + i);
	free(readerTracker[i].fullName);
	readerTracker[i].fullName = NULL;
	free(readerTracker[i].udi);
	readerTracker[i].udi = NULL;

	SYS_MutexUnLock(&usbNotifierMutex);

	return;
} /* HPRemoveDevice */


/**
 * Sets up callbacks for device hotplug events.
 */
ULONG HPRegisterForHotplugEvents(void)
{
	char **device_names;
    int i, num_devices;
	DBusError error;

	if (driverSize <= 0)
	{
		Log1(PCSC_LOG_INFO, "No bundle files in pcsc drivers directory: " PCSCLITE_HP_DROPDIR);
		Log1(PCSC_LOG_INFO, "Disabling USB support for pcscd");
		return 1;
	}

	dbus_error_init(&error);
	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	if (conn == NULL)
	{
		Log3(PCSC_LOG_ERROR, "error: dbus_bus_get: %s: %s",
			error.name, error.message);
		if (dbus_error_is_set(&error))
			dbus_error_free(&error);
		return 1;
	}

	if ((hal_ctx = libhal_ctx_new()) == NULL)
	{
		Log1(PCSC_LOG_ERROR, "error: libhal_ctx_new");
		return 1;
	}
	if (!libhal_ctx_set_dbus_connection(hal_ctx, conn))
	{
		Log1(PCSC_LOG_ERROR, "error: libhal_ctx_set_dbus_connection");
		return 1;
	}
	if (!libhal_ctx_init(hal_ctx, &error))
	{
		if (dbus_error_is_set(&error))
		{
			Log3(PCSC_LOG_ERROR, "error: libhal_ctx_init: %s: %s",
				error.name, error.message);
			if (dbus_error_is_set(&error))
				dbus_error_free(&error);
		}
		Log1(PCSC_LOG_ERROR, "Could not initialise connection to hald.");
		Log1(PCSC_LOG_ERROR, "Normally this means the HAL daemon (hald) is not running or not ready.");
		return 1;
	}

	/* callback when device added */
	libhal_ctx_set_device_added(hal_ctx, HPAddDevice);

	/* callback when device removed */
	libhal_ctx_set_device_removed(hal_ctx, HPRemoveDevice);

	device_names = libhal_get_all_devices(hal_ctx, &num_devices, &error);
	if (device_names == NULL)
	{
		if (dbus_error_is_set(&error))
			dbus_error_free(&error);
		Log1(PCSC_LOG_ERROR, "Couldn't obtain list of devices");
		return 1;
	}

	/* try to add every present USB devices */
	for (i = 0; i < num_devices; i++)
		HPAddDevice(hal_ctx, device_names[i]);

	libhal_free_string_array(device_names);

	SYS_ThreadCreate(&usbNotifyThread, THREAD_ATTR_DETACHED,
		(PCSCLITE_THREAD_FUNCTION( )) HPEstablishUSBNotifications, NULL);

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

