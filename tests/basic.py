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

    igd_vgpu_uuid = "656527d9-c2c9-4745-b16a-00419a5bc32e"
    igd_vgpu_path = "/sys/class/mdev_bus/0000:00:02.0/%s" % igd_vgpu_uuid

    checksums = {
        'AR24' : 'ea39d6f3a207d810c89312316c955c9f',
        'XR24' : '78b3200ce3d3c7f56b1c4b3ed2651592',
        'BX24' : 'b14c03c1d8c1ff483bd7b6c229772d26',
        'RG24' : 'f35b32ed2515a3b9138e3ed958f8640f',
        'RG16' : 'a47cf725172c81d2edf38f2b2544c5aa',
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

        self.console_run('egltest -a -s 10')
        self.console_wait('---ok---', '---root---', 'egltest')
        self.screen_dump(vga, 'egl')
        self.console_wait('---root---')

    @avocado.skipUnless(os.path.exists('/usr/bin/dracut'), "no dracut")
    @avocado.skipUnless(os.path.exists('/usr/bin/drminfo'), "no drminfo")
    def setUp(self):
        TestDRM.setUp(self);
        if not os.path.isfile(self.initrd):
            self.prepare_kernel_initrd()

    def test_stdvga(self):
        """
        :avocado: tags=bochs
        """
        vga = 'VGA'
        self.common_tests(vga)

    def test_bochs_dpy(self):
        """
        :avocado: tags=bochs
        """
        vga = 'bochs-display'
        self.common_tests(vga)

    def test_cirrus(self):
        """
        :avocado: tags=cirrus
        """
        vga = 'cirrus-vga'
        self.common_tests(vga)

    def test_qxl_vga(self):
        """
        :avocado: tags=qxl
        """
        vga = 'qxl-vga'
        self.common_tests(vga)
        self.prime_tests(vga)

    def test_qxl(self):
        """
        :avocado: tags=qxl
        """
        vga = 'qxl'
        self.common_tests(vga)
        self.prime_tests(vga)

    def test_virtio_vga(self):
        """
        :avocado: tags=virtio
        """
        vga = 'virtio-vga'
        self.common_tests(vga)
        self.prime_tests(vga)
        self.virtio_tests(vga)

    def test_virtio_gpu(self):
        """
        :avocado: tags=virtio
        """
        vga = 'virtio-gpu-pci'
        self.common_tests(vga)
        self.prime_tests(vga)
        self.virtio_tests(vga)

    def test_virgl(self):
        """
        :avocado: tags=virtio
        """
        vga = 'virtio-vga'
        self.common_tests(vga, 'egl-headless')
        self.prime_tests(vga)
        self.virtio_tests(vga)
#        self.virgl_tests(vga)

    @avocado.skipUnless(os.path.exists(igd_vgpu_path), "no vgpu")
    def test_vgpu_igd(self):
        """
        :avocado: tags=igd
        """
        vga = 'vfio-pci,display=on,sysfsdev=%s' % self.igd_vgpu_path
        self.common_tests(vga, 'egl-headless')
