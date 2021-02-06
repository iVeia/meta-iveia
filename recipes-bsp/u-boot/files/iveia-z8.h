#define CONFIG_EXTRA_ENV_SETTINGS \
    "bootenv_addr=0x600000\0" \
    "bootenv_file=uEnv.txt\0" \
    "boot_targets=mmc0 mmc1 dhcp\0" \
    "loadenv_mmc0=load mmc 0 ${bootenv_addr} ${bootenv_file}\0" \
    "loadenv_mmc1=load mmc 1 ${bootenv_addr} ${bootenv_file}\0" \
    "loadenv_dhcp=dhcp ${bootenv_addr} ${bootenv_file}\0" \
    "loadenv_jtag=" \
        "setexpr scratch_addr  ${bootenv_addr} - 0x20; " \
        "setexpr magic_addr    ${bootenv_addr} - 0x18; setexpr magic     *${magic_addr}; " \
        "setexpr filesize_addr ${bootenv_addr} - 0x10; setexpr _filesize *${filesize_addr}; " \
        "setexpr crc32_addr    ${bootenv_addr} - 0x08; setexpr crc32     *${crc32_addr}; " \
        "mw.q ${scratch_addr} 0 4; " \
        "setenv filesize 0; " \
        "if test ${magic} = 5d8b1a08c74bf3d7; then " \
            "crc32 ${bootenv_addr} ${_filesize} ${scratch_addr}; " \
            "setexpr crc32_calc *${scratch_addr}; " \
            "if test ${crc32} = ${crc32_calc}; then " \
                "setenv filesize ${_filesize}; " \
            "fi; " \
        "fi;\0" \
    "importenv=env import -t ${bootenv_addr} ${filesize}\0" \
    "load_mmc0=setenv tgt mmc0; run loadtgt\0"\
    "load_mmc1=setenv tgt mmc1; run loadtgt\0"\
    "load_dhcp=setenv tgt dhcp; run loadtgt\0"\
    "load_jtag=setenv tgt jtag; run loadtgt\0"\
    "trybootenv_cmd=test -n ${bootenv_cmd} && run bootenv_cmd\0"\
    "loadtgt=if run loadenv_${tgt}; then run importenv; setenv loadmode ${tgt}; fi\0"\
    "boottgt=run loadtgt trybootenv_cmd\0"\
    "bootseq=" \
        "for tgt in ${boot_targets}; do " \
            "echo Attempting load of ${bootenv_file} from ${tgt}...; " \
            "run boottgt; " \
        "done; " \
        "echo No boot images found.  Aborting to prompt...\0" \

