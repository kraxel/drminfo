#
# cursor tests
#

# stdlib
import os
import time
from shutil import copyfile

# avocado
import avocado

# my bits
from drminfo import TestDRM

class cursor(TestDRM):
    """
    cursor test

    :avocado: tags=x86_64
    """

    timeout = 60

    def run_cursor_test(self, vga):

        self.boot_gfx_vm(vga);
        self.console_prepare();

        self.console_run('drmtest -a -s 10 --cursor')
        self.console_wait('---ok---', '---root---', 'cursor')
        self.screen_dump(vga, "cursor")
        self.console_wait('---root---')

    @avocado.skipUnless(os.path.exists('/usr/bin/dracut'), "no dracut")
    def setUp(self):
        TestDRM.setUp(self);
        if not os.path.isfile(self.initrd):
            self.prepare_kernel_initrd()

    def test_qxl_vga(self):
        """
        :avocado: tags=qxl
        """
        vga = 'qxl-vga'
        self.run_cursor_test(vga)

    def test_virtio_vga(self):
        """
        :avocado: tags=virtio
        """
        vga = 'virtio-vga'
        self.run_cursor_test(vga)
