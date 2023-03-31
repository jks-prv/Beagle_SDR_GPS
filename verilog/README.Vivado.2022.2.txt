Steps to creating and building the KiwiSDR project with Vivado 2022.2
    Updated 25 March 2023

1) You will most likely be running Vivado on a Windows or Linux machine.
We run Vivado successfully on an x86-based MacBook Pro using the VirtualBox application to emulate
a PC and then running Ubuntu Linux on that and Vivado on that! (talk about punishment, lol).
Note that this solution will NOT work on an ARM-based MacBook (e.g. M1)

2) Copy Verilog source files from the KiwiSDR distribution to a directory on the machine
you will run Vivado as follows. We suggest creating a directory like verilog/KiwiSDR/import_srcs
and copying the files there from the KiwiSDR_SDR_GPS/verilog/ directory of the KiwiSDR distribution.

If you're using VirtualBox, use a shared folder between your PC/Mac and the guest OS to make moving
file files easier.

In a few steps you will tell Vivado to use verilog/KiwiSDR as the project directory and it will
create a bunch of files there, but will not modify anything in the import_srcs/ subdirectory.
You can then update import_srcs/ when you (or we) make changes to the Verilog code.
And Vivado will know to look for updated changes to the Verilog in that directory.

3) Now there is the question of the Vivado IP (intellectual property) blocks.

This is handled by a separate set of files in the verilog.Vivado.2022.2.ip/ subdirectory of the Kiwi distribution.
These files contain the IP block configuration parameters captured when we at KiwiSDR initially ran
the IP Catalog user interface as described in the verilog.Vivado.2022.2.ip/README file. These files will also be
copied to your build machine and Vivado told where to find them.
Then the first time you synthesize the project all the IP blocks will get compiled. This takes a
long time but only needs to be done once. You can open each IP block with the IP Re-customize editor
and change the configuration, and re-compile, if needed.

The verilog.Vivado.2022.2.ip/README file also described how to upgrade IP specifications in case you are
running a later Vivado version that doesn't work with the current IP file.

Create a directory called verilog/KiwiSDR/import_ip/ and copy the files from
KiwiSDR_SDR_GPS/verilog.Vivado.2022.2.ip/ there from your your build machine.

4) We are now ready to setup the project. To begin start Vivado.
">" denotes a user action (keyboard entry, mouse button push etc.)

5) On the front page after starting Vivado:
    > Quick Start > Create New Project

6) New Project menu
    > Next
    Project Name
        Project name
            > KiwiSDR
        Project location
            > verilog/      (note: NOT verilog/KiwiSDR/)
            Doesn't matter if "Create project subdirectory" is checked or not since
            verilog/KiwiSDR/ already exists.
	> Next
    Project Type
        > RTL Project
	> Next
    Add Sources
        > Add Directories
            Choose the verilog/KiwiSDR/import_srcs/ subdirectory.
            > Select the entry just created.
            Make sure "Scan and add RTL include files into project" is checked.
            Make sure "Copy sources into project" is NOT checked.
            Make sure "Add sources from subdirectories" is checked.
            Make sure target and simulator language is Verilog.
            > Next

    Add Constraints
        > Add files
            Navigate to the verilog/KiwiSDR/import_srcs/ directory and select the KiwiSDR.xdc file.
            > OK
            Make sure "Copy constraints files into project" is NOT checked.
                > Next

    Default Part
        Family
            > Artix-7
        Package
            > ftg256
        Speed-grade
            > -1
        Then select the xc7a35tftg256-1 from the list.
        > Next

    New Project Summary
        >Finish

7) Now the main Vivado user interface will appear. Look at the Project Manager > Sources window.
After it settles down the "KiwiSDR (kiwi.v)" entry should be listed in bold as the top-level module.

8) Copy the IP definition files. Under the left-side menu called "Project Manager":
    > Add Sources
    > Add or create design sources
    	> Next
    > Add Directories
        And specify the verilog/KiwiSDR/import_ip/ subdirectory.
        > Select
        Make sure "Scan and add RTL include files into project" is NOT checked.
        Make sure "Copy sources into project" is *IS* checked. <=== VERY IMPORTANT
        Make sure "Add sources from subdirectories" is checked
        > Finish

More than 100 errors will be generated because the files do not provide a complete description of the ip.
But they provide enough information for the ip to be rebuilt which will happen in the next step.

9) Build the Verilog by clicking the "Generate Bitstream" icon in the Vivado toolbar (8th from left).

Remember that the very first build will have to compile all the IP blocks. You can monitor the
progress by selecting the "Design Runs" tab at the bottom of the Project Manager window.

After building is complete you should get result similar to these:

Error count:
    Synthesis = 486 (360) [sometimes these numbers are ~100 higher depending on the IP synthesis]
    Implementation = 124 (128)
    DRC violations:
        warnings = 107 (111)
        advisories = 55 (55)

Utilization - Post Implementation:
    LUT 72% (80), FF 49% (59)
    BRAM 98% (79), DSP 50% (54)

10) The new .bit file will be in verilog/KiwiSDR/KiwiSDR.runs/impl_1/KiwiSDR.bit
Copy this to the Beagle_SDR_GPS/ directory where you build the Kiwi server code.
But see the next step for the correct .bit filename to create.

11) You must actually build four FPGA images with different configurations. This is to accomodate
the 3, 4, 8 and 14-channel (BBAI only) configuration modes (see admin page, "mode" tab).

In the file Beagle_SDR_GPS/kiwi.config uncomment the value of RX_CFG for the configuration you
want to build. Then run a "make" which will build the Kiwi server code, but also create the
generated Verilog include files verilog/kiwi.cfg.vh, verilog/kiwi.gen.vh and verilog/other.gen.vh
Copy the files over the files to the source directory of your Vivado build machine as before.

Then build the Kiwi Verilog code in Vivado as described previously. Copy the resulting
KiwiSDR.bit file back to your development machine and rename it to a filename of the form
KiwiSDR.rxA.wfB.bit where A = the number of rx channels in the configuration and 
B = the number of waterfall channels.

That is, the files:
    KiwiSDR.rx4.wf4.bit
    KiwiSDR.rx8.wf2.bit
    KiwiSDR.rx3.wf3.bit
    KiwiSDR.rx14.wf0.bit

The make file in Beagle_SDR_GPS/verilog/Makefile shows an example of this process.

It also shows how to use the Vivado tcl script "make_proj.tcl" to run Vivado in batch
mode to create all four .bit files without manually using the Vivado user interface.

12) Notes:
The files named *.v.OFF are Verilog files not used in the current configuration. By naming them
".OFF" Vivado ignores them and it keeps the Project Manager > Sources window less cluttered.
