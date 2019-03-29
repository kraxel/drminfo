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

    checksums = {
        'XR24' : '661a70b8dca5436443ce09014e6c326c',
        'RG16' : '0dcbe8573e0bf44bb7363cd22639f3b9',
    }

    def run_one_test(self, vga):
        modes = [
            "800x600",
            "1024x768",
            "1280x800",
            "1280x1024",
            "1920x1080",
        ]

        self.boot_gfx_vm(vga);
        self.console_prepare();

        self.console_run('drminfo -a')
        drminfo = self.console_wait('---root---')
        self.write_text(vga, "drminfo", drminfo)

        self.console_run('drminfo -F')
        formats = self.console_wait('---root---')

        fcount = 0;
        for format in formats.split():
            expected = None
            if format in self.checksums:
                expected = self.checksums[format]
            self.console_run('drmtest -a -s 10 -m 640x480 -f %s' % format)
            self.console_wait('---ok---', '---root---', 
                              'drm format error (%s)' % format)
            self.screen_dump(vga, "format-%s" % format, expected)
            self.console_wait('---root---')
            fcount += 1;

        if fcount == 0:
            self.fail("no drm formats");

        mcount = 0
        for mode in modes:
            if drminfo.find("mode: %s" % mode) < 0:
                continue
            self.console_run('drmtest -a -s 10 -m %s' % mode)
            self.console_wait('---ok---', '---root---', 
                              'drm mode error (%s)' % mode)
            self.screen_dump(vga, "mode-%s" % mode)
            self.console_wait('---root---')
            mcount += 1;

        if mcount == 0:
            self.fail("no drm modes");

        if vga == 'virtio-vga':
            self.console_run('virtiotest -i')
            self.console_wait('---root---')

            self.console_run('virtiotest -a -s 10')
            self.console_wait('---ok---', '---root---', 'virtiotest error')
            self.screen_dump(vga, 'virtio')
            self.console_wait('---root---')

        self.console_run('fbinfo')
        fbinfo = self.console_wait('---root---')
        self.write_text(vga, "fbinfo", fbinfo)

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
