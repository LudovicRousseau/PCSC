/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : hotplug_macosx.c
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 10/25/00
	    License: Copyright (C) 2000 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This provides a search API for hot pluggble
	             devices.
	            
********************************************************************/

#include <IOKitLib.h>
#include <IOCFPlugIn.h>
#include <USB.h>
#include <USBSpec.h>
#include <IOUSBLib.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPlugin.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <stdio.h>

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "debuglog.h"
#include "readerfactory.h"
#include "thread_generic.h"
#include "sys_generic.h"
#include "winscard_msg.h"

#define PCSCLITE_HP_DROPDIR                 	"/usr/local/pcsc/drivers/"
#define PCSCLITE_HP_MAX_IDENTICAL_READERS 	16
#define PCSCLITE_HP_MAX_DRIVERS			20

LONG HPAddHotPluggable(int, unsigned long);
LONG HPRemoveHotPluggable(int, unsigned long);
LONG HPSetupHotPlugDevice();
void HPEstablishUSBNotifications();
void RawDeviceAdded(void *, io_iterator_t);
void RawDeviceRemoved(void *, io_iterator_t);

extern PCSCLITE_MUTEX usbNotifierMutex;

static io_iterator_t gRawAddedIter;
static io_iterator_t gRawRemovedIter;
static IONotificationPortRef gNotifyPort;

/*
 * A list to keep track of 20 simultaneous readers 
 */

static struct _bundleTracker
{
	int addrList[PCSCLITE_HP_MAX_IDENTICAL_READERS];
	CFMutableDictionaryRef matchingDict;
}
bundleTracker[PCSCLITE_HP_MAX_DRIVERS];

static mach_port_t masterPort;
static CFURLRef pluginDirURL;
static LONG hpManu_id, hpProd_id;
static CFArrayRef bundleArray;
static int bundleArraySize;
static CFBundleRef currBundle;
static CFDictionaryRef bundleInfoDict;
static PCSCLITE_THREAD_T usbNotifyThread;

void RawDeviceAdded(void *refCon, io_iterator_t iterator)
{

	kern_return_t kr;
	io_service_t obj;

	while (obj = IOIteratorNext(iterator))
	{

		if (refCon == NULL)
		{	/* Dont do this on (void *)1 */
			HPSetupHotPlugDevice();
		}

		kr = IOObjectRelease(obj);
	}

}

void RawDeviceRemoved(void *refCon, io_iterator_t iterator)
{
	kern_return_t kr;
	io_service_t obj;

	while (obj = IOIteratorNext(iterator))
	{

		if (refCon == NULL)
		{
			HPSetupHotPlugDevice();
		}

		kr = IOObjectRelease(obj);
	}
}

void HPEstablishUSBNotifications()
{

	const char *cStringValue;
	CFStringRef propertyString;
	kern_return_t kr;
	CFRunLoopSourceRef runLoopSource;
	int i;

	for (i = 0; i < bundleArraySize; i++)
	{
		currBundle = (CFBundleRef) CFArrayGetValueAtIndex(bundleArray, i);
		bundleInfoDict = CFBundleGetInfoDictionary(currBundle);

		propertyString = CFDictionaryGetValue(bundleInfoDict,
			CFSTR("ifdVendorID"));
		if (propertyString == 0)
		{
			DebugLogA
				("HPEstablishUSBNotifications: No vendor id in bundle.");
			continue;
		}

		cStringValue = CFStringGetCStringPtr(propertyString,
			CFStringGetSystemEncoding());
		hpManu_id = strtoul(cStringValue, 0, 16);

		propertyString = CFDictionaryGetValue(bundleInfoDict,
			CFSTR("ifdProductID"));
		if (propertyString == 0)
		{
			DebugLogA
				("HPEstablishUSBNotifications: No product id in bundle.");
			continue;
		}

		cStringValue = CFStringGetCStringPtr(propertyString,
			CFStringGetSystemEncoding());
		hpProd_id = strtoul(cStringValue, 0, 16);

		// Set up the matching criteria for the devices we're interested
		// in
		bundleTracker[i].matchingDict =
			IOServiceMatching(kIOUSBDeviceClassName);
		if (!bundleTracker[i].matchingDict)
		{
			DebugLogA
				("HPEstablishUSBNotifications: Can't make USBMatch dict.");
			continue;
		}
		// Add our vendor and product IDs to the matching criteria
		CFDictionarySetValue(bundleTracker[i].matchingDict,
			CFSTR(kUSBVendorName),
			CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
				&hpManu_id));
		CFDictionarySetValue(bundleTracker[i].matchingDict,
			CFSTR(kUSBProductName), CFNumberCreate(kCFAllocatorDefault,
				kCFNumberSInt32Type, &hpProd_id));

		// Create a notification port and add its run loop event source to 
		// our run loop
		// This is how async notifications get set up.
		gNotifyPort = IONotificationPortCreate(masterPort);
		runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);

		CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource,
			kCFRunLoopDefaultMode);

		// Retain additional references because we use this same
		// dictionary with four calls to 
		// IOServiceAddMatchingNotification, each of which consumes one
		// reference.
		bundleTracker[i].matchingDict =
			(CFMutableDictionaryRef) CFRetain(bundleTracker[i].
			matchingDict);
		bundleTracker[i].matchingDict =
			(CFMutableDictionaryRef) CFRetain(bundleTracker[i].
			matchingDict);
		bundleTracker[i].matchingDict =
			(CFMutableDictionaryRef) CFRetain(bundleTracker[i].
			matchingDict);

		// Now set up two notifications, one to be called when a raw
		// device is first matched by I/O Kit, and the other to be
		// called when the device is terminated.
		kr = IOServiceAddMatchingNotification(gNotifyPort,
			kIOFirstMatchNotification,
			bundleTracker[i].matchingDict,
			RawDeviceAdded, NULL, &gRawAddedIter);

		/*
		 * The void * 1 allows me to distinguish this initialization
		 * packet from a real event so that I can filter it well 
		 */

		RawDeviceAdded((void *) 1, gRawAddedIter);

		kr = IOServiceAddMatchingNotification(gNotifyPort,
			kIOTerminatedNotification,
			bundleTracker[i].matchingDict,
			RawDeviceRemoved, NULL, &gRawRemovedIter);

		RawDeviceRemoved((void *) 1, gRawRemovedIter);
	}

	CFRunLoopRun();

}

LONG HPSearchHotPluggables()
{

	LONG rv;
	IOReturn iorv;
	CFStringRef pluginDirString;
	int i, j;

	for (i = 0; i < PCSCLITE_HP_MAX_DRIVERS; i++)
	{
		for (j = 0; j < PCSCLITE_HP_MAX_IDENTICAL_READERS; j++)
		{
			bundleTracker[i].addrList[j] = 0;
		}
	}

	iorv = IOMasterPort(bootstrap_port, &masterPort);
	if (iorv != 0)
		return -1;

	pluginDirString = CFStringCreateWithCString(NULL, PCSCLITE_HP_DROPDIR,
		kCFStringEncodingMacRoman);

	if (pluginDirString == NULL)
		return -1;

	pluginDirURL = CFURLCreateWithFileSystemPath(NULL, pluginDirString,
		kCFURLPOSIXPathStyle, TRUE);
	CFRelease(pluginDirString);

	if (pluginDirURL == NULL)
		return -1;

	bundleArray = CFBundleCreateBundlesFromDirectory(NULL, pluginDirURL,
		NULL);

	if (bundleArray == NULL)
		return SCARD_E_NO_MEMORY;

	bundleArraySize = CFArrayGetCount(bundleArray);

	/*
	 * Look for initial USB devices 
	 */
	HPSetupHotPlugDevice();

	rv = SYS_ThreadCreate(&usbNotifyThread, NULL,
		(LPVOID) HPEstablishUSBNotifications, 0);

	return rv;
}

LONG HPSetupHotPlugDevice()
{

	kern_return_t kr;
	io_iterator_t iter = 0;
	io_service_t USBDevice = 0;

	UInt32 usbAddr;
	CFStringRef propertyString;
	CFDictionaryRef USBMatch;
	const char *cStringValue;
	int i, j, k, x;

	UInt32 addrHolder[PCSCLITE_HP_MAX_IDENTICAL_READERS];

	for (j = 0; j < PCSCLITE_HP_MAX_IDENTICAL_READERS; j++)
	{
		addrHolder[j] = 0;
	}

	USBMatch = IOServiceMatching(kIOUSBDeviceClassName);
	if (!USBMatch)
	{
		DebugLogA("HPSearchHotPluggables: Can't make USBMatch dict.");
		return -1;
	}

	/*
	 * create an iterator over all matching IOService nubs 
	 */
	kr = IOServiceGetMatchingServices(masterPort, USBMatch, &iter);
	if (kr)
	{
		DebugLogB("HPSearchHotPluggables: Can't make USBSvc iter: %x.",
			kr);
		if (USBMatch)
			CFRelease(USBMatch);
		return -1;
	}

	for (i = 0; i < bundleArraySize; i++)
	{
		currBundle = (CFBundleRef) CFArrayGetValueAtIndex(bundleArray, i);
		bundleInfoDict = CFBundleGetInfoDictionary(currBundle);
		if (bundleInfoDict == NULL)
		{
			if (USBMatch)
				CFRelease(USBMatch);
			return SCARD_E_NO_MEMORY;
		}

		propertyString = CFDictionaryGetValue(bundleInfoDict,
			CFSTR("ifdVendorID"));
		if (propertyString == 0)
		{
			if (USBMatch)
				CFRelease(USBMatch);
			return -1;
		}

		cStringValue = CFStringGetCStringPtr(propertyString,
			CFStringGetSystemEncoding());
		hpManu_id = strtoul(cStringValue, 0, 16);

		propertyString = CFDictionaryGetValue(bundleInfoDict,
			CFSTR("ifdProductID"));
		if (propertyString == 0)
		{
			if (USBMatch)
				CFRelease(USBMatch);
			return 0;
		}

		cStringValue = CFStringGetCStringPtr(propertyString,
			CFStringGetSystemEncoding());
		hpProd_id = strtoul(cStringValue, 0, 16);

		IOIteratorReset(iter);
		j = 0;

		while ((USBDevice = IOIteratorNext(iter)))
		{
			IOCFPlugInInterface **iodev = NULL;
			IOUSBDeviceInterface **dev = NULL;
			HRESULT res;
			SInt32 score;
			UInt16 vendor;
			UInt16 product;

			kr = IOCreatePlugInInterfaceForService(USBDevice,
				kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID,
				&iodev, &score);
			if (kr || !iodev)
			{
				DebugLogB
					("HPSearchHotPluggables: Can't make plugin intface: %x.",
					kr);
				SYS_Sleep(1);
				kr = IOCreatePlugInInterfaceForService(USBDevice,
					kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID,
					&iodev, &score);
				if (kr || !iodev)
				{
					DebugLogB
						("HPSearchHotPluggables: Can't make plugin intface: %x.",
						kr);

					IOObjectRelease(USBDevice);
					continue;
				}
			}

			IOObjectRelease(USBDevice);

			/*
			 * i have the device plugin. I need the device interface 
			 */
			res =
				(*iodev)->QueryInterface(iodev,
				CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
				(LPVOID) & dev);
			IODestroyPlugInInterface(iodev);

			if (res || !dev)
			{
				DebugLogB
					("HPSearchHotPluggables: Can't query interface: %x.",
					res);
				continue;
			}

			kr = (*dev)->GetDeviceVendor(dev, &vendor);
			kr = (*dev)->GetDeviceProduct(dev, &product);
			kr = (*dev)->GetLocationID(dev, &usbAddr);
			(*dev)->Release(dev);

			if ((vendor == hpManu_id) && (product == hpProd_id))
			{
				addrHolder[j++] = usbAddr;
			}

		}	/* End of while */

		/*
		 * Look through the addresses of devices connected for one's that
		 * we have not seen yet, see if we must add anything 
		 */

		for (k = 0; k < j; k++)
		{
			for (x = 0; x < PCSCLITE_HP_MAX_IDENTICAL_READERS; x++)
			{
				if (addrHolder[k] == bundleTracker[i].addrList[x])
				{
					break;
				}
			}

			if (x == PCSCLITE_HP_MAX_IDENTICAL_READERS)
			{
				/*
				 * Here we will add the reader, since it is not in the
				 * address list 
				 */
				HPAddHotPluggable(i, addrHolder[k]);
			} else
			{
				DebugLogA
					("HPSearchHotPluggables: Warning - reader already in list.");
			}
		}

		/*
		 * Look through the addresses of already connected devices for
		 * one's not seen on the subsystem, see if we must delete anything 
		 */

		for (x = 0; x < PCSCLITE_HP_MAX_IDENTICAL_READERS; x++)
		{
			if (bundleTracker[i].addrList[x] != 0)
			{
				for (k = 0; k < j; k++)
				{
					if (bundleTracker[i].addrList[x] == addrHolder[k])
					{
						break;
					}
				}

				if (k == j)
				{
					/*
					 * Here we will remove the reader, since it is not in
					 * the device address list 
					 */
					HPRemoveHotPluggable(i, bundleTracker[i].addrList[x]);
				}
			}
		}

	}	/* End of for */

	if (iter)
		IOObjectRelease(iter);

	return 0;
}

LONG HPAddHotPluggable(int i, unsigned long usbAddr)
{

	int j;
	LONG rv;
	const char *cStringValue, *cStringLibPath;
	CFURLRef bundPathURL;
	CFStringRef propertyString, cfStringURL;

	propertyString = CFDictionaryGetValue(bundleInfoDict,
		CFSTR("ifdFriendlyName"));
	if (propertyString == 0)
	{
		return -1;
	}

	bundPathURL = CFBundleCopyBundleURL(currBundle);
	cfStringURL = CFURLCopyPath(bundPathURL);
	cStringLibPath = strdup(CFStringGetCStringPtr(cfStringURL,
			CFStringGetSystemEncoding()));
	cStringValue = strdup(CFStringGetCStringPtr(propertyString,
			CFStringGetSystemEncoding()));

	for (j = 0; j < PCSCLITE_HP_MAX_IDENTICAL_READERS; j++)
	{
		if (bundleTracker[i].addrList[j] == 0)
		{
			bundleTracker[i].addrList[j] = usbAddr;
			break;
		}
	}

	if (j == PCSCLITE_HP_MAX_IDENTICAL_READERS)
	{
		/*
		 * We have run out of room 
		 */
		rv = SCARD_E_INSUFFICIENT_BUFFER;
	} else
	{
		SYS_MutexLock(&usbNotifierMutex);
		rv = RFAddReader((LPSTR) cStringValue, 0x200000 + j,
			(LPSTR) cStringLibPath);
		SYS_MutexUnLock(&usbNotifierMutex);
	}

	if (rv != SCARD_S_SUCCESS)
	{
		/*
		 * Function had error so do not keep track of reader 
		 */
		bundleTracker[i].addrList[j] = 0;
	}

	free((LPSTR) cStringValue);
	free((LPSTR) cStringLibPath);

	return rv;
}	/* End of function */

LONG HPRemoveHotPluggable(int i, unsigned long usbAddr)
{

	int j;
	LONG rv;
	const char *cStringValue;
	CFStringRef propertyString;

	propertyString = CFDictionaryGetValue(bundleInfoDict,
		CFSTR("ifdFriendlyName"));
	if (propertyString == 0)
	{
		return -1;
	}

	cStringValue = strdup(CFStringGetCStringPtr(propertyString,
			CFStringGetSystemEncoding()));

	for (j = 0; j < PCSCLITE_HP_MAX_IDENTICAL_READERS; j++)
	{
		if (bundleTracker[i].addrList[j] == usbAddr)
		{
			bundleTracker[i].addrList[j] = 0;
			break;
		}
	}

	SYS_MutexLock(&usbNotifierMutex);
	rv = RFRemoveReader((LPSTR) cStringValue, 0x200000 + j);
	SYS_MutexUnLock(&usbNotifierMutex);

	free((LPSTR) cStringValue);

	return rv;
}	/* End of function */
