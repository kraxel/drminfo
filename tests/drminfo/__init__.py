import os
import sys
import avocado

QEMU_BUILD_DIR = os.environ.get("QEMU_BUILD_DIR");
sys.path.append(os.path.join(QEMU_BUILD_DIR, 'python'))
from qemu import QEMUMachine

def find_qemu_binary():
    """
    Picks the path of a QEMU binary, starting either in the current working
    directory or in the source tree root directory.
    """
    arch = os.uname()[4]
    qemu_build = os.path.join(QEMU_BUILD_DIR,
                              "%s-softmmu" % arch,
                              "qemu-system-%s" % arch)
    qemu_path = [
        qemu_build,
        "/usr/local/bin/qemu-system-%s" % arch,
        "/usr/bin/qemu-system-%s" % arch,
        "/usr/bin/qemu-kvm",
        "/usr/libexec/qemu-kvm",
    ]
    for item in qemu_path:
        if os.path.isfile(item):
            return item;
    return None

class TestDRM(avocado.Test):
    def setUp(self):
        self.vm = QEMUMachine(find_qemu_binary())

    def tearDown(self):
        self.vm.shutdown()

