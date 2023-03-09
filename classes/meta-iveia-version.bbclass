addhandler on_build_start

META_IVEIA_VERSIONS_FILE := "${DEPLOY_DIR_IMAGE}/versions.txt"

on_build_start() {
    import re
    bblayers = d.getVar('BBLAYERS').split()
    pattern = "/meta-iveia(-[^/]*)?$"
    iveia_bblayers = [l for l in bblayers if re.search(pattern, l)]

    from subprocess import run, PIPE
    from datetime import datetime as dt 
    import os

    versions_file = d.getVar("META_IVEIA_VERSIONS_FILE")
    os.makedirs(os.path.dirname(versions_file), exist_ok=True)
    with open(versions_file, 'w') as f:
        f.write('IVEIA_MACHINE="{}"\n'.format(d.getVar("MACHINE")))
        f.write('IVEIA_BUILD_DATE="{}"\n'.format(dt.now().strftime("%Y-%m-%d_%H:%M:%S")))
        f.write('IVEIA_NUM_LAYERS="{}"\n'.format(len(iveia_bblayers)))
        for i, layer_path in enumerate(iveia_bblayers):
            layer_name = layer_path.split('/')[-1]
            f.write('IVEIA_META_{}_LAYER="{}"\n'.format(i + 1, layer_name))

            gitcmd = "git -C {} describe --long --tags --dirty".format(layer_path)
            completetion = run(gitcmd, shell=True, stdout=PIPE)
            first_line = completetion.stdout.decode().split('\n')[0]
            f.write('IVEIA_META_{}_BUILD_HASH="{}"\n'.format(i + 1, first_line))
}

on_build_start[eventmask] = "bb.event.BuildStarted"
