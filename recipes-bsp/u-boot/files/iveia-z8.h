#define CONFIG_EXTRA_ENV_SETTINGS \
    "bootenv_addr=0x600000\0" \
    "fdt_addr=0x700000\0" \
    "kernel_addr=0x800000\0" \
    "initrd_addr=0x10000000\0" \
    "bootenv_file=uEnv.txt\0" \
    "boot_targets=mmc0 mmc1 dhcp\0" \
    "loadenv_mmc=load mmc ${sdbootdev} ${bootenv_addr} ${bootenv_file}\0" \
    "loadenv_mmc0=setenv sdbootdev 0; run loadenv_mmc;\0" \
    "loadenv_mmc1=setenv sdbootdev 1; run loadenv_mmc;\0" \
    "loadenv_dhcp=dhcp ${bootenv_addr} ${bootenv_file}\0" \
    "bootseq=" \
        "for t in ${boot_targets}; do " \
            "echo Attempting load of ${bootenv_file} from ${t}...; " \
            "if run loadenv_${t}; then " \
                "env import -t ${bootenv_addr} ${filesize}; " \
                "run bootenv_cmd; " \
            "fi; " \
        "done; " \
        "echo No boot images found.  Aborting to prompt...\0" \

