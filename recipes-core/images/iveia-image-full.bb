require recipes-core/images/iveia-image-minimal.bb

# Use openssh, not dropbear
IMAGE_FEATURES += "ssh-server-openssh"
IMAGE_INSTALL += "packagegroup-core-ssh-openssh"

# Add on packages
IMAGE_INSTALL += "base-files-extras"
IMAGE_INSTALL += "rsync coreutils"
IMAGE_INSTALL += "i2c-tools"
IMAGE_INSTALL += "strace"
IMAGE_INSTALL += "lmsensors-sensors lmsensors-libsensors"
IMAGE_INSTALL += "bc"
IMAGE_INSTALL += "stress-ng"
IMAGE_INSTALL += "iperf2 iperf3"
IMAGE_INSTALL += "memtester"
IMAGE_INSTALL += "lrzsz"
IMAGE_INSTALL += "libgpiod"
IMAGE_INSTALL += "bash-completion"
IMAGE_INSTALL += "ethtool"
IMAGE_INSTALL += "htop"
IMAGE_INSTALL += "socat"
IMAGE_INSTALL += "tree"
IMAGE_INSTALL += "gdbserver"
IMAGE_INSTALL += "net-tools"
IMAGE_INSTALL += "tcpdump"
IMAGE_INSTALL += "iptables"
IMAGE_INSTALL += "usbutils"

IMAGE_INSTALL += "python3"

