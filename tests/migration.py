#
# live migration tests
#

# stdlib
import os

# avocado
import avocado
from avocado.utils import network
from avocado.utils import wait

# qemu
from qemu import QEMUMachine

# my bits
from drminfo import TestDRM

class Migration(TestDRM):
    """
    qemu display device migration tests

    :avocado: tags=x86_64
    """

    timeout = 60

    @staticmethod
    def migration_finished(vm):
        return vm.command('query-migrate')['status'] in ('completed', 'failed')

    def _get_free_port(self):
        port = network.find_free_port()
        if port is None:
            self.cancel('Failed to find a free port')
        return port

    def migrate_vm(self, vga, display = None):
        self.log.info("### live migration: start ...")
        dest_uri = 'tcp:localhost:%u' % self._get_free_port()
        dest_vm = QEMUMachine(self.qemu_binary)
        self.boot_gfx_vm(vga, display, dest_vm, dest_uri);
        self.vm.qmp('migrate_set_speed', value = "1G")
        self.vm.qmp('migrate', uri=dest_uri)
        wait.wait_for(
            self.migration_finished,
            timeout=self.timeout,
            step=0.1,
            args=(self.vm,)
        )
        self.assertEqual(dest_vm.command('query-migrate')['status'], 'completed')
        self.assertEqual(self.vm.command('query-migrate')['status'], 'completed')
        self.assertEqual(dest_vm.command('query-status')['status'], 'running')
        self.assertEqual(self.vm.command('query-status')['status'], 'postmigrate')
        self.log.info("### live migration: OK")

        # swap vm and console handles, so we can talk to the new vm ...
        self.vm.shutdown()
        self.vm = dest_vm
        self.rconsole = self.vm.console_socket.makefile('r')
        self.wconsole = self.vm.console_socket.makefile('w')

    def migration_test(self, vga):
        self.boot_gfx_vm(vga);
        self.console_prepare();

        self.console_run('drminfo -a')
        drminfo = self.console_wait('---root---')
        self.write_text(vga, "drminfo-pre-migration", drminfo)
        if not "framebuffer formats" in drminfo:
            self.fail("drm device missing");

        self.migrate_vm(vga)

        self.console_run('drminfo -a')
        drminfo = self.console_wait('---root---')
        self.write_text(vga, "drminfo-post-migration", drminfo)
        if not "framebuffer formats" in drminfo:
            self.fail("drm device missing");

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
        self.migration_test(vga)

    def test_bochs_dpy(self):
        """
        :avocado: tags=bochs
        """
        vga = 'bochs-display'
        self.migration_test(vga)

    def test_cirrus(self):
        """
        :avocado: tags=cirrus
        """
        vga = "cirrus-vga"
        self.migration_test(vga)

    def test_qxl(self):
        """
        :avocado: tags=qxl
        """
        vga = "qxl-vga"
        self.migration_test(vga)

    def test_virtio_vga(self):
        """
        :avocado: tags=virtio
        """
        vga = "virtio-vga"
        self.migration_test(vga)
