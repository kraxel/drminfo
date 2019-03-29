#
# basic qemu display device tests
#

# stdlib
import os

# avocado
import avocado

# my bits
from drminfo import TestDRM

class BaseDRM(TestDRM):
    """
    basic qemu display device tests

    :avocado: tags=x86_64
    """

    def run_one_test(self, vga):

        self.boot_gfx_vm(vga);
        self.console_wait('Entering emergency mode')
        self.console_run('PS1=---\\\\u---\\\\n')
        self.console_wait('---root---')

        self.console_run('drminfo -a')
        txt = self.console_wait('---root---')
        self.write_text(vga, "drminfo", txt)

        self.console_run('drminfo -F')
        formats = self.console_wait('---root---')

        fcount = 0;
        for format in formats.split():
            self.console_run('drmtest -a -s 10 -m 640x480 -f %s' % format)
            self.console_wait('---ok---', '---root---', 
                              'drmtest error (%s)' % format)
            self.screen_dump(vga, "drm-%s" % format)
            self.console_wait('---root---')
            fcount += 1;

        if fcount == 0:
            self.fail("no drm formats");

        if vga == 'virtio-vga':
            self.console_run('virtiotest -i')
            self.console_wait('---root---')

            self.console_run('virtiotest -a -s 10')
            self.console_wait('---ok---', '---root---', 'virtiotest error')
            self.screen_dump(vga, 'virtio')
            self.console_wait('---root---')

        self.console_run('fbinfo')
        txt = self.console_wait('---root---')
        self.write_text(vga, "fbinfo", txt)

        self.console_run('fbtest -a -s 10')
        self.console_wait('---ok---', '---root---', 'fbtest error')
        self.screen_dump(vga, 'fbdev')
        self.console_wait('---root---')

    @avocado.skipUnless(os.path.exists('/usr/bin/dracut'), "no dracut")
    @avocado.skipUnless(os.path.exists('/usr/bin/drminfo'), "no drminfo")
    @avocado.skipUnless(os.path.exists('/usr/bin/edid-decode'), "no edid-decode")
    def setUp(self):
        TestDRM.setUp(self);
        if not os.path.isfile(self.initrd):
            self.prepare_kernel_initrd()

    def test_stdvga(self):
        self.run_one_test('VGA')

    def test_cirrus(self):
        self.run_one_test('cirrus-vga')

    def test_qxl(self):
        self.run_one_test('qxl-vga')

    def test_virtio(self):
        self.run_one_test('virtio-vga')
