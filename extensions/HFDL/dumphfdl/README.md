# dumphfdl

 dumphfdl is a multichannel HFDL (High Frequency Data Link) decoder.

HFDL (High Frequency Data Link) is a protocol used for radio communications between aircraft and a network of ground stations using high frequency (HF) radio waves. It is used to carry ACARS and AOC messages as well as CPDLC (Controller-Pilot Data Link Communications) and ADS-C (Automatic Dependent Surveillance - Contract). Thanks to the ability of short waves to propagate over long distances, HFDL is particularly useful in remote areas (eg. over oceans or polar regions) where other ground-based communications services are out of range. While many aircraft prefer satellite communications these days, HFDL is still operational and in use.

## Features

- Decodes multiple channels simultaneously, up to available CPU power and SDR bandwidth (no fixed channel count limit)
- Interfaces directly with SDR hardware via SoapySDR library (no pipes / virtual audio cables needed)
- Reads prerecorded I/Q data from a file
- Automatically reassembles multiblock ACARS messages and MIAM file transfers
- Supports various outputs and output formats (see below)
- Enriches logged messages with data from external sources:
  - ground station details - from the system table file
  - aircraft data - from Basestation SQLite database
- Extracts aircraft position information from decoded messages and provides a data feed for external plane tracking apps (eg. Virtual Radar Server)
- Produces decoding statistics using [Etsy StatsD](https://github.com/etsy/statsd) protocol
- Runs under Linux and MacOS

## Supported output formats

- Human readable text
- Basestation feed with aircraft positions

## Supported output types

- file (with optional daily or hourly file rotation)
- reliable network messaging via [ZeroMQ](https://zeromq.org/) - client and server mode
- TCP connection - client mode
- UDP socket

## Example

![dumphfdl screenshot](screenshots/screenshot.png?raw=true)

## Supported protocols and message types

- Media Protocol Data Units (MPDU)
- Link Protocol Data Units (LPDU):
  - Logon request / resume
  - Logon confirm / Resume confirm
  - Logoff request
  - Logon denied
  - Unnumbered data / Unnumbered acknowledged data
- Squitter Protocol Data Units (SPDU)
- HF Network Protocol Data Units (HFNPDU) in Direct Link Service mode:
  - System table
  - System table request
  - Performance data
  - Frequency data
  - Enveloped data
  - Delayed echo
- ACARS
- All ACARS applications and protocols supported by libacars library (full list [here](https://github.com/szpajder/libacars/blob/master/README.md#supported-message-types))

Less important fields from MPDUs (slot sel, N1, N2, H, NF, U(R), UDR), LPDUs (DDR, P) and SPDUs (slock ack codes, priority) are currently skipped over without decoding.

## Unsupported protocols

- Reliable Link Service (RLS). It's not actively used.

## Things TODO

- Sensitivity and decoding accuracy needs improvement - the amount of CRC errors should be lower.

- CPU usage could possibly be reduced.

## Installation

### Dependencies

Mandatory dependencies:

- C compiler with C11 support (gcc, clang, AppleClang)
- make
- cmake >= 3.1
- pkg-config
- glib2
- libconfig++
- libacars >= 2.1.0
- liquid-dsp >= 1.3.0
- fftw3 (preferably multithreaded implementation - libfftw3f\_threads)

Optional dependencies:

- SoapySDR (to use software defined radios; not needed for decoding from I/Q files)
- sqlite3 (to enrich messages with aircraft data from a basestation.sqb database)
- statsd-c-client (for Etsy StatsD statistics)
- libzmq 3.2.0 or later (for ZeroMQ networked output)
- google-perftools (for profiling)

Install necessary dependencies. Most of them are probably packaged in your Linux distribution. Example for Debian / RaspberryPi OS:

```sh
sudo apt install build-essential cmake pkg-config libglib2.0-dev libconfig++-dev libliquid-dev libfftw3-dev
```

Example for MacOS:

```sh
sudo brew update
sudo brew install liquid-dsp fftw soapysdr libconfig
```

Install `libacars` library:

- download the latest stable release package from [here](https://github.com/szpajder/libacars/releases/latest)
- unpack the archive
- compile and install the library:

```sh
cd libacars-<version_number>
mkdir build
cd build
cmake ../
make
sudo make install
sudo ldconfig       # on Linux only
```

Refer to libacars's README.md for complete instructions and available compilation options.

#### SoapySDR support (optional)

While the SoapySDR library might be included in your Linux distribution, it is a good idea to use the latest available version.

Download and install the library from [here](https://github.com/pothosware/SoapySDR).  Then install the driver module for your device. Refer to [SoapySDR wiki](https://github.com/pothosware/SoapySDR/wiki) for a list of all supported modules. Once you are done, verify your installation with:

```sh
SoapySDRUtil --info
```

Inspect the output and verify if your driver of choice is available. Here I have four drivers: `airspyhf`, `remote`, `rtlsdr`, `sdrplay`:

```text
######################################################
##     Soapy SDR -- the SDR abstraction library     ##
######################################################

Lib Version: v0.8.0-ga8df1c57
API Version: v0.8.0
ABI Version: v0.8
Install root: /usr/local
Search path:  /usr/local/lib/SoapySDR/modules0.8
Module found: /usr/local/lib/SoapySDR/modules0.8/libairspyhfSupport.so (0.2.0-d682533)
Module found: /usr/local/lib/SoapySDR/modules0.8/libremoteSupport.so   (0.5.2-d11da72)
Module found: /usr/local/lib/SoapySDR/modules0.8/librtlsdrSupport.so   (0.3.1-bec4f05)
Module found: /usr/local/lib/SoapySDR/modules0.8/libsdrPlaySupport.so  (0.3.0-8c4e330)
Available factories... airspyhf, remote, rtlsdr, sdrplay
```

Connect your radio and run:

```sh
SoapySDRUtil --probe=driver=<driver_name>
```

and verify if the device shows up. Example for Airspy HF+:

```text
$ SoapySDRUtil --probe=driver=airspyhf
######################################################
##     Soapy SDR -- the SDR abstraction library     ##
######################################################

Probe device driver=airspyhf

----------------------------------------------------
-- Device identification
----------------------------------------------------
  driver=AirspyHF
  hardware=AirspyHF
  serial=****************

----------------------------------------------------
-- Peripheral summary
----------------------------------------------------
  Channels: 1 Rx, 0 Tx
  Timestamps: NO

----------------------------------------------------
-- RX Channel 0
----------------------------------------------------
  Full-duplex: NO
  Supports AGC: YES
  Stream formats: CF32, CS16, CS8, CU16, CU8
  Native format: CF32 [full-scale=1]
  Antennas: RX
  Full gain range: [0, 54] dB
    LNA gain range: [0, 6, 6] dB
    RF gain range: [-48, 0, 6] dB
  Full freq range: [0.009, 31], [60, 260] MHz
    RF freq range: [0.009, 31], [60, 260] MHz
  Sample rates: 0.768, 0.384, 0.256, 0.192 MSps
```

#### SQLite (optional)

Some HFDL messages contain ICAO 24-bit hex code of the aircraft in question. In case you use Planeplotter or Virtual Radar Server or Kinetic Basestation software, you probably have a `basestation.sqb` SQLite database containing aircraft data (registration numbers, aircraft types, operator, etc). dumphfdl may use this database to enrich logged messages with this data. If you want this feature, install SQLite3 library:

Linux:

```sh
sudo apt install libsqlite3-dev
sudo ldconfig
```

MacOS:

```sh
sudo brew install sqlite
```

#### Etsy StatsD statistics (optional)

This feature causes the program to produce various performance counters and send them to the StatsD collector.

Install `statsd-c-client` library:

```sh
git clone https://github.com/romanbsd/statsd-c-client.git
cd statsd-c-client
make
sudo make install
sudo ldconfig       # on Linux only
```

#### ZeroMQ networked output support (optional)

ZeroMQ is a library that allows reliable messaging between applications to be set up easily. dumphfdl can publish decoded messages on a ZeroMQ socket and other apps can receive them over the network using reliable transport (TCP).  To enable this feature, install libzmq library.

Linux:

```sh
sudo apt install libzmq3-dev
```

It won't work on Debian/RaspberryPi OS older than Buster, since the libzmq library shipped with these is too old.

MacOS:

```sh
brew install zeromq
```

### Compiling dumphfdl

- Download a stable release package from [here](https://github.com/szpajder/dumphfdl/releases) and unpack it...
- ...or clone the repository:

```sh
cd
git clone https://github.com/szpajder/dumphfdl.git
cd dumphfdl
```

Configure the build:

```sh
mkdir build
cd build
cmake ../
```

`cmake` attempts to find all required and optional libraries. If a mandatory dependency is not installed, it will throw out an error describing what is missing. Unavailable optional dependencies cause relevant features to be disabled. At the end of the process `cmake` displays a short configuration summary, like this:

```text
-- dumphfdl configuration summary:
-- - SDR drivers:
--   - soapysdr:                requested: ON, enabled: TRUE
-- - Other options:
--   - Etsy StatsD:             requested: ON, enabled: TRUE
--   - SQLite:                  requested: ON, enabled: TRUE
--   - ZeroMQ:                  requested: ON, enabled: TRUE
--   - Profiling:               requested: OFF, enabled: FALSE
--   - Multithreaded FFT:       TRUE
-- Configuring done
-- Generating done
```

Here you can verify whether all the optional components that you need were properly detected and enabled. Then compile and install the program:

```sh
make
sudo make install
```

The last command installs the binary named `dumphfdl` to the default bin directory (on Linux it's `/usr/local/bin`). To display a list of available command line options, run:

```sh
/usr/local/bin/dumphfdl --help
```

or just `dumphfdl --help` if `/usr/local/bin` is in your `PATH`.

### Build options

Build options can be configured with `-D` option to `cmake`, for example:

```sh
cmake -DSOAPYSDR=FALSE ../
```

causes SOAPYSDR support to be disabled. It will not be compiled in, even if SoapySDR library is installed.

Disabling optional features:

- `-DSQLITE=FALSE`
- `-DETSY_STATSD=FALSE`
- `-DZMQ=FALSE`

Setting build type:

- `-DCMAKE_BUILD_TYPE=Debug` - builds the program with only minimal optimizations and enables `--debug` command line option which turns on debug messages (useful for troubleshooting, not recommended for general use)
- `-DCMAKE_BUILD_TYPE=Release` - debugging output disabled (the default)

**Note:** Always recompile the program with `make` command after changing build options.

**Note:** cmake stores build option values in its cache. Subsequent runs of cmake will cause values set during previous runs to be preserved, unless they are explicitly overriden with `-D` option. So if you disable a feature with, eg.  `-DSOAPYSDR=FALSE` and if you want to re-enable it later, you have to explicitly use `-DSOAPYSDR=TRUE` option. Just omitting `-DSOAPYSDR=FALSE` will not revert the option value to the default.

### Enabling and disabling optional features

As described in the "Dependencies" section, optional features (like SQLite support, ZMQ support, etc) are enabled automatically whenever libraries they depend upon are found during cmake run. Results of library searches are also stored in cmake's cache. If the program has initially been built without a particular feature and you later change your mind and decide to enable it, you need to:

- install the library required by the feature
- remove cmake's cache file to force all checks to be done again:

```sh
cd build
rm CMakeCache.txt
```

- rerun cmake and recompile the program as described in "Compiling dumphfdl" section.

## Basic usage

Simplest case for an SDRPlay radio:

```sh
dumphfdl --soapysdr driver=sdrplay --sample-rate 250000 8834 8885 8894 8912 8927 8939 8942 8948 8957 8977
```

- No particular device is specified, only the driver name - this causes the SoapySDR library to pick the first available SDRPlay device of the given type (sdrplay).
- No gain is specified - this causes auto gain control (AGC) to be used.
- No center frequency is specified - the program will compute one automatically, based on the given list of channel frequencies.
- No output is specified - the program will print decoded messages to the terminal (standard output).
- Sampling rate is set to 250000 samples per second (this parameter is mandatory).
- Then comes the list of 10 HFDL channel frequencies that should be monitored - each frequency must be specified in kHz. At least one channel must be specified. There is no upper limit on channel count - except the computing power of your machine, of course. All channels must fit in the bandwidth of the receiver, which is equal to the configured sampling rate.

### Configuring the sampling rate

Sampling rate is important. First of all, it must be actually supported by your radio! `SoapySDRUtil --probe=driver=sdrplay` will tell you that:

```sh
  Sample rates: 0.25, 0.5, 1, 2, 2.048, 6, 7, 8, 9, 10 MSps
  Filter bandwidths: 0.2, 0.3, 0.6, 1.536, 5, 6, 7, 8 MHz
```

While some radios may support a wider set of sample rates than the SoapySDR driver reports, others do not. Specifying an unsupported sampling rate often causes it to be silently accepted by the device, but in fact it will operate at a different rate (probably the nearest one supported). The program then won't be able to decode anything, since the time scale in the received signal will be wrong. So **always make sure to pick a supported sampling rate**. From the above example it appears that we can use any of the following values for `--sample-rate` option: 250000, 500000, 1000000, 2000000, 2048000, 6000000, 7000000, 8000000, 9000000 and 10000000 samples per second.

You should also pay attention to the available filter bandwidths. The radio performs bandpass filtering on the input signal before sampling it to prevent aliasing. It shall automatically pick the correct filter bandwidth that does not exceed the configured sampling rate. This means that the band edges will be somewhat attenuated and the useful bandwidth will therefore be smaller than the sampling rate - about 80% of its value. Therefore choose your HFDL channel frequencies wisely, so that they don't fall close to the edges of the receiver band, outside of the filter bandwidth. In the above example, HFDL channels span across 8977-8834=143 kHz which means that a sampling rate of 250000 samples per second is enough to cover them all (the device will pick a filter bandwidth of 200 kHz for this rate).

### Selecting a particular device

If multiple radios of the same type are connected to the machine, it is usually necessary to specify the one to use. `SoapySDRUtil --probe=driver=<driver_name>` tells their serial numbers or other identifiers:

```text
######################################################
##     Soapy SDR -- the SDR abstraction library     ##
######################################################

Probe device driver=sdrplay

----------------------------------------------------
-- Device identification
----------------------------------------------------
  driver=SDRplay
  hardware=RSP1A
  mir_sdr_api_version=2.130000
  mir_sdr_hw_version=255
  serial=1234567890
(...)
```

To select the radio with a serial number of 1234567890 use the following syntax:

```sh
dumphfdl --soapysdr driver=sdrplay,serial=1234567890 ...
```

### Frequency correction

Some radios have cheap quartz oscillators and may need a non-zero frequency correction. On HF this is mostly irrelevant, but just in case:

```sh
dumphfdl --freq-correction <ppm> ...
```

where `<ppm>` indicates the desired frequency correction expressed in parts per million, as a floating point decimal number.

### Gain settings

There are two ways to specify receiver gain manually. First, there is `--gain <gain_in_dB>` option. It attempts to set the end-to-end SDR gain to the given value by distributing it across all available gain stages optimally. Second, there is `--gain-elements <elem1=val1,elem2=val2,...>` option which allows setting the gain for each gain stage manually. Again, `SoapySDRUtil --probe=driver=<driver_name>` will tell you what gain stages are available and what are their supported value ranges:

```text
  Full gain range: [0, 48] dB
    IFGR gain range: [20, 59] dB
    RFGR gain range: [0, 9] dB
```

This shows that the device has two gain stages. The first one is named IFGR, the second one - RFGR. dumphfdl has no idea about what these stages mean and what they actually control. Please refer to the documentation of your SDR hardware.

Let's set these gains to some values. This is done as follows:

```sh
dumphfdl --gain-elements IFGR=54,RFGR=0 ...
```

Do not put spaces around `=` and `,` characters.

### Choosing antenna port

This might be useful for radios that have more than one antenna port. `SoapySDRUtil` will tell their names.

```sh
dumphfdl --antenna <antenna_port_name> ...
```

### Special device settings

Some radios might support special settings, like bias-T, notch filters, etc. `SoapySDRUtil` provides their list, descriptions, default and supported values. Example:

```text
----------------------------------------------------
-- Peripheral summary
----------------------------------------------------
  Channels: 1 Rx, 0 Tx
  Timestamps: NO
  Other Settings:
     * RF Gain Select - RF Gain Select
       [key=rfgain_sel, default=4, type=string, options=(0, 1, 2, 3, 4, 5, 6, 7, 8, 9)]
     * IF Mode - IF frequency in kHz
       [key=if_mode, default=Zero-IF, type=string, options=(Zero-IF, 450kHz, 1620kHz, 2048kHz)]
     * IQ Correction - IQ Correction Control
       [key=iqcorr_ctrl, default=true, type=bool]
     * AGC Setpoint - AGC Setpoint (dBfs)
       [key=agc_setpoint, default=-30, type=int, range=[-60, 0]]
     * BiasT Enable - BiasT Control
       [key=biasT_ctrl, default=true, type=bool]
     * RfNotch Enable - RF Notch Filter Control
       [key=rfnotch_ctrl, default=true, type=bool]
     * DabNotch Enable - DAB Notch Filter Control
       [key=dabnotch_ctrl, default=true, type=bool]
```

It appears that RF Notch Filter and DAB Notch filter are both enabled by default for this device. In order to turn them off, use the following syntax:

```sh
dumphfdl --device-settings rfnotch_ctrl=false,dabnotch_ctrl=false ...
```

### Setting the center frequency

Here is how to set the center frequency for the receiver:

```sh
dumphfdl --centerfreq 10050 ...
```

This sets the center frequency to 10050 kHz.

It is usually fine to omit this option and rely on automatic centerfreq configuration. The program will then set it to be exactly centered between the highest and the lowest channel frequency.

## Configuring outputs

### Quick start

By default dumphfdl formats decoded messages into human readable text and prints it to standard output. You can direct the output to a disk file instead:

```sh
dumphfdl --output decoded:text:file:path=/some/dir/hfdl.log ...
```

If you want the file to be automatically rotated on top of every hour, do
the following:

```sh
dumphfdl --output decoded:text:file:path=/some/dir/hfdl.log,rotate=hourly ...
```

The file name will be appended with `_YYYYMMDDHH` suffix.  If file extension is present, it will be placed after the suffix.

If you prefer daily rotation, change `rotate=hourly` to `rotate=daily`. The file name suffix will be `_YYYYMMDD` in this case. If file extension is present, it will be placed after the suffix.

### Output configuration syntax

The `--output` option takes a single parameter consisting of four fields separated by colons:

```sh
    <what_to_output>:<output_format>:<output_type>:<output_parameters>
```

where:

- `<what_to_output>` specifies what data should be sent to the output. Supported values:

  - `decoded` - output decoded messages

- `<output_format>` specifies how the data should be formatted before sending it to the output. The following formats are currently supported:

  - `text` - human-readable text
  - `basestation` - aircraft position feed in Kinetic Basestation format

- `<output_type>` specifies the type of the output. The following output types are supported:

  - `file` - output to a file
  - `tcp` - output to a remote server via TCP
  - `udp` - output to a remote host via UDP network socket
  - `zmq` - output to a ZeroMQ publisher socket

- `<output_parameters>` - specifies options for this output. The syntax is as follows:

```sh
    param1=value1,param2=value2,...
```

The list of available formats and output types may vary depending on which optional features have been enabled during program compilation and whether necessary dependencies are installed (see "Dependencies" subsection above).  Run `dumphfdl --output help` to determine which formats and output types are available on your system. It also shows all parameters supported by each output type.

Back to the above example:

```sh
--output decoded:text:file:path=/some/dir/hfdl.log,rotate=hourly
```

It basically says: "take decoded frames, format them as text and output the result to a file". Of course this output requires some more configuration - at least it needs the path where the file is to be created. This is done by specifying `path=/some/dir/hfdl.log` in the last field. The `file` output driver also supports an optional parameter named `rotate` which indicates how often the file is to be rotated, if at all.

A few more remarks about how output configuration works:

- Multiple simultaneous outputs are supported. Just specify the `--output` option more than once.

- If dumphfdl is run without any `--output` option, it creates a default output of `decoded:text:file:path=-` which causes decoded frames to be formatted as text and printed to standard output.

### Output drivers

#### `file`

Outputs data to a file.

Supported formats: `text`, `basestation`

Parameters:

- `path` (required) - path to the output file. If it already exists, the data is appended to it.

- `rotate` (optional) - how often to rotate the file. Supported values: `daily` (at midnight UTC or LT depending on whether `--utc` option is used) and `hourly` (rotate at the top of every hour). Default: no rotation.

#### `tcp`

Sends data to a remote host over the network using TCP/IP.

Supported formats: `text`, `basestation`

Parameters:

- `address` (required) - host name or IP address of the remote host

- `port` (required) - remote TCP port number

The primary purpose of the `tcp` output is to feed various plane tracking apps (like VRS) with aircraft position feed in `basestation` format.

#### `udp`

Sends data to a remote host over network using UDP/IP.

Supported formats: `text`, `basestation`

Parameters:

- `address` (required) - host name or IP address of the remote host

- `port` (required) - remote UDP port number

**Note:** UDP protocol does not guarantee successful message delivery (it works
on a "fire and forget" principle, no retransmissions, no acknowledgements, etc).
If you plan to use networked output for real, use `tcp` or `zmq` driver.

#### `zmq`

Opens a ZeroMQ publisher socket and sends data to it.

Supported formats: `text`, `basestation`

Parameters:

- `mode` (required) - socket mode. Can be `client` or `server`.  In the first case dumphfdl initiates a connection to the given consumer. In the latter case, dumphfdl listens on a port and expects consumers to connect to it.

- `endpoint` (required) - ZeroMQ endpoint. The syntax is: `tcp://address:port`.  When working in server mode, it specifies the address and port where dumphfdl shall listen for incoming connections. In client mode it specifies the address and port of the remote ZeroMQ consumer where dumphfdl shall connect to.

Examples:

- `mode=server,endpoint=tcp://*:5555` - listen on TCP port 5555 on all local addresses.

- `mode=server,endpoint=tcp://10.1.1.1:6666` - listen on TCP port 6666 on address 10.1.1.1 (it must be a local address).

- `mode=client,endpoint=tcp://host.example.com:1234` - connect to port 1234 on host.example.com.

### Diagnosing problems with outputs

Outputs may fail for various reasons. A file output may fail to write to the given path due to lack of permissions or lack of storage space, zmq output may fail to set up a socket due to incorrect endpoint syntax, etc. Whenever an output fails, the program disables it and prints a message on standard error, for example:

```text
Could not open output file /etc/hfdl.log: Permission denied
output_file: could not write to '/etc/hfdl.log', deactivating output
```

`tcp` output is an exception to the above. After it is initialized correctly, it never fails. If the connection is lost, it attempts to reestablish it every 10 seconds.

The program will continue to run and write data to all other outputs, except the failed one.

An output may also hang and stop processing messages (although this is a "shouldn't happen" situation). Messages will then accumulate in that output's queue. To prevent memory exhaustion, there is a high water mark limit on the number of messages that might be queued for each output. By default it is set to 1000 messages. If this value is reached, the program will not push any more messages to that output before messages get consumed and the queue length drops down. The following message is then printed on standard error for every dropped message:

```text
<output_type> output queue overflow, throttling
```

Other outputs won't be affected, since each one is running in a separate thread and has its own message queue.

High water mark limit is disabled when dumphfdl is decoding data from a file (ie. `--iq-file` option is in use). This allows all queues to grow indefinitely, but it assures that no frames get dropped.

The high water mark threshold can be changed with `--output-queue-hwm` option.  Set its value to 0 to disable the limit.

### Additional options for text formatting

The following options work globally across all outputs with text format:

- Add `--utc` option if you prefer UTC timestamps rather than local timezone in the logs and filenames.

- Add `--milliseconds` to print timestamps with millisecond resolution.

- Add `--raw-frames` option to display payload of all PDUs in hex for debugging purposes.

- Add `--output-mpdus` option to log Media Protocol Data Units. This is disabled by default - only LPDUs contained in MPDUs are logged.

- Add `--output-corrupted-pdus` option to log corrupted messages (too short, CRC check failed, etc). dumphfdl can't decode such messages, but they might optionally be logged as hex dump if `--raw-frames` is in use. By default corrupted messages are not included in the log.

- Some ACARS and MIAM CORE messages contain XML data. Use `--prettify-xml` option to enable pretty-printing of such content. XML will then be reformatted with proper indentation for easier reading. This feature requires libacars built with libxml2 library support - otherwise this option has no effect.

## Using the system table

HFDL system table is the database of all HFDL ground stations - their numeric ID, name, location and a list of assigned frequencies. The most recent version of the system table (as of writing this document) is available in the `etc/systable.conf` file in dumphfdl source tree. This file is not required for dumphfdl to work and it is not used by default. As a result, only numeric IDs of ground stations and frequencies are logged, since only these IDs are contained in HFDL messages:

```text
[2021-09-30 21:28:46 CEST] [11384.0 kHz] [24.7 Hz] [300 bps] [S]
Uplink SPDU:
 Src GS: 17
 Squitter: ver: 0 rls: 0 iso: 0
  Change note: None
  TDMA Frame: index: 906 offset: 4
  Minimum priority: 0
  System table version: 51
  Ground station status:
  ID: 17
   UTC sync: 1
   Frequencies in use: 3, 5
  ID: 7
   UTC sync: 1
   Frequencies in use: 1, 7
  ID: 8
   UTC sync: 1
   Frequencies in use: 2, 4
```

In order to use the system table, use the `--system-table /path/to/system_table_file` option. You may store this file wherever you want, dumphfdl only needs read permissions to it. If the file has been loaded successfully, the following message is printed to standard error on startup:

```text
System table loaded from /home/pi/dumphfdl/etc/systable.conf
```

Now dumphfdl is able to substitute ground station IDs with their names and frequency IDs with their actual values in kHz:

```text
[2021-09-30 21:29:39 CEST] [11384.0 kHz] [24.7 Hz] [300 bps] [S]
Uplink SPDU:
 Src GS: Canarias, Spain
 Squitter: ver: 0 rls: 0 iso: 0
  Change note: None
  TDMA Frame: index: 906 offset: 4
  Minimum priority: 0
  System table version: 51
  Ground station status:
  ID: Canarias, Spain
   UTC sync: 1
   Frequencies in use: 11348.0, 6529.0
  ID: Shannon, Ireland
   UTC sync: 1
   Frequencies in use: 10081.0, 2998.0
  ID: Johannesburg, South Africa
   UTC sync: 1
   Frequencies in use: 13321.0, 8834.0
```

`systable.conf` may be edited with any text editor, so for example, if you don't like the default station names, you may change them to whatever you want. Changing other things doesn't make much sense, since the table must be kept in sync with the current system table used by the HFDL network. If you change station frequencies, the contents of your log files will no longer match reality.

### Automatic updates of the system table

An aircraft may obtain the current system table by sending a System Table Request HFNPDU. The ground station will then respond with a full copy of the system table. Because it's too large to fit in a single HFDL message it is split into several HFNPDUs (typically 5). When dumphfdl receives all parts successfully, it reassembles the full table and prints it to the log file.

Whenever a change is introduced into the HFDL network (for example a new ground station is brought up or frequency assignments are modified), the system table version is incremented. The new version number will eventually appear in Squitter messages (shown above). Subsequently, aircraft will start requesting the new table to update their local copy with the new one. Once dumphfdl receives the new table successfully, it automatically updates its copy in memory and the new data will be used to enrich logged messages from now on. However the `systable.conf` file on disk will be kept unchanged. This means that after the program is restarted, it will continue to use the old table from the file. If you want to store newer versions of the system table to a file, add the `--system-table-save <target_file>` option. Whenever a new system table version is received, dumphfdl will then store it in the given file. If the file already exists, it will be overwritten.

`--system-table` and `--system-table-save` options might be used separately or together. The effects are as follows:

```sh
dumphfdl --system-table /home/pi/systable.conf ...
```

causes the program to read the system table from `/home/pi/systable.conf` on startup. When a new version of the table is received, its copy in memory is updated, but it is not stored on disk, so when the program is stopped, the new version is lost.

```sh
dumphfdl --system-table-save /home/pi/systable-new.conf ...
```

The program will start with an empty system table. If the system table is successfully received from the air, it will be cached in memory and saved into `/home/pi/systable-new.conf`.

```sh
dumphfdl --system-table /home/pi/systable.conf --system-table-save /home/pi/systable-new.conf ...
```

causes the program to read the system table from `/home/pi/systable.conf` on startup. When a new version of the table is received, its copy in memory is updated and saved to `/home/pi/systable-new.conf`. This approach allows you to review the new table for correctness and perhaps edit it by hand before putting it into use. Once you are happy with its contents, you may rename `systable-new.conf` to `systable.conf` making the new system table your current one, which will get loaded on every start of the program.

```sh
dumphfdl --system-table /home/pi/systable.conf --system-table-save /home/pi/systable.conf ...
```

as above, but dumphfdl will now overwrite the current system table file with the new one. If you restart the program, it will load the new table automatically.

#### Preserving station names on system table updates

System table updates sent over the air contain geographical coordinates of ground stations, but they don't contain station names. Whenever dumphfdl receives a new version of the system table, it tries to match stations from the new table to stations in the old table. If both versions of the table contain a station with the same ID and coordinates of this station are the same or differ by less than 1 degree, dumphfdl assumes that this is the same station as before and copies its name from the old table to the new one. This saves the user from copying station names from the old table to the new one manually. If however the new table contains a new station which does not exist in the old table, its name is obviously unknown. You have to edit the new table file by hand and add a proper station name of your choice.

## Enriching logs with aircraft data

If compiled with SQLite3 support, dumphfdl can read aircraft data from SQLite3 database in a well-known Basestation format used in various plane tracking applications. This data is then written to the logs in messages for which the ICAO code is known. Use `--bs-db /path/to/basestation.sqb` option to enable the feature. Verbosity can be controlled with the `--ac-details` option which takes two values: `normal` (the default) and `verbose`. Example with `--ac-details` set to `verbose`:

```text
[2021-09-30 22:28:37 CEST] [11384.0 kHz] [25.3 Hz] [300 bps] [S]
Uplink LPDU:
 Src GS: Canarias, Spain
 Dst AC: 255
 Type: Logon confirm
  ICAO: 424297
   AC info: VP-BJY, Airbus, A320 214SL, Aeroflot Russian Airlines
  Assigned AC ID: 70
```

dumphfdl reads data from `Aircraft` table. The following fields are used:

- `--ac-details normal`: Registration, ICAOTypeCode, OperatorFlagCode
- `--ac-details verbose`: Registration, Manufacturer, Type, RegisteredOwners

ICAO hex code is read from the ModeS field. All fields are expected to have a data type `varchar`. The ModeS field must be unique and non-NULL. Other fields are allowed to be NULL (the program substitutes NULLs with a dash).

Entries from the database are read on the fly, when needed. They are cached in memory for 30 minutes and then re-read from the database or purged.

## Debugging output

If the program has been compiled with `-DCMAKE_BUILD_TYPE=Debug`, there is a `--debug` option available. It controls debug message classes which should (or should not) be printed to standard error. Run the program with `--debug help` to list all debug message classes available. It is not advised to use this feature during normal operation, as it incurs a performance penalty.

## Statistics

The program does not calculate statistics by itself. Instead, it sends metric values (mostly counters) to the external collector using Etsy StatsD protocol.  It's the collector's job to receive, aggregate, store and graph them. Some examples of software which can be used for this purpose:

- [Collectd](https://collectd.org/) is a statistics collection daemon which supports a lot of metric sources by using various [plugins](https://collectd.org/wiki/index.php/Table_of_Plugins).  It has a [StatsD](https://collectd.org/wiki/index.php/Plugin:StatsD) plugin which can receive statistics emitted by dumphfdl, aggregate them and write to various time-series databases like RRD, Graphite, MongoDB or TSDB.

- [Graphite](https://graphiteapp.org/) is a time-series database with powerful analytics and aggregation functions. Its graphing engine is quite basic, though.

- [Grafana](http://grafana.org/) is a sophisticated and elegant graphing solution supporting a variety of data sources.

Here is an example of some dumphfdl metrics being graphed by Grafana:

![Statistics](screenshots/statistics.png?raw=true)

Metrics are quite handy when tuning your receiving installation or monitoring HF propagation or HFDL channel usage patterns.

To enable statistics just give dumphfdl your StatsD collector's hostname (or IP address) and UDP port number, for example:

```sh
dumphfdl --statsd 10.10.10.15:1234 ...
```

Refer to the `doc/STATSD_METRICS.md` file for a complete list of currently supported metrics.

## Processing recorded I/Q data from file

The syntax is:

```sh
dumphfdl --iq-file <file_name> --sample-rate <samples_per_sec> --sample-format <sample_format>
  [--centerfreq <center_frequency_in_kHz>] hfdl_freq_1 [hfdl_freq_2] [...]
```

The program accepts raw data files without any header. Files produced by `rx_sdr` or `airspyhf_rx` apps are perfectly valid input files. Different radios produce samples in different formats, though. dumphfdl currently supports following sample formats:

- `U8` - 8-bit unsigned (eg. recorded with rtl\_sdr program).
- `CS16` - 16-bit signed, little-endian (eg. SDRPlay)
- `CF32` - 32-bit float, little-endian (eg. Airspy HF+)

Use `--sample-format` option to set the format. There is no default. This option is mandatory for an `--iq-file` input. Use `--centerfreq` to set the center frequency. This shall be the frequency that the SDR was tuned to when the recording was made. Then provide a list of HFDL channel frequencies to monitor, in the same way as for SoapySDR input.

Putting it all together:

```sh
dumphfdl --iq-file iq.cs16 --sample-rate 250000 --sample-format CS16 --centerfreq 10000.0 10063.0 10081.0 10084.0
```

processes `iq.dat` file recorded at 250000 samples/sec using 16-bit signed samples, with receiver center frequency set to 10000 kHz (10 MHz). The program will monitor HFDL channels located at 10063, 10081 and 10084 kHz.

## Launching dumphfdl as a service on system boot

There is an example systemd unit file in `etc` subdirectory (which means you need a systemd-based distribution, like Debian/RaspberryPi OS Jessie or newer).

First, go to the dumphfdl build directory and install the binary to `/usr/local/bin` if you haven't done this before:

```sh
cd dumphfdl/build
sudo make install
```

Copy the unit file to the systemd unit directory:

```sh
cd ..
sudo cp etc/dumphfdl.service /etc/systemd/system/
```

Copy the example environment file to `/etc/default` directory:

```sh
sudo cp etc/dumphfdl /etc/default/
```

Edit `/etc/default/dumphfdl` with a text editor (eg. nano). Uncomment the
`DUMPHFDL_OPTIONS=` line and put your preferred dumphfdl options there.

Reload systemd configuration:

```sh
sudo systemctl daemon-reload
```

Start the service:

```sh
sudo systemctl start dumphfdl
```

Verify if it's running:

```sh
systemctl status dumphfdl
```

It should show: `Active: active (running) since <date>`. If it failed, it might be due to an error in the `DUMPHFDL_OPTIONS` value. Read the log messages in the status output and fix the problem.

If everything works fine, enable the service, so that systemd starts it automatically at boot:

```sh
systemctl enable dumphfdl
```

## Extras

There are a few additions to the program in the `extras` directory in the source tree. Refer to the README.md file in that directory for the current list of extras and their purpose.

## Performance

Digital signal processing is a computationally expensive task. dumphfdl uses FFT channelizer to extract narrowband HFDL channels from the wideband signal received from the SDR. Compared to a commonly used approach (direct frequency downconversion) it lowers the CPU usage and makes it less dependent on the number of monitored channels. This in turn makes it feasible to run the program on small single-board computers like Raspberry Pi, Odroid or the like. Observe two things:

- The primary factor that determines the overall CPU usage is the sampling rate.

- At the given sampling rate the number of configured HFDL channels does not matter much.

For example, Raspberry Pi 3 runs fine with AirspyHF+ set to its maximum sample rate (768000 samples per second), on condition that no other CPU-intensive asks are running on it. Odroid XU4 works OK with SDRPlay RSP1A up to about 2 Msps which results in a CPU usage at about 300% (that is, 3 CPU cores fully utilized). This allows simultaneous monitoring of approximately 1.5 MHz of bandwidth, ie. 2 HFDL subbands (for example, 8.9 MHz and 10.0 MHz or 10.0 MHz and 11.3 MHz). Powerful PCs are of course capable of handling higher sampling rates, however it is worth noting that monitoring a large swath of bandwidth with a single receiver is not optimal from sensitivity standpoint. Short wave bands are challenging - weak transmissions (like HFDL) are interspersed with very strong ones (broadcast stations, OTH radars, etc), which may saturate the receiver and distort the signal. This is also not optimal from CPU usage perspective, since dumphfdl must process a lot of data just to discard most of it. It is therefore a better option to set up multiple dumphfdl instances, each one with a separate SDR configured to a low sampling rate (just enough to cover all channels from a single HFDL subband - 192 ksps or 250 ksps works fine).

## Frequently Asked Questions

### Is HFDL used in my area?

Definitely, as it has a global reach. At any time of day and year you should be able to receive something.

### What antenna should I use?

HFDL operates in several sub-bands in the range between 2.8 MHz and 22 MHz (see `etc/systable.conf` for a complete list). dumphfdl, paired with a suitable wideband SDR, can receive many channels simultaneously. Therefore the most desirable choice is a wideband short wave receiving antenna, eg. a magnetic loop or a mini whip. Wire antennas of proper length are of course a good choice too, however they might not be wideband enough to cover all HF bands simultaneously. In general, HF reception is a broad topic which is out of scope of this FAQ. Head on to the books, articles and discussion groups for more knowledge on this matter.

### What receiver should I use?

Any software defined radio which supports HF and for which there is a SoapySDR driver available, should do the job. During development the program is tested on Airspy HF+ and SDRPlay RSP1A. Popular RTL-SDR dongles are not suitable, because they can't tune down below 24 MHz. While there exists a hack called direct sampling mode, please be aware that it's no magic - it does not turn the dongle into a proper HF radio. There is no gain control, the sensitivity is rather poor, the dongle is then prone to overload and frequencies above 14.4 MHz are out of reach. You may have some luck and receive something with it, but expect no miracles and please do not report your poor results as a bug in the program.

### What do these numbers in the message header mean?

```text
[2021-09-30 23:37:23 CEST] [6535.0 kHz] [7.3 Hz] [300 bps] [S]
```

From left to right:

- timestamp of frame reception

- channel frequency where the frame has been received

- frequency offset of the frame

- bit rate at which the frame has been transmitted: 300, 600, 1200 or 1800 bps

- HFDL frame length; "S" means single-timeslot frame, while "D" means double-slot frame (ie. occupying two consecutive slots)

### I want colorized logs, like on the screenshot

dumphfdl does not have log colorization feature. But there is a program named [MultiTail](https://www.vanheusden.com/multitail/) which you can use to follow dumphfdl log file in real time, as it grows, with optional colorization. It's just a matter of writing a proper colorization scheme which  tells the program what words or phrases to colorize and what color to use. Refer to `multitail-dumphfdl.conf` file in the `extras` subdirectory for an example. To use it:

- Install the program:

```sh
sudo apt install multitail
```

- Copy the example colorization scheme to `/etc`:

```sh
sudo cp extras/multitail-dumphfdl.conf /etc
```

- Include the color scheme into the main MultiTail configuration file:

```sh
sudo echo "include:/etc/multitail-dumphfdl.conf" >> /etc/multitail.conf
```

- You can only colorize the file while it grows.  So assuming that dumphfdl is running and writing its log into `hfdl.log` file in the current directory, type the following:

```sh
multitail -cS dumphfdl hfdl.log
```

`-cS dumphfdl` option indicates that multitail should use the color scheme named `dumphfdl`.

### How do I send aircraft positions to Virtual Radar Server?

First, configure a new push feed in VRS:

- Go to Tools / Options

- Choose "Receivers" in the tree on the left, then click the "+" icon above the receiver list in the right pane

- Configure the receiver like this:

![VRS receiver configuration](screenshots/vrs_config.png?raw=true)

- You can choose a different receiver name and port number, if you wish. Click OK to save your changes. In the main window you should see the new feed in "Waiting" state on the Feed status list.

- Add the following option to your dumphfdl command line (change the 10.10.10.10 address with the address of the machine where VRS is running):

```sh
--output decoded:basestation:tcp:address=10.10.10.10,port=20005
```

Once you start the program, it should immediately connect to VRS and start sending position data as they are received. The feed state in VRS should change to "Connected".

When running multiple instances of dumphfdl, you must create a separate push receiver in VRS for each one.

**IMPORTANT NOTE!**

While the position feed generated by dumphfdl is a nice way to visualise your reception range, please use it on your local VRS instance only. **Don't be tempted to send it to places like ADS-B Exchange or Planeplotter network!** The reasons are as follows:

- ADS-B positions are near-realtime, while positions reported on HFDL are often delayed due to low speed of this datalink. While the plane tracking software shall discard positions that are older than the most recently received, you may still cause some planes to jump backwards on the map (eg. when they go out of ADS-B reception range and your HFDL data takes over).

- Aircraft callsigns in ModeS are 7 characters long, but HFDL only allows 6 characters. Mixing ADS-B with HFDL data may therefore cause the callsign to flip between two values, one of them truncated to 6 characters.

- Some aircraft use incorrect ICAO code in HFDL (different than in ModeS/ADS-B). Such aircraft will appear on the map with a wrong registration number, owner and type.

In short - mixing ADS-B data with HFDL data is a sure-fire way to make a mess. If you keep your data local, then it's not a big deal. If you publish it to a public site intended for feeding ADS-B data without prior agreement with the site owner, then you will make a mess and nobody will thank you.

**END OF IMPORTANT NOTE**

### I see some aircraft positions in the logs that are not sent to VRS. Why?

- Positions with timestamps older than 5 minutes are discarded. This is to prevent outdated information from appearing on the map.

- VRS needs an ICAO code to display the plane. However, not all HFDL messages contain an ICAO code. In fact, most of them don't - messages in HFDL are addressed with aircraft IDs which are assigned dynamically by the ground station when the aircraft logs on on a particular channel. In order to establish the mapping between aircraft ID and its ICAO code, dumphfdl must receive the Logon confirm LPDU from the ground station that contains this mapping. Only then will it log the aircraft ICAO code in subsequent messages and emit its positions to the VRS feed. The program discards positions of aircraft for which it doesn't know the ICAO code.

- Only messages of the following types are searched for position data:

  - Performance data HFNPDU
  - Frequency data HFNPDU
  - ADS-C Basic report

### How to receive data from dumphfdl using ZeroMQ sockets?

Here is how to do it in Python, assuming that you are running RaspberryPi OS Buster or later.

First, install python3-zmq:

```sh
apt install python3-zmq
```

**Scenario 1:** dumphfdl works as a client, Python script is a server:

```python
import zmq, sys
context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.bind(sys.argv[1])
socket.setsockopt_string(zmq.SUBSCRIBE, '')
while True:
    string = socket.recv_string()
    print(string)
```

Save the script in a file (for example, `zmqserver.py`) and run it like this:

```sh
python3 zmqserver.py tcp://*:5555
```

Assuming that the above script is running on a machine with an IP address of 10.10.10.1, you can then run dumphfdl with `zmq` output set to client mode like this:

```sh
dumphfdl --output decoded:text:zmq:mode=client,endpoint=tcp://10.10.10.1:5555 ...
```

**Scenario 2:** dumphfdl works as a server, Python script is a client:

```python
import zmq,sys
context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect(sys.argv[1])
socket.setsockopt_string(zmq.SUBSCRIBE, '')
while True:
    string = socket.recv_string()
    print(string)
```

The only difference is that now the script calls `socket.connect()` instead of `socket.bind()`. Assuming that the IP address of the machine where dumphfdl is running is 10.10.10.2, save the script as `zmqclient.py` and run it as follows:

```sh
python3 zmqclient.py tcp://10.10.10.2:5555
```

Then start dumphfdl in ZeroMQ server mode:

```sh
dumphfdl --output decoded:text:zmq:mode=server,endpoint=tcp://*:5555
```

Both scripts print arriving messages to standard output.

The advantage of the second scenario is that dumphfdl can serve multiple clients using just a single `zmq` output. However the first scenario may come in handy if dumphfdl is running behind a firewall which does not permit connections from the outside. In this case if the output is to be sent to multiple consumers, each one must be configured as a separate `zmq` output.

## License

Copyright (c) Tomasz Lemiech <szpajder@gmail.com>

Contains code from the following software projects:

- libfec, (c) 2006 by Phil Karn, KA9Q

- Rocksoft^tm Model CRC Algorithm Table Generation Program V1.0 by Ross Williams

- csdr, Copyright (c) 2014, Andras Retzler <randras@sdr.hu>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
