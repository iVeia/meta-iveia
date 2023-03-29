#
# Connect and halt processor
#
connect

# If given, first arg is the jtag_cable_serial wildcard string.  Use to select
# the specific JTAG adapter.
if {$argc > 0} {
    set jtag_cable_serial [lindex $argv 0]
} else {
    set jtag_cable_serial "*"
}

targets -set -filter {jtag_cable_serial =~ "$jtag_cable_serial" && name =~ "APU"}
stop
