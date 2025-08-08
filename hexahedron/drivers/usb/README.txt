=-=-=-=-=-=-=-=-=-=-=-=-=-=-= HEXAHEDRON USB STACK =-=-=-=-=-=-=-=-=-=-=-=-=-=-=

The Hexahedron USB stack is a sometimes confusing and weird framework.
It allows you to write drivers for host controllers or USB devices, whatever you please.

==== I. Overview

USB controllers contain almost no valuable information for USB drivers except for internal USB systems and host controller drivers.
They are exposed in USBController_t, and passed to every usual host controller driver
However, they do contain a list of up to 127 (the max amount of devices currently) devices - you can access these through API functions

Devices are given out in the structure USBDevice_t. This structure is LARGE. It contains a list of configurations, interfaces, endpoints,
as well as basic information about the device itself.

Device drivers themselves will actually work with USB interfaces. One drievr will have 

==== II. Writing a USB driver

When writing a USB driver, the first thing you want to do is register your driver.
Create a new driver by calling usb_createDriver and then call usb_registerDriver to register your driver.

Then you need to find your device. This is more complex than it seems due to Hexahedron's driver-focused nature.
Hexahedron also employs a sort of hands-off approach when it comes to requiring drivers to be loaded before. Asyncronous is always best in my philosophy.
However, the USB stack is forced to employ a syncronous/asyncronous approach, as the HC driver containing another driver's device could be loaded before or after.

The weight of locating your device is taken off your shoulders - usb_registerDriver will place your driver in the main list, 
and every time a new device is initialized, it will cycle through the existing list to find a proper driver. You can employ find parameters,
which is discussed later. As well as that, usb_registerDriver will also scan through all controllers, their devices, and those devices' interfaces,
and call your driver's dev_init method on each of them (just return USB_FAILURE if the device is invalid)

NOTE:   If a driver was already registered to a device, then depending on your driver's setting for weak bind (which by default is 0),
        it may be unloaded from its existing driver and then loaded onto the new driver. Please set your driver as a weak bind if it is a generic driver.

NOTE2:  There is a small bit of priority besides weak binds. Providing find parameters will cause a higher tendency for your driver to get the device

This approach can be slow, so timeouts can be used, and if the driver wants to wait for multithreading you can just sleep until one is plugged in.

==== III. Writing a host controller driver

Easy. Register your controller with usb_registerController and probe your device for any ports.
When you find a device, use usb_createDevice to create a new USBDevice_t structure, and then use usb_initializeDevice to initialize the device
and select a configuration.

==== IV. Using this stack as an inspiration

I'd highly recommend you don't. Some of the code is kind of messy and strewn together, using externs to avoid having weird header calls.
I've done my very best to clean up the exposed API, however, so if you want you should actually just implement the API functions yourself.
