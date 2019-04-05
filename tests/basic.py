#
# basic qemu display device tests
#

# stdlib
import os
import time

# avocado
import avocado

# my bits
from drminfo import TestDRM

class BaseDRM(TestDRM):
    """
    basic qemu display device tests

    :avocado: tags=x86_64
    """

    timeout = 60

    checksums = {
        'AR24' : 'adf315fe78e8f7e2947d65158545b4d3',
        'XR24' : '661a70b8dca5436443ce09014e6c326c',
        'BX24' : '0ab929a5c0ccd0123c6a64fe6fdcc24f',
        'RG24' : 'a250b72b15cbb53306ed9e101aac3600',
        'RG16' : '0dcbe8573e0bf44bb7363cd22639f3b9',
    }
    modes = [
        "800x600",
        "1024x768",
        "1280x800",
        "1600x900",
        "1920x1080",
    ]

    def common_tests(self, vga, display = None):

        self.boot_gfx_vm(vga, display);
        self.console_prepare();

        self.console_run('drminfo -a')
        drminfo = self.console_wait('---root---')
        self.write_text(vga, "drminfo", drminfo)
        if not "framebuffer formats" in drminfo:
            self.fail("drm device missing");

        self.console_run('drminfo -F')
        formats = self.console_wait('---root---')

        fcount = 0;
        for format in formats.split():
            expected = None
            if format in self.checksums:
                expected = self.checksums[format]
            self.console_run('drmtest -a -s 10 -m 640x480 -f %s' % format)
            self.console_wait('---ok---', '---root---', 
                              'drm format (%s)' % format)
            self.screen_dump(vga, "format-%s" % format, expected)
            self.console_wait('---root---')
            fcount += 1;

        if fcount == 0:
            self.fail("no drm formats");

        mcount = 0
        for mode in self.modes:
            if drminfo.find("mode: %s" % mode) < 0:
                continue
            self.console_run('drmtest -a -s 10 -m %s' % mode)
            self.console_wait('---ok---', '---root---', 
                              'drm mode (%s)' % mode)
            self.screen_dump(vga, "mode-%s" % mode)
            self.console_wait('---root---')
            mcount += 1;

        if mcount == 0:
            self.fail("no drm modes");

        self.console_run('fbinfo')
        fbinfo = self.console_wait('---root---')
        self.write_text(vga, "fbinfo", fbinfo)

        self.console_run('fbtest -a -s 10')
        self.console_wait('---ok---', '---root---', 'fbtest')
        time.sleep(0.1)
        self.screen_dump(vga, 'fbdev')
        self.console_wait('---root---')

    def prime_tests(self, vga):
        self.console_run('prime')
        self.console_wait('---root---')

    def virtio_tests(self, vga):
        self.console_run('virtiotest -i -l')
        virtcaps = self.console_wait('---root---')
        self.write_text(vga, "virtcaps", virtcaps)

        self.console_run('virtiotest -a -s 10')
        self.console_wait('---ok---', '---root---', 'virtiotest')
        self.screen_dump(vga, 'virtio')
        self.console_wait('---root---')

    def virgl_tests(self, vga):
        self.console_run('egltest -i')
        eglinfo = self.console_wait('---root---')
        self.write_text(vga, "eglinfo", eglinfo)
        if not "virgl" in eglinfo:
            self.fail("virgl not available");

    @avocado.skipUnless(os.path.exists('/usr/bin/dracut'), "no dracut")
    @avocado.skipUnless(os.path.exists('/usr/bin/drminfo'), "no drminfo")
    def setUp(self):
        TestDRM.setUp(self);
        if not os.path.isfile(self.initrd):
            self.prepare_kernel_initrd()

    def test_stdvga(self):
        vga = 'VGA'
        self.common_tests(vga)

    def test_bochs_dpy(self):
        vga = 'bochs-display'
        self.common_tests(vga)

    def test_cirrus(self):
        vga = 'cirrus-vga'
        self.common_tests(vga)

    def test_qxl_vga(self):
        vga = 'qxl-vga'
        self.common_tests(vga)
        self.prime_tests(vga)

    def test_qxl(self):
        vga = 'qxl'
        self.common_tests(vga)
        self.prime_tests(vga)

    def test_virtio_vga(self):
        vga = 'virtio-vga'
        self.common_tests(vga)
        self.prime_tests(vga)
        self.virtio_tests(vga)

    def test_virtio_gpu(self):
        vga = 'virtio-gpu-pci'
        self.common_tests(vga)
        self.prime_tests(vga)
        self.virtio_tests(vga)

    def test_virgl(self):
        vga = 'virtio-vga'
        self.common_tests(vga, 'egl-headless')
        self.prime_tests(vga)
        self.virtio_tests(vga)
        self.virgl_tests(vga)
