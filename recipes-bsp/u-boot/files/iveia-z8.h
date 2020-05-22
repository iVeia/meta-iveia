#define CONFIG_EXTRA_ENV_SETTINGS \
    "bootenv_addr=0x600000\0" \
    "bootenv_file=uEnv.txt\0" \
    "boot_targets=mmc0 mmc1 dhcp\0" \
    "loadenv_mmc0=load mmc 0 ${bootenv_addr} ${bootenv_file}\0" \
    "loadenv_mmc1=load mmc 1 ${bootenv_addr} ${bootenv_file}\0" \
    "loadenv_dhcp=dhcp ${bootenv_addr} ${bootenv_file}\0" \
    "importenv=env import -t ${bootenv_addr} ${filesize}\0" \
    "boottgt=" \
        "if run loadenv_${tgt}; then " \
            "run importenv; " \
            "setenv loadmode ${tgt}; " \
            "run bootenv_cmd; " \
        "fi;\0" \
    "bootseq=" \
        "for tgt in ${boot_targets}; do " \
            "echo Attempting load of ${bootenv_file} from ${tgt}...; " \
            "run boottgt; " \
        "done; " \
        "echo No boot images found.  Aborting to prompt...\0" \

