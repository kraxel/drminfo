# stdlib
import os
import sys
import time
import logging

# avocado
import avocado
from avocado.utils.process import run

# qemu
QEMU_BUILD_DIR = os.environ.get("QEMU_BUILD_DIR");
sys.path.append(os.path.join(QEMU_BUILD_DIR, 'python'))
from qemu import QEMUMachine

class TestDRM(avocado.Test):
    def find_qemu_binary(self):
        """
        Picks the path of a QEMU binary, starting either in the current working
        directory or in the source tree root directory.
        """
        arch = os.uname()[4]
        qemu_build = os.path.join(QEMU_BUILD_DIR,
                                  "%s-softmmu" % arch,
                                  "qemu-system-%s" % arch)
        qemu_path = [
            qemu_build,
            "/usr/local/bin/qemu-system-%s" % arch,
            "/usr/bin/qemu-system-%s" % arch,
            "/usr/bin/qemu-kvm",
            "/usr/libexec/qemu-kvm",
        ]
        for item in qemu_path:
            if os.path.isfile(item):
                return item;
        return None

    def create_initrd(self, outfile, kversion):
        modules = "base systemd bash drm"
        drivers = "cirrus bochs-drm qxl virtio-pci virtio-gpu vgem vkms"
        files = [
            "/usr/bin/drminfo",
            "/usr/bin/drmtest",
            "/usr/bin/fbinfo",
            "/usr/bin/fbtest",
            "/usr/bin/virtiotest",
            "/usr/bin/prime",
            "/usr/bin/edid-decode",
            "/usr/share/fontconfig/conf.avail/59-liberation-mono.conf",
            "/usr/share/fonts/liberation/LiberationMono-Regular.ttf",
        ]

        cmdline = "dracut"
        cmdline += " --force"
        cmdline += " --modules \"%s\"" % modules
        cmdline += " --drivers \"%s\"" % drivers
        for item in files:
            cmdline += " --install %s" % item
        cmdline += " \"%s\" \"%s\"" % (outfile, kversion)
        run(cmdline)

    def boot_gfx_vm(self, vga):
        append = "console=ttyS0"

        self.vm.set_machine('pc')
        self.vm.set_console()
        self.vm.add_args('-enable-kvm')
        self.vm.add_args('-m', '1G')
        self.vm.add_args('-vga', 'none')
        self.vm.add_args('-device', vga)
        self.vm.add_args('-kernel', self.kernel)
        self.vm.add_args('-initrd', self.initrd)
        self.vm.add_args('-append', append)
        self.vm.launch()

        self.rconsole = self.vm.console_socket.makefile('r')
        self.wconsole = self.vm.console_socket.makefile('w')
        self.lconsole = logging.getLogger('console')
        self.lcommand = logging.getLogger('command')

    def console_run(self, command):
        self.lcommand.debug(command)
        self.wconsole.write(command)
        self.wconsole.write('\n')
        self.wconsole.flush()
        self.rconsole.readline() # newline
        self.rconsole.readline() # command line echo

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

    def screen_dump(self, vga, name):
        if vga == 'qxl-vga':
            self.vm.qmp('screendump', filename = '/dev/null');
            time.sleep(0.1)
        outfile = '%s/%s-%s.ppm' % (self.outputdir, name, vga)
        self.vm.qmp('screendump', filename = outfile);
        self.wconsole.write('\n')
        self.wconsole.flush()

    def write_text(self, vga, name, content):
        outfile = '%s/%s-%s.txt' % (self.outputdir, name, vga)
        f = open(outfile, "w")
        f.write(content)
        f.close()

    def setUp(self):
        self.jobdir = os.path.dirname(self.workdir)
        self.kernel = os.path.join(self.jobdir, "drminfo.kernel");
        self.initrd = os.path.join(self.jobdir, "drminfo.initrd");
        self.vm = QEMUMachine(self.find_qemu_binary())

    def tearDown(self):
        self.vm.shutdown()
