# TCF source fails to download when using https - use default
SRC_URI_NO_PROTOCOL = "git://git.eclipse.org/gitroot/tcf/org.eclipse.tcf.agent.git;branch=master"
SRC_URI_remove = "${SRC_URI_NO_PROTOCOL};protocol=https"
SRC_URI_prepend = "${SRC_URI_NO_PROTOCOL} "
