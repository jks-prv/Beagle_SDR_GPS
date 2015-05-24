WRX
===

Software-defined Radio (SDR) and GPS for the BeagleBone Black
----------------------------------------------------------------------

An add-on board ("cape") that turns your Beagle into a web-accessible shortwave receiver.

### Details

* [Live prototype receiver](http://www.jks.com:8073) (when available -- password is 'kiwi').
* [Design review document](http://www.jks.com/docs/wrx/wrx.design.review.pdf).
* [Project history](http://www.jks.com/wrx/wrx.html).

### Components
* SDR covering the 10 KHz to 30 MHz (VLF-HF) spectrum.
* Web interface based on [OpenWebRX](http://http://openwebrx.org/) from András Retzler, HA7ILM.
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

### Objectives

I wanted to design an SDR that provides certain features, at a low price point, that I felt wasn't covered by current devices. The SDR must be web-accessible and simple to setup and use.

I also want to provide a self-contained platform for experimentation with SDR and GPS techniques. In this respect the project has a lot in common with the recent
[HackRF](https://www.kickstarter.com/projects/mossmann/hackrf-an-open-source-sdr-platform) and [BladeRF](https://www.kickstarter.com/projects/1085541682/bladerf-usb-30-software-defined-radio) Kickstarter projects.

Most importantly, I'd really like to see a significant number of web-enabled, wide-band SDRs deployed in diverse locations world-wide because that makes possible some really interesting applications and experiments.

### Description

The user will be required to purchase the Beagle, plug the SDR into the cape connectors, install a couple of software packages and open up an incoming port through whatever Internet router may exist. An antenna solution must also be provided although the included active antenna should help in this regard. A $10 GPS puck antenna will work fine. An adequate power supply to the Beagle (e.g. 5V @ 2A) will be required.

Four channels of audio and waterfall streamed over the Internet 24/7 requires about 30 GB per month. This is a common cap for many residential broadband plans. An automatic dynamic-DNS system is already part of the software so a web link to the SDR is immediately available with no configuration. Of course the system can be configured to only allow connections from the local network and ignore Internet connection requests.
