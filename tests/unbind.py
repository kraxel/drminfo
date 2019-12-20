#
# driver unbind tests
#

# stdlib
import os
import time
from shutil import copyfile

# avocado
import avocado

# my bits
from drminfo import TestDRM

class unbind(TestDRM):
    """
    drm module unbind test

    :avocado: tags=x86_64
    """

    timeout = 60

    def run_unbind_test(self, vga):

        self.boot_gfx_vm(vga);
        self.console_prepare();

        self.console_run('drmtest -a -s 10 --unbind')
        self.console_wait('---ok---', '---root---', 'unbind')
        self.screen_dump(vga, "unbind")
        self.console_wait('---root---')

    @avocado.skipUnless(os.path.exists('/usr/bin/dracut'), "no dracut")
    def setUp(self):
        TestDRM.setUp(self);
        if not os.path.isfile(self.initrd):
            self.prepare_kernel_initrd()

    def test_stdvga(self):
        vga = "VGA"
        self.run_unbind_test(vga)

    def test_cirrus(self):
        vga = "cirrus-vga"
        self.run_unbind_test(vga)

    def test_qxl_vga(self):
        vga = "qxl-vga"
        self.run_unbind_test(vga)

    def test_virtio_vga(self):
        vga = "virtio-vga"
        self.run_unbind_test(vga)
