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
    basic qemu display device tests

    :avocado: tags=x86_64
    """

    def run_one_test(self, vga):

        self.boot_gfx_vm(vga);
        self.console_wait('Entering emergency mode')
        self.console_run('PS1=---\\\\u---\\\\n')
        self.console_wait('---root---')

        self.console_run('edid-decode /sys/class/drm/card0-Virtual-1/edid')
        edid = self.console_wait('---root---')
        vganame = vga.split(",")[0];
        self.write_text(vganame, "edid", edid)
        if edid.find("QEMU Monitor") < 0:
            self.fail("edid not valid")

    @avocado.skipUnless(os.path.exists('/usr/bin/dracut'), "no dracut")
    @avocado.skipUnless(os.path.exists('/usr/bin/drminfo'), "no drminfo")
    @avocado.skipUnless(os.path.exists('/usr/bin/edid-decode'), "no edid-decode")
    def setUp(self):
        TestDRM.setUp(self);
        version = os.uname()[2]
        if not os.path.isfile(self.initrd):
            self.create_initrd(self.initrd, version)
        if not os.path.isfile(self.kernel):
            copyfile("/boot/vmlinuz-%s" % version, self.kernel)

    def test_stdvga(self):
        self.run_one_test('VGA,edid=on')

    def test_virtio(self):
        self.run_one_test('virtio-vga,edid=on')
