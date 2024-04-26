# KiwiSDR-antenna-switch-extension

This is an antenna switch extension for the KiwiSDR software defined radio.

The antenna switch can control out-of-stock antenna switches and utilize Beaglebone GPIO-pins.
You can also write your own backend for third party antenna switches.

![MS-S7-WEB kit](http://kiwisdr.com/files/ant_switch/MS-S7-WEB.jpg)

## Features

* Basic antenna switching.
* Antenna mixing. In antenna mix mode multiple antennas can be selected simultaneously.
* Admin can lock/unlock antenna switching from admin panel.
* Admin can enable/disable antenna mixing from admin panel.
* Admin can deny antenna switching if more than one user is online.
* Antenna switching can be time scheluded using Beaglebone's crontab.
  * Look at docs/antenna-schedules-using-crontab.txt
* Switching back to default antennas when no users are online.
  * Look at Kiwi admin page, extensions tab: Antenna Switch
* Thunderstorm mode. In thunderstorm mode all antennas are forced to ground and switching is denied.
* The Kiwi frequency scale offset can be set for each antenna selection. Supports transverters or downconverters that are switched together with the antenna.
* Antenna switching can be made from the http URL with a parameter,  
e.g. my\_kiwi:8073/?ant=6 would select antenna #6 and  
my\_kiwi:8073/?ant=6,3 would select antennas #6 and #3. Instead of an antenna number
a substring of the antenna description can be specified. See the extension help button for details.
  
## Required hardware

You will need the KiwiSDR software-defined radio (SDR) kit.

You will need antenna switch hardware.

## Available backends for hardware

* ms-s7-web for controlling the older LZ2RR MS-S7-WEB antenna switch.
    * The version with antenna mixing.
* ms-s[34567]a-web for controlling newer LZ2RR MS-SnA-WEB antenna switches.
    * The version using the MS-V2 WiFi controller with SnA antenna selectors, n=3,4,5,6,7.
* beagle-gpio for controlling up to ten Beaglebone Green/Black/AI/AI-64 GPIO pins.
* snaptekk for controlling Snaptekk WiFi ham radio 8 antenna switch.
* kmtronic for controlling KMTronic LAN Ethernet IP 8 channels WEB Relay.
* kmtronic-udp for controlling KMTronic LAN Ethernet IP 8 channels UDP Relay.
* arduino-netshield for Arduino Nano V3.0 GPIO pins. ENC28J60 Ethernet Shield needed.
* example-backend is an example script for your own backend development.

## Installation

As of Kiwi version v1.665 the code is integrated into the Kiwi server instead
of being a separate Kiwi extension downloaded and installed from the Internet.

## Configuration

If you had a switch device previously setup the configuration will carryover.
See the <x1>Antenna Switch</x1> entry on the admin page, extensions tab
(same location as previously). The switch device can be selected from a menu here
as well as any IP address or URL the device requires.

Open your KiwiSDR admin panel. Then Extensions -> Antenna Switch.

![ant switch extenstion admin interface](http://kiwisdr.com/files/ant_switch/admin.jpg)

By default users can switch antennas and select multiple simultaneous antennas.

Describe your antennas 1-8 (1-10 if using the Beagle GPIO backend). If you leave antenna description empty, antenna button won't be visible to users.

Antenna switch failure or unknown status decription will be show to users if antenna switch is unreachable or malfunctioning. 

## Usage

Open your KiwiSDR as user. In the main control panel at lower right select the "RF" tab. Scroll down to the Antenna switch section. You can also select "ant_switch" in the extension menu.

![ant_switch_extension_user_interface](http://kiwisdr.com/files/ant_switch/user.jpg)

Single antenna mode: Click to select antenna. 

Antenna mixing mode: you can select multiple antennas simultaneously. Click antennas on/off. 

If admin has disable antenna switching, buttons are grey and you cannot click them.

## Demo site

KiwiSDR Kaustinen http://sdr.vy.fi

## Donate
If you want to support this project, you can [send a donation via PayPal](https://www.paypal.me/oh1kk).

## License

[The MIT License (MIT)](LICENSE)

Copyright (c) 2019-2024 Kari Karvonen
