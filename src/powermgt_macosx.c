/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "config.h"
#include <ctype.h>

#ifdef __APPLE__
#include <stdlib.h>
#include <stdio.h>

#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

#include "pcsclite.h"
#include "debuglog.h"
#include "readerfactory.h"
#include "thread_generic.h"
#include "hotplug.h"

static io_connect_t		root_port;
static IONotificationPortRef	notify;
static io_object_t 		anIterator;

PCSCLITE_THREAD_T       pmgmtThread;
extern PCSCLITE_MUTEX   usbNotifierMutex;

void PMPowerRegistrationThread();

void PMPowerEventCallback(void * x,io_service_t y,natural_t messageType,void * messageArgument)
{

    switch ( messageType ) {
    case kIOMessageCanSystemSleep:
          IOAllowPowerChange(root_port,(long)messageArgument);
          break;
    case kIOMessageSystemWillSleep:
          DebugLogA("system going into sleep");
          SYS_MutexLock(&usbNotifierMutex);
          RFSuspendAllReaders();
          IOAllowPowerChange(root_port,(long)messageArgument);
          DebugLogA("system allowed to sleep");
          break;
    case kIOMessageSystemHasPoweredOn: 
        DebugLogA("system coming out of sleep");
        HPSearchHotPluggables();
        RFAwakeAllReaders();
        SYS_MutexUnLock(&usbNotifierMutex);
        break;
    }
    
}

void PMPowerRegistrationThread() {

    root_port = IORegisterForSystemPower (0,&notify,PMPowerEventCallback,&anIterator);
  
    if ( root_port == NULL ) {
            DebugLogA("IORegisterForSystemPower failed");
            return;
    }
    
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                        IONotificationPortGetRunLoopSource(notify),
                        kCFRunLoopDefaultMode);
                
    CFRunLoopRun();
}

ULONG PMRegisterForPowerEvents() {

  LONG rv; 
    
  rv = SYS_ThreadCreate(&pmgmtThread, NULL,
                        (LPVOID) PMPowerRegistrationThread, NULL);
  return 0;
}

#endif	/* __APPLE__ */
