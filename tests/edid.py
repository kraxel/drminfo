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

class EDID(TestDRM):
    """
    edid test

    :avocado: tags=x86_64
    """

    timeout = 60

    def run_edid_test(self, vga):

        self.boot_gfx_vm("%s,edid=on" % vga);
        self.console_prepare();

        self.console_run('edid-decode /sys/class/drm/card0-Virtual-1/edid')
        edid = self.console_wait('---root---')
        self.write_text(vga, "edid", edid)
        if edid.find("QEMU Monitor") < 0:
            self.fail("edid not valid")

    @avocado.skipUnless(os.path.exists('/usr/bin/dracut'), "no dracut")
    @avocado.skipUnless(os.path.exists('/usr/bin/edid-decode'), "no edid-decode")
    def setUp(self):
        TestDRM.setUp(self);
        if not os.path.isfile(self.initrd):
            self.prepare_kernel_initrd()

    def test_stdvga(self):
        vga = "VGA"
        self.run_edid_test(vga)

    def test_bochs_dpy(self):
        vga = 'bochs-display'
        self.run_edid_test(vga)

    def test_virtio_vga(self):
        vga = 'virtio-vga'
        self.run_edid_test(vga)

    def test_virtio_gpu(self):
        vga = 'virtio-gpu-pci'
        self.run_edid_test(vga)
