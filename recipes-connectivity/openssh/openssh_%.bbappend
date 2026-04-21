do_install:append() {
    # UN-hardening to allow SSH root login
    for cfg in PermitRootLogin PermitEmptyPasswords PasswordAuthentication; do
        sed -i "/\<${cfg}\>/d" ${D}${sysconfdir}/ssh/sshd_config
        echo "${cfg} yes" >> ${D}${sysconfdir}/ssh/sshd_config
    done
}
