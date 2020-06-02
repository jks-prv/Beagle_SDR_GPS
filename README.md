[updated 2 June 2020]

[![KiwiSDR](http://www.kiwisdr.com/ks/Seeed.sample.1.780px.jpg)](http://www.kiwisdr.com/ks/Seeed.sample.1.jpg)

Click image for full size.

[![KiwiSDR](http://www.kiwisdr.com/ks/kiwi-with-headphones.130x170.png)© bluebison.net](http://bluebison.net)

KiwiSDR
=======

Software-defined Radio (SDR) and GPS for the BeagleBone Black
----------------------------------------------------------------------

An add-on board ("cape") that turns your Beagle into a web-accessible shortwave receiver.

### Kickstarter
KiwiSDR had a successful [Kickstarter](https://www.kickstarter.com/projects/1575992013/kiwisdr-beaglebone-software-defined-radio-sdr-with/).

### Details

* Listen live: [List](http://rx.kiwisdr.com), [Map](http://rx.linkfanel.net)
* [Project webpage](http://www.kiwisdr.com/)
* [Operating information: installation, operation, FAQ](http://www.kiwisdr.com/quickstart/)
* Latest [source code commits](https://github.com/jks-prv/Beagle_SDR_GPS/commits/master)
* [Design review document](https://www.dropbox.com/s/i1bjyp1acghnc16/KiwiSDR.design.review.pdf?dl=1)
* [Schematic](http://www.kiwisdr.com/docs/KiwiSDR/kiwi.schematic.pdf), [Gerbers](http://www.kiwisdr.com/docs/KiwiSDR/kiwi.gerbers.tar), [Active antenna](http://www.kiwisdr.com/docs/KiwiSDR/ant.pdf), [BOM ODS](http://www.kiwisdr.com/docs/KiwiSDR/kiwi.bom.ods), [BOM XLS](http://www.kiwisdr.com/docs/KiwiSDR/kiwi.bom.xls)

### Description
This SDR is a bit different. It has a web interface that can be used by up to four separate listeners. Each one listening and tuning an independent frequency simultaneously. See the screenshots below.

### Components
* SDR covering the 10 kHz to 30 MHz (VLF-HF) spectrum.
* Web interface based on [OpenWebRX](http://openwebrx.org/) from András Retzler, HA7ILM.
* Integrated software-defined GPS receiver from Andrew Holme's [Homemade GPS Receiver](http://www.aholme.co.uk/GPS/Main.htm).
* LTC 14-bit 65 MHz ADC.
* Xilinx Artix-7 A35 FPGA.
* Skyworks SE4150L GPS front-end.

### Features
* 100% Open Source / Open Hardware.
* Browser-based interface allowing multiple simultaneous user web connections (currently 4).
* Each connection tunes an independent receiver channel over the entire spectrum.
* Waterfall tunes independently of audio and includes zooming and panning.
* Multi-channel, parallel DDC design using bit-width optimized CIC filters.
* Good performance at VLF/LF since we personally spend time monitoring those frequencies.
* Automatic frequency calibration via received GPS timing.
* Easy hardware and software setup. Browser-based configuration interface.

### Status

Give the live receivers a try at the links above. You'll need a recent version of a modern web browser that supports HTML5. The web interface works, with some problems, on mobile devices. But there is no mobile version of the interface yet.

Please email us any comments you have after reviewing the design document above.

### Objectives

We wanted to design an SDR that provides certain features, at a low price point, that we felt weren't covered by current devices. The SDR must be web-accessible and simple to setup and use.
We also wanted to provide a self-contained platform for experimentation with SDR and GPS techniques. 

Most importantly, We'd really like to see a significant number of web-enabled, wide-band SDRs deployed in diverse locations world-wide because that makes possible some really interesting applications and experiments.

### Operation

Users can purchase just the KiwiSDR board or a complete "kit" consisting of the board, BeagleBone Green (software pre-installed), enclosure (assembly required), and GPS antenna (see [here](http://www.kiwisdr.com/)).
The software will try to automatically open up an incoming port through whatever Internet firewall/router may exist on the local network, but the user may have to perform this step manually. An antenna solution must be provided. An adequate power supply to the Beagle (e.g. 5V @ 2A) will also be required.

Four channels of audio and waterfall streamed over the Internet 24/7 requires about 30 GB per month. This is a common cap for many residential broadband plans. An automatic dynamic-DNS system is already part of the software so a web link to the SDR is immediately available with no configuration. Of course the system can be configured to only allow connections from the local network and ignore Internet connection requests.

### Web interface screenshots

Click images for full size.

View of entire 0-30 MHz range:

[![](http://www.kiwisdr.com/ks/ss.full.780px.jpg)](http://www.kiwisdr.com/ks/ss.full.jpg)


Moderate zoom of medium-wave broadcast band with spectrum display on top enabled:

[![](http://www.kiwisdr.com/ks/ss.MW.780px.jpg)](http://www.kiwisdr.com/ks/ss.MW.jpg)


Over-the-horizon-radar (OTHR) from Cyprus showing short-term ionospheric fluctuations:

[![](http://www.kiwisdr.com/ks/ss.Cyprus.780px.jpg)](http://www.kiwisdr.com/ks/ss.Cyprus.jpg)

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

[![](http://www.kiwisdr.com/ks/ss.VLF.LF.780px.jpg)](http://www.kiwisdr.com/ks/ss.VLF.LF.jpg)

[end-of-document]
