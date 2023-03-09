SWITCH_UART ?= "1"
SWITCH_UART_MACRO = "XPAR_PSU_UART_${SWITCH_UART}_BASEADDR"

CONFIGURE_UART_TASK_NAME = ""
CONFIGURE_UART_TASK_NAME:zynqmp = "do_post_configure_uart"
do_configure[postfuncs] += "${CONFIGURE_UART_TASK_NAME}"
do_post_configure_uart () {
    for MACRO_NAME in STDIN_BASEADDRESS STDOUT_BASEADDRESS; do
        sed -i "s/^\(#define *${MACRO_NAME} *\).*/\1${SWITCH_UART_MACRO}/" ${XPARAMETERS_H}
    done
}

