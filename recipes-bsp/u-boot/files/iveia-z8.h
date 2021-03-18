#define CONFIG_EXTRA_ENV_SETTINGS \
    "bootenv_addr=0x600000\0" \
    "bootenv_file=uEnv.txt\0" \
    "boot_targets=mmc0 mmc1 dhcp\0" \
    "loadenv_mmc0=load mmc 0 ${bootenv_addr} ${bootenv_file}\0" \
    "loadenv_mmc1=load mmc 1 ${bootenv_addr} ${bootenv_file}\0" \
    "loadenv_dhcp=dhcp ${bootenv_addr} ${bootenv_file}\0" \
    "loadenv_jtag=\\\n" \
    "    setexpr scratch_addr  ${bootenv_addr} - 0x20; \\\n" \
    "    setexpr magic_addr    ${bootenv_addr} - 0x18; setexpr magic *${magic_addr}; \\\n" \
    "    setexpr filesize_addr ${bootenv_addr} - 0x10; setexpr _fs   *${filesize_addr}; \\\n" \
    "    setexpr crc32_addr    ${bootenv_addr} - 0x08; setexpr crc32 *${crc32_addr}; \\\n" \
    "    mw.q ${scratch_addr} 0 4; \\\n" \
    "    setenv filesize 0; \\\n" \
    "    if test ${magic} = 5d8b1a08c74bf3d7; then \\\n" \
    "        crc32 ${bootenv_addr} ${_fs} ${scratch_addr}; \\\n" \
    "        setexpr crc32_calc *${scratch_addr}; \\\n" \
    "        if test ${crc32} = ${crc32_calc}; then \\\n" \
    "            setenv filesize ${_fs}; \\\n" \
    "        fi; \\\n" \
    "    fi;\0" \
    "importenv=env import -t ${bootenv_addr} ${filesize}\0" \
    "load_mmc0=setenv tgt mmc0; run loadtgt\0"\
    "load_mmc1=setenv tgt mmc1; run loadtgt\0"\
    "load_dhcp=setenv tgt dhcp; run loadtgt\0"\
    "load_jtag=setenv tgt jtag; run loadtgt\0"\
    "loadtgt=\\\n" \
    "    if run loadenv_${tgt}; then \\\n" \
    "        run importenv; \\\n" \
    "        setenv loadmode ${tgt}; \\\n" \
    "    fi\0"\
    "bootseq=\\\n" \
    "    for tgt in ${boot_targets}; do \\\n" \
    "        echo Attempting load of ${bootenv_file} from ${tgt}...; \\\n" \
    "        run loadtgt; \\\n" \
    "        test -n ${bootenv_cmd} && run bootenv_cmd; \\\n" \
    "    done; \\\n" \
    "    echo No boot images found.  Aborting to prompt...\0" \

