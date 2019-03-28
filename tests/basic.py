#
# basic qemu display device tests
#

# stdlib
import os
import time
import logging
import tempfile

# avocado
import avocado
from avocado.utils.process import run

# my bits
from drminfo import TestDRM

class BaseDRM(TestDRM):
    """
    basic qemu display device tests

    :avocado: tags=x86_64
    """

    def create_initrd(self, outfile, kversion):
        modules = "base systemd bash drm"
        drivers = "cirrus bochs-drm qxl virtio-pci virtio-gpu vgem vkms"

        cmdline = "dracut"
        cmdline += " --force"
        cmdline += " --modules \"%s\"" % modules
        cmdline += " --drivers \"%s\"" % drivers
        cmdline += " --install /usr/bin/drminfo"
        cmdline += " --install /usr/bin/drmtest"
        cmdline += " --install /usr/bin/fbinfo"
        cmdline += " --install /usr/bin/fbtest"
        cmdline += " --install /usr/bin/virtiotest"
        cmdline += " --install /usr/bin/prime"
        cmdline += " --install /usr/share/fontconfig/conf.avail/59-liberation-mono.conf"
        cmdline += " --install /usr/share/fonts/liberation/LiberationMono-Regular.ttf"
        cmdline += " \"%s\" \"%s\"" % (outfile, kversion)
        run(cmdline)

    def console_wait(self, good, bad = None, errmsg = None):
        output = ""
        counter = 1
        while True:
            msg = self.rconsole.readline()
            self.lconsole.debug("%3d: %s" % (counter, msg.rstrip()))
            counter += 1
            if good in msg:
                break
            if not bad is None:
                if bad in msg:
                    if errmsg is None:
                        errmsg = "unexpected output (%s)" % bad
                    self.fail(errmsg)
            if 'Kernel panic - not syncing' in msg:
                self.fail("kernel panic")
            output += msg
        return output

    def console_run(self, command):
        self.lcommand.debug(command)
        self.wconsole.write(command)
        self.wconsole.write('\n')
        self.wconsole.flush()
        self.rconsole.readline() # newline
        self.rconsole.readline() # command line echo

    def screen_dump(self, device, name):
        if device == 'qxl-vga':
            self.vm.qmp('screendump', filename = '/dev/null');
            time.sleep(1)
        outfile = '%s/%s-%s.ppm' % (self.outputdir, name, device)
        self.vm.qmp('screendump', filename = outfile);
        self.wconsole.write('\n')
        self.wconsole.flush()

    def run_one_test(self, device):
        version = os.uname()[2]
        kernel = "/boot/vmlinuz-%s" % version
        append = "console=ttyS0"

        self.vm.set_machine('pc')
        self.vm.set_console()
        self.vm.add_args('-enable-kvm')
        self.vm.add_args('-m', '1G')
        self.vm.add_args('-vga', 'none')
        self.vm.add_args('-device', device)
        self.vm.add_args('-kernel', kernel)
        self.vm.add_args('-initrd', self.dracut.name)
        self.vm.add_args('-append', append)
        self.vm.launch()

        self.rconsole = self.vm.console_socket.makefile('r')
        self.wconsole = self.vm.console_socket.makefile('w')
        self.lconsole = logging.getLogger('console')
        self.lcommand = logging.getLogger('command')

        self.console_wait('Entering emergency mode')

        self.console_run('PS1=---\\\\u---\\\\n')
        self.console_wait('---root---')

        self.console_run('drminfo -a')
        self.console_wait('---root---')

        self.console_run('drminfo -F')
        formats = self.console_wait('---root---')

        fcount = 0;
        for format in formats.split():
            self.console_run('drmtest -a -s 10 -m 640x480 -f %s' % format)
            self.console_wait('---ok---', '---root---', 
                              'drmtest failed (%s)' % format)
            self.screen_dump(device, "drm-%s" % format)
            self.console_wait('---root---')
            fcount += 1;

        if fcount == 0:
            self.fail("no drm formats");

        if device == 'virtio-vga':
            self.console_run('virtiotest -i')
            self.console_wait('---root---')

            self.console_run('virtiotest -a -s 10')
            self.console_wait('---ok---', '---root---', 'virtiotest failed')
            self.screen_dump(device, 'virtio')
            self.console_wait('---root---')

        self.console_run('fbinfo -a')
        self.console_wait('---root---')

        self.console_run('fbtest -a -s 10')
        self.console_wait('---ok---', '---root---', 'fbtest failed')
        self.screen_dump(device, 'fbdev')
        self.console_wait('---root---')

    @avocado.skipUnless(os.path.exists('/usr/bin/dracut'), "no dracut")
    @avocado.skipUnless(os.path.exists('/usr/bin/drminfo'), "no drminfo")
    def setUp(self):
        version = os.uname()[2]
        self.dracut = tempfile.NamedTemporaryFile()
        self.create_initrd(self.dracut.name, version)
        TestDRM.setUp(self);

    def test_stdvga(self):
        self.run_one_test('VGA')

    def test_cirrus(self):
        self.run_one_test('cirrus-vga')

    def test_qxl(self):
        self.run_one_test('qxl-vga')

    def test_virtio(self):
        self.run_one_test('virtio-vga')
