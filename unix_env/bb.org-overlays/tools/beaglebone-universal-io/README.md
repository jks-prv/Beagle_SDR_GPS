# beaglebone-universal-io

## Overview

Device tree overlay and support scripts for using most available
hardware I/O on the BeagleBone without editing dts files or rebuilding
the kernel.

This project is a series of four overlay files, designed to work with
the BeagleBone Black:

  * cape-universal  Exports all pins not used by HDMIN and eMMC (including audio)
  * cape-universaln Exports all pins not used by HDMI and eMMC (no audio pins are exported)
  * cape-unversalh  Exports all pins not used by eMMC
  * cape-unversala  Exports all pins
  * cape-univ-emmc  Exports pins used by eMMC, load if eMMC is disabled
  * cape-univ-hdmi  Exports pins used by HDMI video, load if HDMI is disabled
  * cape-univ-audio Exports pins used by HDMI audio


## Usage

If using a 3.14 or newer kernel, make sure your device tree inclues the
pinmux helper entries required for the pins you want to use.  If you're
using an RCN built kernel (if you don't know, you probably are), these
should be present by default.

If using a 3.8.13 kernel with capemgr, load the overlay as usual

    echo cape-universaln > /sys/devices/bone_capemgr.*/slots
    
From kernel 4.1 the slots file lives in a new home

    echo cape-universaln > /sys/devices/platform/bone_capemgr/slots

At this point, the various devices are loaded and all gpio have been
exported.  All pins currently default to gpio inputs, with pull up or
pull down resistors set the same as when the AM335x comes out of reset.
This behavior may change, however.  If the BeagleBone begins shipping
with some I/O pins configured by default at boot, this overlay will
likely change to match.

The provided config-pin utility is intended to assist with  setting up
the various pin modes and querying the current pin state.

```
# Configure a pin for a specific mode
config-pin P8.07 timer

# Configure a pin as gpio output and setting the state
config-pin P8.07 hi
config-pin P8.07 low

# Configure a pin as a gpio input
config-pin P8.07 in

# List the valid modes for a specific pin
config-pin -l P8.07

# Query the status of a pin (note the appropriate universal cape
# must be loaded for this to work)
config-pin -q P8.07

# Complete usage details
config-pin -h
```

## GUI 

You can find a Qt based graphical user interface for the
beaglebone-universal-io here: https://github.com/strahlex/BBIOConfig


## Details

If you wish to setup the pins manually, or to know what is happening
behind the curtain, keep reading.

To control the gpio pins, you may use the sysfs interface.  Each
exported gpio pin has an entry under /sys/class/gpio/gpioNN, where NN
represents the kernel gpio number for that pin.  If you have forgotten
the BeagleBone pin to kernel gpio number mapping, you may refer to the
list created for you in the status file of the device tree overlay:

    root@arm:~# cat /sys/devices/ocp.*/cape-universal.*/status
     0 P9_92                    114 IN  0
     1 P9_42                      7 IN  0
     2 P9_91                    116 IN  0
     3 P9_41                     20 IN  0
     4 P9_31                    110 IN  0
     5 P9_30                    112 IN  0
     ...

Pin multiplexing is controled via files in /sys/devices/ocp.*/, where
each exported I/O pin has a pinmux control directory.  The directory is
named using the actual BeagleBone pin header number, so P8_ or P9_ 
followed by the pin number, the suffix _pinmux, and an instance number
(that is subject to change).  So a typical full path to the pinmux
control directory for P8 pin 7 might be:

    /sys/devices/ocp.3/P8_07_pinmux.13/

However since the instance numbers are subject to change, you are
advised to use shell wildcards for the instance values when referencing
these paths:

    /sys/devices/ocp.*/P8_07_pinmux.*/

Each pinmux directory contains a state file, which you can read to
determine the current pinmux setting of the pin:

    root@arm:~# cat /sys/devices/ocp.*/P8_07_pinmux.*/state
    default

Or you can write to change the pinmux setting for the pin:

    root@arm:~# echo timer > /sys/devices/ocp.*/P8_07_pinmux.*/state
    root@arm:~# cat /sys/devices/ocp.*/P8_07_pinmux.*/state
    timer

Each pin has a default state and a gpio state.  Currently the default
state for all pins is gpio with pull-up/down resistor set to the reset
default value, but this could change.  If the BeagleBone begins shipping
with a default I/O setup that enables some special functions, this
overlay will likely change to match.  Most pins have other functions
available besides gpio, see the list below for valid settings.

For a list of valid pinmux states for each pin, use the config-pin
utility:

    config-pin -l <pin>

You will need to reference the AM335x data sheet and Technical Reference
Manual from TI to determine how to setup pin multiplexing for the
various special functions.  I find the following reference quite
helpful, particularly the spreadsheet file:

https://github.com/selsinork/beaglebone-black-pinmux

