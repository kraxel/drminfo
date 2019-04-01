#
# edid tests
#

# stdlib
import os
import time
from shutil import copyfile

# avocado
import avocado

# my bits
from drminfo import TestDRM

class unload(TestDRM):
    """
    drm module unload test

    :avocado: tags=x86_64
    """

    timeout = 60

    def run_unload_test(self, vga, module):

        self.boot_gfx_vm(vga);
        self.console_prepare();

        self.console_run('for vt in /sys/class/vtconsole/vtcon*; do echo 0 > $vt/bind; done')
        self.console_wait('---root---')
        self.console_run('rmmod %s' % module)
        self.console_wait('---root---', 'rmmod: ERROR: ', 'rmmod')

    @avocado.skipUnless(os.path.exists('/usr/bin/dracut'), "no dracut")
    def setUp(self):
        TestDRM.setUp(self);
        if not os.path.isfile(self.initrd):
            self.prepare_kernel_initrd()

    def test_stdvga(self):
        vga = "VGA"
        mod = "bochs-drm"
        self.run_unload_test(vga, mod)

    def test_cirrus(self):
        vga = "cirrus-vga"
        mod = "cirrus"
        self.run_unload_test(vga, mod)

    @avocado.skip("known broken")
    def test_qxl_vga(self):
        vga = "qxl-vga"
        mod = "qxl"
        self.run_unload_test(vga, mod)

    def test_virtio_vga(self):
        vga = "virtio-vga"
        mod = "virtio-gpu"
        self.run_unload_test(vga, mod)
