[![KiwiSDR](http://www.jks.com/KiwiSDR/pcb.1.780px.jpg)](http://www.jks.com/KiwiSDR/pcb.1.jpg)

Click image for full size.
##### Yes, the assembly house _did_ solder those headers on the wrong side of the board.

[![KiwiSDR](http://www.jks.com/KiwiSDR/kiwi-with-headphones.130x170.png)© bluebison.net](http://bluebison.net)

KiwiSDR
=======

Software-defined Radio (SDR) and GPS for the BeagleBone Black
----------------------------------------------------------------------

An add-on board ("cape") that turns your Beagle into a web-accessible shortwave receiver.

### Details

* [Listen live](http://www.jks.com:8073) [jks.com:8073]
* [Design review document](http://www.jks.com/docs/KiwiSDR/KiwiSDR.design.review.pdf)
* [Project status & history](http://www.jks.com/KiwiSDR/)

### Description
This SDR is a bit different. It has a web interface that can be used by up to four separate listeners. Each one listening and tuning an independent frequency simultaneously. See the screenshots below.

### Components
* SDR covering the 10 KHz to 30 MHz (VLF-HF) spectrum.
* Web interface based on [OpenWebRX](http://openwebrx.org/) from András Retzler, HA7ILM.
* Integrated software-defined GPS receiver from Andrew Holme's [Homemade GPS Receiver](http://www.aholme.co.uk/GPS/Main.htm).
* LTC 14-bit 65 MHz ADC.
* Xilinx Artix-7 A35 FPGA.
* Skyworks SE4150L GPS front-end.

### Features
* 100% Open Source / Open Hardware.
* Includes VLF-HF active antenna and associated power injector PCBs.
* Browser-based interface allowing multiple simultaneous user web connections (currently 4).
* Each connection tunes an independent receiver channel over the entire spectrum.
* Waterfall tunes independently of audio and includes zooming and panning.
* Multi-channel, parallel DDC design using bit-width optimized CIC filters.
* Good performance at VLF/LF since I personally spend time monitoring those frequencies.
* Automatic frequency calibration via received GPS timing.
* Easy hardware and software setup. Browser-based configuration interface.

### Status

Give the live receiver a try at the link above. You'll need a recent version of a modern web browser that supports HTML5. The web interface works, with lots of problems, on iPad. But there is no mobile version of the interface yet.

The files of a KiCAD PCB layout of the cape are included. This PCB has been fabricated in prototype quantities. Please email me any comments you have after reviewing the design document above (I need all the advice I can get). The active antenna and injector PCBs have been fabricated and are undergoing development.

### Objectives

I wanted to design an SDR that provides certain features, at a low price point, that I felt wasn't covered by current devices. The SDR must be web-accessible and simple to setup and use.

I also want to provide a self-contained platform for experimentation with SDR and GPS techniques. In this respect the project has a lot in common with the recent
[HackRF](https://www.kickstarter.com/projects/mossmann/hackrf-an-open-source-sdr-platform) and [BladeRF](https://www.kickstarter.com/projects/1085541682/bladerf-usb-30-software-defined-radio) Kickstarter projects.

Most importantly, I'd really like to see a significant number of web-enabled, wide-band SDRs deployed in diverse locations world-wide because that makes possible some really interesting applications and experiments.

### Operation

The user will be required to purchase the Beagle, plug the SDR into the cape connectors, install a couple of software packages and open up an incoming port through whatever Internet router may exist. An antenna solution must also be provided although the included active antenna should help in this regard. A $10 GPS puck antenna will work fine. An adequate power supply to the Beagle (e.g. 5V @ 2A) will be required.

Four channels of audio and waterfall streamed over the Internet 24/7 requires about 30 GB per month. This is a common cap for many residential broadband plans. An automatic dynamic-DNS system is already part of the software so a web link to the SDR is immediately available with no configuration. Of course the system can be configured to only allow connections from the local network and ignore Internet connection requests.

### Web interface screenshots

Click images for full size.

View of entire 0-30 MHz range:

[![](http://www.jks.com/KiwiSDR/ss.full.780px.jpg)](http://www.jks.com/KiwiSDR/ss.full.jpg)


Moderate zoom of medium-wave broadcast band with spectrum display on top enabled:

[![](http://www.jks.com/KiwiSDR/ss.MW.780px.jpg)](http://www.jks.com/KiwiSDR/ss.MW.jpg)


Over-the-horizon-radar (OTHR) from Cyprus showing short-term ionospheric fluctuations:

[![](http://www.jks.com/KiwiSDR/ss.Cyprus.780px.jpg)](http://www.jks.com/KiwiSDR/ss.Cyprus.jpg)

VLF/LF reception in New Zealand

| kHz | Station | Location | Signal |
| :-- | :------ | :------- | :----- |
| 12.88 | Alpha | Khabarovsk & Novosibirsk Russia | Navigation system |
| 17.0 | VTX2 | India | MSK comms |
| 19.8 | NWC | Australia | MSK comms |
| 21.4 | NPM | Hawaii | MSK comms |
| 22.2 | NDT/JJI | Japan | MSK comms |
| 24.1 | (UNID) | Korea, per David L Wilson | FSK comms |
| 40.0 | JJY | Japan | Time signal |
| 45.9 | NSY | Italy | MSK comms |
| 54.0 | NDI | Japan | MSK comms |
| 60.0 | JJY/WWVB | Japan, USA | Time signals |
| 68.5 | BPC | China | Time signal |

Many others heard during different times and conditions.

[![](http://www.jks.com/KiwiSDR/ss.VLF.LF.780px.jpg)](http://www.jks.com/KiwiSDR/ss.VLF.LF.jpg)
