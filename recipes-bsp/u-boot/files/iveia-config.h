#define CONFIG_EXTRA_ENV_SETTINGS \
    "bootenv_addr=0x600000\0" \
    "bootenv_file=uEnv.txt\0" \
    "boot_targets=mmc0 mmc1 dhcp\0" \
    "loadenv_mmc0=load mmc 0 ${bootenv_addr} ${bootenv_file}\0" \
    "loadenv_mmc1=load mmc 1 ${bootenv_addr} ${bootenv_file}\0" \
    "loadenv_dhcp=dhcp ${bootenv_addr} ${bootenv_file}\0" \
    "jtag_boot_magic=4a544147\0" \
    "loadenv_jtag=\n" \
    "    setexpr scratch_addr  ${bootenv_addr} - 0x10; \n" \
    "    setexpr magic_addr    ${bootenv_addr} - 0x0c; setexpr.l magic *${magic_addr}; \n" \
    "    setexpr filesize_addr ${bootenv_addr} - 0x08; setexpr.l fsize *${filesize_addr}; \n" \
    "    setexpr crc32_addr    ${bootenv_addr} - 0x04; setexpr.l crc32 *${crc32_addr}; \n" \
    "    mw.l ${scratch_addr} 0 4; \n" \
    "    setenv filesize 0; \n" \
    "    if test ${magic} = ${jtag_boot_magic}; then \n" \
    "        crc32 ${bootenv_addr} ${fsize} ${scratch_addr}; \n" \
    "        setexpr crc32_calc *${scratch_addr}; \n" \
    "        if test ${crc32} = ${crc32_calc}; then \n" \
    "            setenv filesize ${fsize}; \n" \
    "        fi; \n" \
    "    fi;\0" \
    "importenv=env import -t ${bootenv_addr} ${filesize}\0" \
    "load_mmc0=setenv tgt mmc0; run loadtgt\0" \
    "load_mmc1=setenv tgt mmc1; run loadtgt\0" \
    "load_dhcp=setenv tgt dhcp; run loadtgt\0" \
    "load_jtag=setenv tgt jtag; run loadtgt\0" \
    "loadtgt=\n" \
    "    if run loadenv_${tgt}; then \n" \
    "        run importenv; \n" \
    "        setenv loadmode ${tgt}; \n" \
    "    fi\0" \
    "zynq_forced_jtag_magic_addr=0x500000\0" \
    "zynq_test_forced_jtag=\n" \
    "    setexpr.l magic *${zynq_forced_jtag_magic_addr}; \n" \
    "    if test ${board} = zynq -a ${magic} = ${jtag_boot_magic}; then \n" \
    "        mw.l ${zynq_forced_jtag_magic_addr} 0; \n" \
    "        echo JTAG Magic found - forcing JTAG boot; \n" \
    "        setenv boot_targets jtag ${boot_targets}; \n" \
    "    fi;\0" \
    "pauseboot=setenv pauseboot_on 1; boot\0" \
    "iv-bootseq=\n" \
    "    run zynq_test_forced_jtag; \n" \
    "    setexpr first_target sub ' .*' '' \"${boot_targets} \"; \n" \
    "    setexpr other_targets gsub \" ${first_target} \" ' ' \" ${boot_targets} \"; \n" \
    "    setenv break; \n" \
    "    for tgt in ${first_target} ${other_targets}; do \n" \
    "        if test -z \"${break}\"; then " \
    "            echo Attempting load of ${bootenv_file} from ${tgt}...; \n" \
    "            run loadtgt; \n" \
    "            if test -n \"${bootenv_cmd}\"; then \n" \
    "                run bootenv_cmd; \n" \
    "                if test -n \"${pauseboot_on}\"; then \n" \
    "                    echo Pausing boot.; \n" \
    "                    setenv break 1; \n" \
    "                fi; \n" \
    "            fi; \n" \
    "        fi; \n" \
    "    done; \n" \
    "    echo No boot images found.  Aborting to prompt...\0" \

