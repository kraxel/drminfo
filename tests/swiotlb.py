#
# swiotlb tests
#

# stdlib
import os
import time
from shutil import copyfile

# avocado
import avocado

# my bits
from drminfo import TestDRM

class swiotlb(TestDRM):
    """
    swiotlb test

    :avocado: tags=x86_64
    """

    timeout = 60

    def run_swiotlb_test(self, vga):

        self.boot_gfx_vm(vga, iommu = True, append = "swiotlb=force");
        self.console_prepare();

        self.console_run('drmtest -a -s 10')
        self.console_wait('---ok---', '---root---', 'swiotlb')
        self.screen_dump(vga, 'swiotlb', None)
        self.console_wait('---root---')

    @avocado.skipUnless(os.path.exists('/usr/bin/dracut'), "no dracut")
    def setUp(self):
        TestDRM.setUp(self);
        if not os.path.isfile(self.initrd):
            self.prepare_kernel_initrd()

    def test_virtio_vga(self):
        """
        :avocado: tags=virtio
        """
        vga = 'virtio-vga'
        self.run_swiotlb_test(vga)

    def test_virtio_gpu(self):
        """
        :avocado: tags=virtio
        """
        vga = 'virtio-gpu-pci'
        self.run_swiotlb_test(vga)
