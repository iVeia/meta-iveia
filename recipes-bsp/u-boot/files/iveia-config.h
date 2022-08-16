#define CONFIG_EXTRA_ENV_SETTINGS \
    "bootenv_addr=0x600000\0" \
    "bootenv_file=uEnv.txt\0" \
    "boot_targets=mmc0 mmc1 dhcp\0" \
    "loadenv_mmc0=load mmc 0 ${bootenv_addr} ${bootenv_file}\0" \
    "loadenv_mmc1=load mmc 1 ${bootenv_addr} ${bootenv_file}\0" \
    "loadenv_dhcp=dhcp ${bootenv_addr} ${bootenv_file}\0" \
    "jtag_boot_magic=4a544147\0" \
    "loadenv_jtag=" \
    "    setexpr scratch_addr  ${bootenv_addr} - 0x10; " \
    "    setexpr magic_addr    ${bootenv_addr} - 0x0c; setexpr.l magic *${magic_addr}; " \
    "    setexpr filesize_addr ${bootenv_addr} - 0x08; setexpr.l fsize *${filesize_addr}; " \
    "    setexpr crc32_addr    ${bootenv_addr} - 0x04; setexpr.l crc32 *${crc32_addr}; " \
    "    mw.l ${scratch_addr} 0 4; " \
    "    setenv filesize 0; " \
    "    if test ${magic} = ${jtag_boot_magic}; then " \
    "        crc32 ${bootenv_addr} ${fsize} ${scratch_addr}; " \
    "        setexpr crc32_calc *${scratch_addr}; " \
    "        if test ${crc32} = ${crc32_calc}; then " \
    "            setenv filesize ${fsize}; " \
    "        fi; " \
    "    fi;\0" \
    "importenv=env import -t ${bootenv_addr} ${filesize}\0" \
    "load_mmc0=setenv tgt mmc0; run loadtgt\0" \
    "load_mmc1=setenv tgt mmc1; run loadtgt\0" \
    "load_dhcp=setenv tgt dhcp; run loadtgt\0" \
    "load_jtag=setenv tgt jtag; run loadtgt\0" \
    "loadtgt=" \
    "    if run loadenv_${tgt}; then " \
    "        run importenv; " \
    "        setenv loadmode ${tgt}; " \
    "    fi\0" \
    "zynq_forced_jtag_magic_addr=0x500000\0" \
    "zynq_test_forced_jtag=" \
    "    setexpr.l magic *${zynq_forced_jtag_magic_addr}; " \
    "    if test ${board} = zynq -a ${magic} = ${jtag_boot_magic}; then " \
    "        mw.l ${zynq_forced_jtag_magic_addr} 0; " \
    "        echo JTAG Magic found - forcing JTAG boot; " \
    "        setenv boot_targets jtag ${boot_targets}; " \
    "    fi;\0" \
    "pauseboot=setenv pauseboot_on 1; boot\0" \
    "bootseq=" \
    "    run zynq_test_forced_jtag " \
    "    setexpr first_target sub ' .*' '' \"${boot_targets} \" " \
    "    setexpr other_targets gsub \" ${first_target} \" ' ' \" ${boot_targets} \" " \
    "    setenv break " \
    "    for tgt in ${first_target} ${other_targets}; do " \
    "        if test -z \"${break}\"; then " \
    "            echo Attempting load of ${bootenv_file} from ${tgt}...; " \
    "            run loadtgt; " \
    "            if test -n \"${bootenv_cmd}\"; then " \
    "                run bootenv_cmd; " \
    "                if test -n \"${pauseboot_on}\"; then " \
    "                    echo Pausing boot. " \
    "                    setenv break 1 " \
    "                fi; " \
    "            fi; " \
    "        fi; " \
    "    done; " \
    "    echo No boot images found.  Aborting to prompt...\0" \

