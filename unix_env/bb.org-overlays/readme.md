Users:
------------

Install or Update bb-cape-overlays debian package (pre installed on: Debian Jessie/Stretch & Ubuntu Xenial images)

    sudo apt update ; sudo apt install bb-cape-overlays

Support Kernels:
------------

Pre-built kernels: (there are multiple options avaiable)

    cd /opt/scripts/tools
    git pull

v4.4.x-ti:

    sudo ./update_kernel.sh --lts-4_4 --ti-channel

v4.4.x-ti + Real Time:

    sudo ./update_kernel.sh --lts-4_4 --ti-rt-channel

v4.4.x mainline:

    sudo ./update_kernel.sh --lts-4_4 --bone-channel

v4.4.x mainline + Real Time:

    sudo ./update_kernel.sh --lts-4_4 --bone-rt-channel

v4.9.x-ti:

    sudo ./update_kernel.sh --lts-4_9 --ti-channel

v4.9.x-ti + Real Time:

    sudo ./update_kernel.sh --lts-4_9 --ti-rt-channel

v4.9.x mainline:

    sudo ./update_kernel.sh --lts-4_9 --bone-channel

v4.9.x mainline + Real Time:

    sudo ./update_kernel.sh --lts-4_9 --bone-rt-channel

v4.14.x-ti:

    sudo ./update_kernel.sh --lts-4_14 --ti-channel

v4.14.x-ti + Real Time:

    sudo ./update_kernel.sh --lts-4_14 --ti-rt-channel

v4.14.x mainline:

    sudo ./update_kernel.sh --lts-4_14 --bone-channel

v4.14.x mainline + Real Time:

    sudo ./update_kernel.sh --lts-4_14 --bone-rt-channel

Developers:
------------

Step 1: Clone this repo:

    git clone https://github.com/beagleboard/bb.org-overlays
    cd ./bb.org-overlays

Step 2: Install *.dtbo:

    ./install.sh

U-Boot Overlays:
------------

https://elinux.org/Beagleboard:BeagleBoneBlack_Debian#U-Boot_Overlays
