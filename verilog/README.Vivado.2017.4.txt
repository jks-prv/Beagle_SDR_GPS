Steps to creating and building the KiwiSDR project with Vivado 2017.4
    Updated 6 Apr 2018

1) You will most likely be running Vivado on a Windows or Linux machine.
We run Vivado successfully on a MacBook Pro using the VirtualBox application to emulate a PC
and then running Ubuntu Linux on that and Vivado on that! (talk about punishment, lol).

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

This is handled by a separate set of files in the verilog.Vivado.2017.4.ip/ subdirectory of the Kiwi distribution.
These files contain the IP block configuration parameters captured when we at KiwiSDR initially ran
the IP Catalog user interface as described in the verilog.Vivado.2017.4.ip/README file. These files will also be
copied to your build machine and Vivado told where to find them.
Then the first time you synthesize the project all the IP blocks will get compiled. This takes a
long time but only needs to be done once. You can open each IP block with the IP Re-customize editor
and change the configuration, and re-compile, if needed.

Create a directory called verilog/KiwiSDR/import_ip/ and copy the files from
KiwiSDR_SDR_GPS/verilog.Vivado.2017.4.ip/ there from your your build machine.

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
    Project Type
        > RTL Project
    Add Sources
        > Add Directories
            Choose the verilog/KiwiSDR/import_srcs/ subdirectory.
            > Select
            Make sure "Copy sources into project" is NOT checked.
            Make sure "Add sources from subdirectories" is checked.
            Make sure target language is Verilog.
            > Next

    NB: Vivado 2017.4 differs from 2014.4 in that there is no "Add Existing IP" dialog displayed at
    this point. The IP will be added at step 8 below.

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
The GEN module will be listed because it isn't used in the current Verilog configuration.

8) Copy the IP definition files. This step if new for Vivado 2017.4 compared to 2014.4.
Under the left-side menu called "Project Manager":
    > Add Sources
    > Add or create design sources
    > Next
    > Add Directories
        And specify the verilog/KiwiSDR/import_ip/ subdirectory.
        > Select
        Make sure "Copy sources into project" is *IS* checked. <=== VERY IMPORTANT
        Make sure "Add sources from subdirectories" is checked
        > Finish

More than 100 errors will be generated because the files do not provide a complete description of the ip.
But they provide enough information for the ip to be rebuilt which will happen in the next step.

9) Build the Verilog by clicking the "Generate Bitstream" icon in the Vivado toolbar (8th from left).

Remember that the very first build will have to compile all the IP blocks. You can monitor the
progress by selecting the "Design Runs" tab at the bottom of the Project Manager window.

After building is complete you should get result similar to these: (includes Galileo GPS work in progress)

Error count:
    Synthesis = 382
    Implementation = 122
    DRC advisories = 106

Utilization - Post Implementation:
    FF 49%, LUT 71%
    BRAM 98%, DSP 50%

10) The new .bit file will be in verilog/KiwiSDR/KiwiSDR.runs/impl_1/KiwiSDR.bit
Copy this to the Beagle_SDR_GPS/ directory where you build the Kiwi server code.

11) Notes:
The files named *.v.OFF are Verilog files not used in the current configuration. By naming them
".OFF" Vivado ignores them and it keeps the Project Manager > Sources window less cluttered.


