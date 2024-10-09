/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2002-2004
 *  Stephen M. Webb <stephenw@cryptocard.com>
 * Copyright (C) 2002-2023
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 * Copyright (C) 2002
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2003
 *  Antti Tapaninen
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
 * @brief This provides a search API for hot pluggble devices.
 */

#include "config.h"
#include "misc.h"
#include "pcscd.h"

#if defined(__APPLE__) && !defined(HAVE_LIBUSB)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <stdlib.h>
#include <string.h>

#include "debuglog.h"
#include "parser.h"
#include "readerfactory.h"
#include "winscard_msg.h"
#include "utils.h"
#include "hotplug.h"

#undef DEBUG_HOTPLUG

/*
 * An aggregation of useful information on a driver bundle in the
 * drop directory.
 */
typedef struct HPDriver
{
	UInt32 m_vendorId;			/* unique vendor's manufacturer code */
	UInt32 m_productId;			/* manufacturer's unique product code */
	char *m_friendlyName;		/* bundle friendly name */
	char *m_libPath;			/* bundle's plugin library location */
} HPDriver, *HPDriverVector;

/*
 * An aggregation on information on currently active reader drivers.
 */
typedef struct HPDevice
{
	HPDriver *m_driver;			/* driver bundle information */
	UInt32 m_address;			/* unique system address of device */
	struct HPDevice *m_next;	/* next device in list */
} HPDevice, *HPDeviceList;

/*
 * Pointer to a list of (currently) known hotplug reader devices (and their
 * drivers).
 */
static HPDeviceList sDeviceList = NULL;

static int HPScan(void);
static HPDriver *Drivers = NULL;

/*
 * A callback to handle the asynchronous appearance of new devices that are
 * candidates for PCSC readers.
 */
static void HPDeviceAppeared(void *refCon, io_iterator_t iterator)
{
	io_service_t obj;

	(void)refCon;

	while ((obj = IOIteratorNext(iterator)))
		IOObjectRelease(obj);

	HPScan();
}

/*
 * A callback to handle the asynchronous disappearance of devices that are
 * possibly PCSC readers.
 */
static void HPDeviceDisappeared(void *refCon, io_iterator_t iterator)
{
	io_service_t obj;

	(void)refCon;

	while ((obj = IOIteratorNext(iterator)))
		IOObjectRelease(obj);

	HPScan();
}


/*
 * Creates a vector of driver bundle info structures from the hot-plug driver
 * directory.
 *
 * Returns NULL on error and a pointer to an allocated HPDriver vector on
 * success.  The caller must free the HPDriver with a call to
 * HPDriversRelease().
 */
static HPDriverVector HPDriversGetFromDirectory(const char *driverBundlePath)
{
#ifdef DEBUG_HOTPLUG
	Log2(PCSC_LOG_DEBUG, "Entering HPDriversGetFromDirectory: %s",
		driverBundlePath);
#endif

	int readersNumber = 0;
	HPDriverVector bundleVector = NULL;
	CFArrayRef bundleArray;
	CFStringRef driverBundlePathString =
		CFStringCreateWithCString(kCFAllocatorDefault,
		driverBundlePath,
		kCFStringEncodingMacRoman);
	CFURLRef pluginUrl = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
		driverBundlePathString,
		kCFURLPOSIXPathStyle, TRUE);

	CFRelease(driverBundlePathString);
	if (!pluginUrl)
	{
		Log1(PCSC_LOG_ERROR, "error getting plugin directory URL");
		return NULL;
	}
	bundleArray = CFBundleCreateBundlesFromDirectory(kCFAllocatorDefault,
		pluginUrl, NULL);
	CFRelease(pluginUrl);
	if (!bundleArray)
	{
		Log1(PCSC_LOG_ERROR, "error getting plugin directory bundles");
		return NULL;
	}

	size_t bundleArraySize = CFArrayGetCount(bundleArray);
	size_t i;

	/* get the number of readers (including aliases) */
	for (i = 0; i < bundleArraySize; i++)
	{
		CFBundleRef currBundle =
			(CFBundleRef) CFArrayGetValueAtIndex(bundleArray, i);
		CFDictionaryRef dict = CFBundleGetInfoDictionary(currBundle);

		const void * blobValue = CFDictionaryGetValue(dict,
			CFSTR(PCSCLITE_HP_MANUKEY_NAME));

		if (!blobValue)
		{
			Log1(PCSC_LOG_ERROR, "error getting vendor ID from bundle");
			CFRelease(bundleArray);
			return NULL;
		}

		if (CFGetTypeID(blobValue) == CFArrayGetTypeID())
		{
			/* alias found, each reader count as 1 */
			CFArrayRef propertyArray = blobValue;
			readersNumber += CFArrayGetCount(propertyArray);
		}
		else
			/* No alias, only one reader supported */
			readersNumber++;
	}
#ifdef DEBUG_HOTPLUG
	Log2(PCSC_LOG_DEBUG, "Total of %d readers supported", readersNumber);
#endif

	/* The last entry is an end marker (m_vendorId = 0)
	 * see checks in HPDriversMatchUSBDevices:503
	 *  and HPDriverVectorRelease:376 */
	readersNumber++;

	bundleVector = calloc(readersNumber, sizeof(HPDriver));
	if (!bundleVector)
	{
		Log1(PCSC_LOG_ERROR, "memory allocation failure");
		CFRelease(bundleArray);
		return NULL;
	}

	HPDriver *driverBundle = bundleVector;
	for (i = 0; i < bundleArraySize; i++)
	{
		CFBundleRef currBundle =
			(CFBundleRef) CFArrayGetValueAtIndex(bundleArray, i);
		CFDictionaryRef dict = CFBundleGetInfoDictionary(currBundle);

		CFURLRef bundleUrl = CFBundleCopyBundleURL(currBundle);
		CFStringRef bundlePath = CFURLCopyPath(bundleUrl);
		CFRelease(bundleUrl);

		driverBundle->m_libPath = strdup(CFStringGetCStringPtr(bundlePath,
				CFStringGetSystemEncoding()));
		CFRelease(bundlePath);

		const void * blobValue = CFDictionaryGetValue(dict,
			CFSTR(PCSCLITE_HP_MANUKEY_NAME));

		if (!blobValue)
		{
			Log1(PCSC_LOG_ERROR, "error getting vendor ID from bundle");
			CFRelease(bundleArray);
			return bundleVector;
		}

		if (CFGetTypeID(blobValue) == CFArrayGetTypeID())
		{
			CFArrayRef vendorArray = blobValue;
			CFArrayRef productArray;
			CFArrayRef friendlyNameArray;
			char *libPath = driverBundle->m_libPath;

#ifdef DEBUG_HOTPLUG
			Log2(PCSC_LOG_DEBUG, "Driver with aliases: %s", libPath);
#endif
			/* get list of ProductID */
			productArray = CFDictionaryGetValue(dict,
				 CFSTR(PCSCLITE_HP_PRODKEY_NAME));
			if (!productArray)
			{
				Log1(PCSC_LOG_ERROR, "error getting product ID from bundle");
				CFRelease(bundleArray);
				return bundleVector;
			}

			/* get list of FriendlyName */
			friendlyNameArray = CFDictionaryGetValue(dict,
				 CFSTR(PCSCLITE_HP_NAMEKEY_NAME));
			if (!friendlyNameArray)
			{
				Log1(PCSC_LOG_ERROR, "error getting product ID from bundle");
				CFRelease(bundleArray);
				return bundleVector;
			}

			long reader_nb = CFArrayGetCount(vendorArray);

			if (reader_nb != CFArrayGetCount(productArray))
			{
				Log3(PCSC_LOG_ERROR,
					"Malformed Info.plist: %ld vendors and %ld products",
					reader_nb, CFArrayGetCount(productArray));
				CFRelease(bundleArray);
				return bundleVector;
			}

			if (reader_nb != CFArrayGetCount(friendlyNameArray))
			{
				Log3(PCSC_LOG_ERROR,
					"Malformed Info.plist: %ld vendors and %ld friendlynames",
					reader_nb, CFArrayGetCount(friendlyNameArray));
				CFRelease(bundleArray);
				return bundleVector;
			}

			int j;
			for (j=0; j<reader_nb; j++)
			{
				char stringBuffer[1000];
				CFStringRef strValue = CFArrayGetValueAtIndex(vendorArray, j);

				CFStringGetCString(strValue, stringBuffer, sizeof stringBuffer,
					kCFStringEncodingUTF8);
				driverBundle->m_vendorId = (unsigned int)strtoul(stringBuffer, NULL, 16);

				strValue = CFArrayGetValueAtIndex(productArray, j);
				CFStringGetCString(strValue, stringBuffer, sizeof stringBuffer,
					kCFStringEncodingUTF8);
				driverBundle->m_productId = (unsigned int)strtoul(stringBuffer, NULL, 16);

				strValue = CFArrayGetValueAtIndex(friendlyNameArray, j);
				CFStringGetCString(strValue, stringBuffer, sizeof stringBuffer,
					kCFStringEncodingUTF8);
				driverBundle->m_friendlyName = strdup(stringBuffer);

				if (!driverBundle->m_libPath)
					driverBundle->m_libPath = strdup(libPath);

#ifdef DEBUG_HOTPLUG
				Log2(PCSC_LOG_DEBUG, "VendorID: 0x%04X",
					driverBundle->m_vendorId);
				Log2(PCSC_LOG_DEBUG, "ProductID: 0x%04X",
					driverBundle->m_productId);
				Log2(PCSC_LOG_DEBUG, "Friendly name: %s",
					driverBundle->m_friendlyName);
				Log2(PCSC_LOG_DEBUG, "Driver: %s", driverBundle->m_libPath);
#endif

				/* go to next bundle in the vector */
				driverBundle++;
			}
		}
		else
		{
			Log1(PCSC_LOG_ERROR, "Non array not supported");
		}
	}
	CFRelease(bundleArray);
	return bundleVector;
}

/*
 * Copies a driver bundle instance.
 */
static HPDriver *HPDriverCopy(HPDriver * rhs)
{
	if (!rhs)
		return NULL;

	HPDriver *newDriverBundle = calloc(1, sizeof(HPDriver));

	if (!newDriverBundle)
		return NULL;

	newDriverBundle->m_vendorId = rhs->m_vendorId;
	newDriverBundle->m_productId = rhs->m_productId;
	newDriverBundle->m_friendlyName = strdup(rhs->m_friendlyName);
	newDriverBundle->m_libPath = strdup(rhs->m_libPath);

	return newDriverBundle;
}

/*
 * Releases resources allocated to a driver bundle vector.
 */
static void HPDriverRelease(HPDriver * driverBundle)
{
	if (driverBundle)
	{
		free(driverBundle->m_friendlyName);
		free(driverBundle->m_libPath);
	}
}

/*
 * Inserts a new reader device in the list.
 */
static HPDeviceList
HPDeviceListInsert(HPDeviceList list, HPDriver * bundle, UInt32 address)
{
	HPDevice *newReader = calloc(1, sizeof(HPDevice));

	if (!newReader)
	{
		Log1(PCSC_LOG_ERROR, "memory allocation failure");
		return list;
	}

	newReader->m_driver = HPDriverCopy(bundle);
	newReader->m_address = address;
	newReader->m_next = list;

	return newReader;
}

/*
 * Frees resources allocated to a HPDeviceList.
 */
static void HPDeviceListRelease(HPDeviceList list)
{
	HPDevice *p;

	for (p = list; p; p = p->m_next)
		HPDriverRelease(p->m_driver);
}

/*
 * Compares two driver bundle instances for equality.
 */
static int HPDeviceEquals(HPDevice * a, HPDevice * b)
{
	return (a->m_driver->m_vendorId == b->m_driver->m_vendorId)
		&& (a->m_driver->m_productId == b->m_driver->m_productId)
		&& (a->m_address == b->m_address);
}

/*
 * Finds USB devices currently registered in the system that match any of
 * the drivers detected in the driver bundle vector.
 */
static int
HPDriversMatchUSBDevices(HPDriverVector driverBundle,
	HPDeviceList * readerList)
{
	CFDictionaryRef usbMatch = IOServiceMatching("IOUSBDevice");

	if (0 == usbMatch)
	{
		Log1(PCSC_LOG_ERROR,
			"error getting USB match from IOServiceMatching()");
		return 1;
	}

	io_iterator_t usbIter;
	kern_return_t kret = IOServiceGetMatchingServices(kIOMainPortDefault,
		usbMatch, &usbIter);

	if (kret != 0)
	{
		Log1(PCSC_LOG_ERROR,
			"error getting iterator from IOServiceGetMatchingServices()");
		return 1;
	}

	IOIteratorReset(usbIter);
	io_object_t usbDevice = 0;

	while ((usbDevice = IOIteratorNext(usbIter)))
	{
		char namebuf[1024];

		kret = IORegistryEntryGetName(usbDevice, namebuf);
		if (kret != 0)
		{
			Log1(PCSC_LOG_ERROR,
				"error getting device name from IORegistryEntryGetName()");
			return 1;
		}

		IOCFPlugInInterface **iodev;
		SInt32 score;

		kret = IOCreatePlugInInterfaceForService(usbDevice,
			kIOUSBDeviceUserClientTypeID,
			kIOCFPlugInInterfaceID, &iodev, &score);
		if (kret != 0)
		{
			Log1(PCSC_LOG_ERROR, "error getting plugin interface from IOCreatePlugInInterfaceForService()");
			return 1;
		}
		IOObjectRelease(usbDevice);

		IOUSBDeviceInterface **usbdev;
		HRESULT hres = (*iodev)->QueryInterface(iodev,
			CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
			(LPVOID *) & usbdev);

		(*iodev)->Release(iodev);
		if (hres)
		{
			Log1(PCSC_LOG_ERROR,
				"error querying interface in QueryInterface()");
			return 1;
		}

		UInt16 vendorId = 0;
		UInt16 productId = 0;
		UInt32 usbAddress = 0;

		kret = (*usbdev)->GetDeviceVendor(usbdev, &vendorId);
		kret = (*usbdev)->GetDeviceProduct(usbdev, &productId);
		kret = (*usbdev)->GetLocationID(usbdev, &usbAddress);
		(*usbdev)->Release(usbdev);

#ifdef DEBUG_HOTPLUG
		Log4(PCSC_LOG_DEBUG, "Found USB device 0x%04X:0x%04X at 0x%X",
			vendorId, productId, usbAddress);
#endif
		HPDriver *driver;
		for (driver = driverBundle; driver->m_vendorId; ++driver)
		{
			if ((driver->m_vendorId == vendorId)
				&& (driver->m_productId == productId))
			{
#ifdef DEBUG_HOTPLUG
				Log4(PCSC_LOG_DEBUG, "Adding USB device %04X:%04X at 0x%X",
					vendorId, productId, usbAddress);
#endif
				*readerList =
					HPDeviceListInsert(*readerList, driver, usbAddress);
			}
		}
	}

	IOObjectRelease(usbIter);
	return 0;
}

/*
 * Finds PC Card devices currently registered in the system that match any of
 * the drivers detected in the driver bundle vector.
 */
static int
HPDriversMatchPCCardDevices(HPDriver * driverBundle,
	HPDeviceList * readerList)
{
	CFDictionaryRef pccMatch = IOServiceMatching("IOPCCard16Device");

	if (pccMatch == NULL)
	{
		Log1(PCSC_LOG_ERROR,
			"error getting PCCard match from IOServiceMatching()");
		return 1;
	}

	io_iterator_t pccIter;
	kern_return_t kret =
		IOServiceGetMatchingServices(kIOMainPortDefault, pccMatch,
		&pccIter);
	if (kret != 0)
	{
		Log1(PCSC_LOG_ERROR,
			"error getting iterator from IOServiceGetMatchingServices()");
		return 1;
	}

	IOIteratorReset(pccIter);
	io_object_t pccDevice = 0;

	while ((pccDevice = IOIteratorNext(pccIter)))
	{
		char namebuf[1024];

		kret = IORegistryEntryGetName(pccDevice, namebuf);
		if (kret != 0)
		{
			Log1(PCSC_LOG_ERROR, "error getting plugin interface from IOCreatePlugInInterfaceForService()");
			return 1;
		}
		UInt32 vendorId = 0;
		UInt32 productId = 0;
		UInt32 pccAddress = 0;
		CFTypeRef valueRef =
			IORegistryEntryCreateCFProperty(pccDevice, CFSTR("VendorID"),
			kCFAllocatorDefault, 0);

		if (!valueRef)
		{
			Log1(PCSC_LOG_ERROR, "error getting vendor");
		}
		else
		{
			CFNumberGetValue((CFNumberRef) valueRef, kCFNumberSInt32Type,
				&vendorId);
			CFRelease(valueRef);
		}
		valueRef =
			IORegistryEntryCreateCFProperty(pccDevice, CFSTR("DeviceID"),
			kCFAllocatorDefault, 0);
		if (!valueRef)
		{
			Log1(PCSC_LOG_ERROR, "error getting device");
		}
		else
		{
			CFNumberGetValue((CFNumberRef) valueRef, kCFNumberSInt32Type,
				&productId);
			CFRelease(valueRef);
		}
		valueRef =
			IORegistryEntryCreateCFProperty(pccDevice, CFSTR("SocketNumber"),
			kCFAllocatorDefault, 0);
		if (!valueRef)
		{
			Log1(PCSC_LOG_ERROR, "error getting PC Card socket");
		}
		else
		{
			CFNumberGetValue((CFNumberRef) valueRef, kCFNumberSInt32Type,
				&pccAddress);
			CFRelease(valueRef);
		}
		HPDriver *driver = driverBundle;

		for (; driver->m_vendorId; ++driver)
		{
			if ((driver->m_vendorId == vendorId)
				&& (driver->m_productId == productId))
			{
				*readerList =
					HPDeviceListInsert(*readerList, driver, pccAddress);
			}
		}
	}
	IOObjectRelease(pccIter);
	return 0;
}


static void HPEstablishUSBNotification(void)
{
	io_iterator_t deviceAddedIterator;
	io_iterator_t deviceRemovedIterator;
	CFMutableDictionaryRef matchingDictionary;
	IONotificationPortRef notificationPort;
	IOReturn kret;

	notificationPort = IONotificationPortCreate(kIOMainPortDefault);
	CFRunLoopAddSource(CFRunLoopGetCurrent(),
		IONotificationPortGetRunLoopSource(notificationPort),
		kCFRunLoopDefaultMode);

	matchingDictionary = IOServiceMatching("IOUSBDevice");
	if (!matchingDictionary)
	{
		Log1(PCSC_LOG_ERROR, "IOServiceMatching() failed");
		return;
	}
	matchingDictionary =
		(CFMutableDictionaryRef) CFRetain(matchingDictionary);

	kret = IOServiceAddMatchingNotification(notificationPort,
		kIOMatchedNotification,
		matchingDictionary, HPDeviceAppeared, NULL, &deviceAddedIterator);
	if (kret)
	{
		Log2(PCSC_LOG_ERROR,
			"IOServiceAddMatchingNotification()-1 failed with code %d", kret);
	}
	HPDeviceAppeared(NULL, deviceAddedIterator);

	kret = IOServiceAddMatchingNotification(notificationPort,
		kIOTerminatedNotification,
		matchingDictionary,
		HPDeviceDisappeared, NULL, &deviceRemovedIterator);
	if (kret)
	{
		Log2(PCSC_LOG_ERROR,
			"IOServiceAddMatchingNotification()-2 failed with code %d", kret);
	}
	HPDeviceDisappeared(NULL, deviceRemovedIterator);
}

static void HPEstablishPCCardNotification(void)
{
	io_iterator_t deviceAddedIterator;
	io_iterator_t deviceRemovedIterator;
	CFMutableDictionaryRef matchingDictionary;
	IONotificationPortRef notificationPort;
	IOReturn kret;

	notificationPort = IONotificationPortCreate(kIOMainPortDefault);
	CFRunLoopAddSource(CFRunLoopGetCurrent(),
		IONotificationPortGetRunLoopSource(notificationPort),
		kCFRunLoopDefaultMode);

	matchingDictionary = IOServiceMatching("IOPCCard16Device");
	if (!matchingDictionary)
	{
		Log1(PCSC_LOG_ERROR, "IOServiceMatching() failed");
		return;
	}
	matchingDictionary =
		(CFMutableDictionaryRef) CFRetain(matchingDictionary);

	kret = IOServiceAddMatchingNotification(notificationPort,
		kIOMatchedNotification,
		matchingDictionary, HPDeviceAppeared, NULL, &deviceAddedIterator);
	if (kret)
	{
		Log2(PCSC_LOG_ERROR,
			"IOServiceAddMatchingNotification()-1 failed with code %d", kret);
	}
	HPDeviceAppeared(NULL, deviceAddedIterator);

	kret = IOServiceAddMatchingNotification(notificationPort,
		kIOTerminatedNotification,
		matchingDictionary,
		HPDeviceDisappeared, NULL, &deviceRemovedIterator);
	if (kret)
	{
		Log2(PCSC_LOG_ERROR,
			"IOServiceAddMatchingNotification()-2 failed with code %d", kret);
	}
	HPDeviceDisappeared(NULL, deviceRemovedIterator);
}

/*
 * Thread runner (does not return).
 */
static void HPDeviceNotificationThread(void)
{
	HPEstablishUSBNotification();
	HPEstablishPCCardNotification();
	CFRunLoopRun();
}

/*
 * Scans the hotplug driver directory and looks in the system for
 * matching devices.
 * Adds or removes matching readers as necessary.
 */
LONG HPSearchHotPluggables(const char * hpDirPath)
{
	Drivers = HPDriversGetFromDirectory(hpDirPath);

	if (!Drivers)
		return 1;

	return 0;
}

static int HPScan(void)
{
	HPDeviceList devices = NULL;

	if (HPDriversMatchUSBDevices(Drivers, &devices))
	{
		if (devices)
			free(devices);

		return -1;
	}

	if (HPDriversMatchPCCardDevices(Drivers, &devices))
	{
		if (devices)
			free(devices);

		return -1;
	}

	HPDevice *a;

	for (a = devices; a; a = a->m_next)
	{
		bool found = false;
		HPDevice *b;

		for (b = sDeviceList; b; b = b->m_next)
		{
			if (HPDeviceEquals(a, b))
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			char *deviceName;

			/* the format should be "usb:%04x/%04x" but Apple uses the
			 * friendly name instead */
			asprintf(&deviceName, "%s", a->m_driver->m_friendlyName);

			RFAddReader(a->m_driver->m_friendlyName,
				PCSCLITE_HP_BASE_PORT + a->m_address, a->m_driver->m_libPath,
				deviceName);
			free(deviceName);
		}
	}

	for (a = sDeviceList; a; a = a->m_next)
	{
		bool found = false;
		HPDevice *b;

		for (b = devices; b; b = b->m_next)
		{
			if (HPDeviceEquals(a, b))
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			RFRemoveReader(a->m_driver->m_friendlyName,
				PCSCLITE_HP_BASE_PORT + a->m_address,
				REMOVE_READER_FLAG_REMOVED);
		}
	}

	HPDeviceListRelease(sDeviceList);
	sDeviceList = devices;

	return 0;
}


pthread_t sHotplugWatcherThread;

/*
 * Sets up callbacks for device hotplug events.
 */
ULONG HPRegisterForHotplugEvents(const char * hpDirPath)
{
	(void)hpDirPath;

	ThreadCreate(&sHotplugWatcherThread,
		THREAD_ATTR_DEFAULT,
		(PCSCLITE_THREAD_FUNCTION( )) HPDeviceNotificationThread, NULL);

	return 0;
}

LONG HPStopHotPluggables(void)
{
	return 0;
}

void HPReCheckSerialReaders(void)
{
}

#endif	/* __APPLE__ */

