#
# Boot to U-Boot/Linux for ivinstall
#
# Some parts from Xilinx's AR# 68657
#

set usage {
  usage: ivinstall-zynqmp.tcl [-hws <url>] [-xvc <url>] [-cable <serial>] [-index <n>]

  options:
    -hws <url>        Connect to hw_server, url is 'tcp:<host>:<port>'
                      (default: use or start default hw_server).
    -xvc <url>        Connect to xvcserver, url is 'tcp:<host>:<port>'.
    -adapt <pattern>  Match JTAG adapter with 'pattern' (default: *).
    -index <n>        Select the nth matching target (default: 0).
}

set arg_hws     ""
set arg_xvc     ""
set arg_adapt   "*"
set arg_index   0
set arg_extra   {}

for { set index 0 } { $index < [ llength $argv ] } { incr index } {
    switch -exact -- [ lindex $argv $index ] {
        {-hws}       { set arg_hws        [ lindex $argv [ incr index ] ] }
        {-xvc}       { set arg_xvc        [ lindex $argv [ incr index ] ] }
        {-adapt}     { set arg_adapt      [ lindex $argv [ incr index ] ] }
        {-index}     { set arg_index      [ lindex $argv [ incr index ] ] }
        {-help}      { puts $usage; return 0 }
        default      { lappend arg_extra  [ lindex $argv $index ] }
    }
}

if { [llength $arg_extra] > 0 } {
    puts "*** unrecognized arguments: $arg_extra"
    puts $usage
    exit 1
}

if { $arg_index != 0 } {
    puts "*** -index not supported"
    # TODO add support for -index on ZynqMP
    exit 1
}

set conn_args {}
if { $arg_hws != "" } {
    puts "using hw_server: $arg_hws"
    lappend conn_args "-url" $arg_hws
}

if { $arg_xvc != ""  } {
    puts "using xvcserver: $arg_xvc"
    lappend conn_args "-xvc-url" $arg_xvc
}

puts "connect: $conn_args"
connect {*}$conn_args

if { $arg_xvc != "" } {
    # allow extra time for XVC to enumerate targets
    # ('targets -timeout <sec>' did not seem to work...)
    after 3000
}

if { $arg_xvc == "" } {
    # Set jtag freq to a super high number (it will be adjusted to max)
    # (jtag frequency not supported on xvcserver)
    jtag targets 1
    jtag frequency 10000000000
}

# Restart in JTAG mode, required as iVeia devices are fixed to QSPI bootstrap
# See also: ZynqMP BOOT_MODE_USER (CRL_APB) Register
targets -set -filter {jtag_cable_serial =~ "$arg_adapt" && name =~ "PSU"}
mwr  0xff5e0200 0x0100
rst -system

# Disable Security gates to view PMU MB target
# By default, JTAG security gates are enabled
# This disables security gates for DAP, PLTAP and PMU.
# See also: ZynqMP jtag_sec (CSU) Register
mwr 0xffca0038 0x1ff
after 500

# Load and run PMU FW
targets -set -filter {jtag_cable_serial =~ "$arg_adapt" && name =~ "MicroBlaze PMU"}
dow pmu.elf
con
after 500

# Reset A53, load and run FSBL
targets -set -filter {jtag_cable_serial =~ "$arg_adapt" && name =~ "Cortex-A53 #0"}
rst -processor
dow fsbl.elf
con

# Give FSBL time to run
after 5000
stop

# Load Images - see ivinstall script for details on image locations
dow -data uEnv.txt.bin 0x005ffff4
dow -data system.dtb 0x00700000
dow -data Image 0x00800000
dow -data initrd 0x04400000
dow -data tarball.tgz.bin 0x17fffff4

# Other SW...
dow u-boot.elf
dow atf.elf
con
