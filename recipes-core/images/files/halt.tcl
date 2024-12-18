
set usage {
  usage: halt.tcl [-hws <url>] [-xvc <url>] [-cable <serial>] [-index <n>]

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
    after 3000
}

set target_filter {jtag_cable_serial =~ "$arg_adapt" && name =~ "*Cortex-A*#0"}
set target_list   [targets -filter $target_filter -target-properties]
set target_count  [llength $target_list]

if { $target_count < ($arg_index + 1) } {
    puts "*** matched $target_count targets, target $arg_index not found"
    exit 1
}

set target_item  [lindex $target_list $arg_index]
set target_index [dict get $target_item jtag_device_index]

targets -set -filter $target_filter -index $target_index

if { $arg_xvc != "" } {
    # reset and stop the system
    # ...as a backup, add a HW breakpoint at start-of-world
    set reset_bp [bpadd -type hw -addr 0x0]
    rst -system -stop
    after 500
    bpremove $reset_bp
} else {
    stop
}

exit 0
