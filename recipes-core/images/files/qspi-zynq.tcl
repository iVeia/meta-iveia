#
# Run U-Boot on Zynq-7000 to write BOOT.BIN to QSPI flash
#

set usage {
  usage: qspi-zynq.tcl [-hws <url>] [-xvc <url>] [-cable <serial>] [-index <n>]

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

# find targets
set target_filter {jtag_cable_serial =~ "$arg_adapt" && name =~ "*Cortex-A*#0"}
set target_list   [targets -filter $target_filter -target-properties]
set target_count  [llength $target_list]

if { $target_count < ($arg_index + 1) } {
    puts "*** matched $target_count targets, target $arg_index not found"
    exit 1
}

# find nth target (based on arg_index) and select it by jtag_device_index
set target_item  [lindex $target_list $arg_index]
set target_index [dict get $target_item jtag_device_index]

targets -set -filter $target_filter -index $target_index

# Zynq occasionally fails to reset and run FSBL (?)
# retry a few times if necessary...
#
set stopped 0
set retries 5

for {set i 1} {$i <= $retries} {incr i} {
    # reset and stop the system
    # a) with 'rst -stop' option, or
    # b) with start-of-world 'reset_bp' as a backup
    set reset_bp [bpadd -addr 0x0]
    rst -system -stop
    after 1000
    bpremove $reset_bp

    # load and run FSBL, break before loading boot image
    dow    fsbl.elf
    verify fsbl.elf
    bpremove -all
    set load_bp [bpadd LoadBootImage]
    con

    # allow FSBL time to run to breakpoint
    after 1000

    bpremove $load_bp

    # Zynq-7000 doesn't support restart in JTAG mode (Zynqmp does).  So we hardcode
    # a magic number in memory to force U-Boot to act as if it is booting in JTAG
    # mode.  (Even if this happened to match, it's doesn't affect boot unless the
    # uEnv.txt file/crc matches).
    if { [catch {mwr 0x500000 0x4a544147} err] } {
        # if reset/FSBL goes wrong, 'mwr' fails:
        # - neither reset_bp nor load_bp is triggered,
        # - 'mwr' yields a "Cannot write memory if not stopped." error
        puts "*** reset $i of $retries, mwr failed: $err"
    } else {
        set stopped 1
        break
    }
}

if {$stopped != 1} {
    error "*** reset failed after $retries attempts"
}

# Load Images - see ivinstall script for details on image locations
dow -data uEnv.qspi.txt.bin 0x5ffff4
dow -data boot.bin.bin 0x6ffff4

# Load and run U-Boot
dow    u-boot.elf
verify u-boot.elf
con

