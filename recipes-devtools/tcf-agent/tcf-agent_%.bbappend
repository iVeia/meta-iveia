# TCF source fails to download when using https - use default
SRC_URI_BAD = \
    "git://git.eclipse.org/gitroot/tcf/org.eclipse.tcf.agent.git;branch=master;protocol=https"
SRC_URI_GOOD = \
    "git://git.eclipse.org/r/tcf/org.eclipse.tcf.agent.git;branch=master;protocol=https"
SRC_URI:remove = "${SRC_URI_BAD}"
SRC_URI:prepend = "${SRC_URI_GOOD} "
