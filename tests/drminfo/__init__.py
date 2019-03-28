import os
import sys
import avocado

QEMU_BUILD_DIR = os.environ.get("QEMU_BUILD_DIR");
sys.path.append(os.path.join(QEMU_BUILD_DIR, 'python'))
from qemu import QEMUMachine

class TestDRM(avocado.Test):
    def setUp(self):
        self.vm = QEMUMachine("/usr/local/bin/qemu-system-x86_64")

    def tearDown(self):
        self.vm.shutdown()

