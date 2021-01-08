# stdlib
import os
import re
import sys
import time
import logging
from glob import glob
from shutil import copyfile

# avocado
import avocado
from avocado.utils.process import run

# qemu
from qemu import QEMUMachine

# configuration
QEMU_BINARY =  os.environ.get("QEMU_BUILD_DIR");
QEMU_BUILD_DIR = os.environ.get("QEMU_BUILD_DIR");
LINUX_BUILD_DIR = os.environ.get("LINUX_BUILD_DIR");

class TestDRM(avocado.Test):

    rconsole = None
    wconsole = None

    def find_qemu_binary(self):
        """
        Picks the path of a QEMU binary, starting either in the current working
        directory or in the source tree root directory.
        """
        arch = os.uname()[4]
        qemu_path = [
            "/usr/local/bin/qemu-system-%s" % arch,
            "/usr/bin/qemu-system-%s" % arch,
            "/usr/bin/qemu-kvm",
            "/usr/libexec/qemu-kvm",
        ]
        if not QEMU_BUILD_DIR is None:
            qemu_build = os.path.join(QEMU_BUILD_DIR,
                                      "%s-softmmu" % arch,
                                      "qemu-system-%s" % arch)
            qemu_path.insert(0, qemu_build)
        if not QEMU_BINARY is None:
            qemu_path.insert(0, QEMU_BINARY)

        for item in qemu_path:
            if os.path.isfile(item):
                return item;
        return None

    def prepare_kernel_initrd(self):
        if LINUX_BUILD_DIR is None:
            kmoddir = None
            kversion = os.uname()[2]
            copyfile("/boot/vmlinuz-%s" % kversion, self.kernel)
        else:
            self.log.info("### install kernel modules (%s) for initrd"
                          % LINUX_BUILD_DIR)
            cmdline = "make -C %s" % LINUX_BUILD_DIR
            cmdline += " INSTALL_MOD_PATH=%s" % self.workdir
            cmdline += " modules_install"
            run(cmdline)
            kmoddir = glob("%s/lib/modules/*" % self.workdir)[0]
            kversion = os.path.basename(kmoddir)
            copyfile("%s/arch/x86/boot/bzImage" % LINUX_BUILD_DIR, self.kernel)

        drivers = [
            "cirrus",
            "bochs-drm",
            "qxl",
            "virtio-pci",
            "virtio-gpu",
            "vgem",
            "vkms",
        ]
        modules = [
            "base",
            "systemd",
            "systemd-initrd",
            "dracut-systemd",
            "bash",
            "drm",
            "rootfs-block",
        ]
        files = [
            "/usr/bin/drminfo",
            "/usr/bin/drmtest",
            "/usr/bin/fbinfo",
            "/usr/bin/fbtest",
            "/usr/bin/virtiotest",
            "/usr/bin/egltest",
            "/usr/bin/prime",
            "/usr/bin/edid-decode",
            "/usr/bin/strace",

            "/etc/fonts/fonts.conf",
            "/etc/fonts/conf.d/59-liberation-mono.conf",
            "/usr/share/fontconfig/conf.avail/59-liberation-mono.conf",
            "/usr/share/fonts/liberation/LiberationMono-Regular.ttf",
        ]
        rpms = [
            "mesa-libGL",
            "mesa-libEGL",
            "mesa-dri-drivers",
        ]

        # drop non-existing modules
        all_modules = run("dracut --list-modules").stdout.decode().split()
        module_list = []
        for module in modules:
            if module in all_modules:
                module_list.append(module)

        # add files from rpms
        for rpm in rpms:
            rpmfiles = run("rpm -ql %s" % rpm)
            for item in rpmfiles.stdout.decode().split():
                files.append(item)

        self.log.info("### create initrd for %s" % kversion)
        cmdline = "dracut"
        cmdline += " --no-hostonly"
        cmdline += " --force"
        if not kmoddir is None:
            cmdline += " --kmoddir \"%s\"" % kmoddir
        cmdline += " --modules \"%s\"" % " ".join(module_list)
        cmdline += " --drivers \"%s\"" % " ".join(drivers)
        for item in files:
            cmdline += " --install %s" % item
        cmdline += " \"%s\" \"%s\"" % (self.initrd, kversion)
        run(cmdline)

    def boot_gfx_vm(self, vga, display = None, vm = None, incoming = None, iommu = False, append = ""):
        append += " console=ttyS0"
        append += " rd.shell"
        append += " rd.break"

        self.log.info("### boot kernel with display device \"%s\"" % vga)
        if vm is None:
            vm = self.vm

        if iommu:
            vm.set_machine('q35,kernel-irqchip=split')
        else:
            vm.set_machine('pc')

        vm.set_console()
        vm.add_args('-enable-kvm')
        vm.add_args('-m', '1G')
        vm.add_args('-kernel', self.kernel)
        vm.add_args('-initrd', self.initrd)
        vm.add_args('-append', append)
        vm.add_args('-device', vga)

        if iommu:
            vm.add_args('-device', 'intel-iommu,intremap=on,device-iotlb=on')
            vm.add_args('-global', 'virtio-pci.iommu_platform=on')

        if not display is None:
            vm.add_args('-display', display)
        if not incoming is None:
            vm.add_args('-incoming', incoming)

        vm.launch()

        if self.rconsole is None:
            self.rconsole = vm.console_socket.makefile('r')
        if self.wconsole is None:
            self.wconsole = vm.console_socket.makefile('w')

    def console_prepare(self):
        self.lconsole = logging.getLogger('console')
        self.lcommand = logging.getLogger('command')

        self.console_wait('Entering emergency mode')
        self.console_run('PS1=---\\\\u---\\\\n')
        self.console_wait('---root---')

    def console_send(self, line = ""):
        self.wconsole.write(line)
        self.wconsole.write('\n')
        self.wconsole.flush()

    def console_run(self, command):
        self.lcommand.debug(command)
        self.console_send(command)
        self.rconsole.readline() # newline
        self.rconsole.readline() # command line echo

    def console_trace(self, name):
        while True:
            msg = self.rconsole.readline()
            self.lconsole.debug("%s: %s" % (name, msg.rstrip()))
            if '--[ end trace' in msg:
                break

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
            if 'Oops: ' in msg:
                self.console_trace("oops")
                self.fail("kernel oops")
            if 'WARNING: ' in msg:
                self.console_trace("warn")
                self.fail("kernel warn")
            if 'BUG ' in msg:
                #self.console_trace("bug")
                self.fail("kernel bug")
            if re.search('drm:.*ERROR', msg):
                self.fail("kernel drm error")
            if len(msg) == 0:
                if not self.vm.is_running():
                    self.fail("unexpected qemu exit %d" % self.vm.exitcode())
                else:
                    self.fail("unexpected console eof")
            if counter > 1000:
                self.fail("too much output")
            output += msg
        return output

    def html_append(self, html):
        outfile = '%s/_checkdata.html' % self.outputdir
        f = open(outfile, "a")
        f.write("<hr>\n")
        f.write(html)
        f.close()

    def screen_dump(self, vga, name, expected = None):
        if vga == 'qxl-vga' or vga == 'qxl':
            self.vm.qmp('screendump', filename = '/dev/null');
            time.sleep(0.1)
        out_ppm = '%s/%s-%s.ppm' % (self.outputdir, name, vga)
        out_jpg = '%s/%s-%s.jpg' % (self.outputdir, name, vga)
        self.vm.qmp('screendump', filename = out_ppm);
        self.wconsole.write('\n')
        self.wconsole.flush()
        checksum = run("md5sum %s" % out_ppm).stdout.decode().split()[0]
        self.log.debug("checksum: %s" % checksum)
        if not expected is None:
            self.log.debug("expected: %s" % expected)
            if checksum != expected:
                self.log.warning("checksum mismatch")
        if os.path.isfile("/usr/bin/convert"):
            run("/usr/bin/convert %s %s" % (out_ppm, out_jpg))
            os.remove(out_ppm)
        img = "<h3>%s-%s</h3>\n" % (name, vga)
        img += "<img src='%s-%s.jpg'>\n" % (name, vga)
        self.html_append(img)

    def write_text(self, vga, name, content):
        outfile = '%s/%s-%s.txt' % (self.outputdir, name, vga)
        f = open(outfile, "w")
        f.write(content)
        f.close()
        div = "<h3>%s-%s</h3>\n" % (name, vga)
        div += "<pre>%s</pre>" % content
        self.html_append(div)

    def setUp(self):
        self.jobdir = os.path.dirname(self.workdir)
        self.kernel = os.path.join(self.jobdir, "drminfo.kernel");
        self.initrd = os.path.join(self.jobdir, "drminfo.initrd");
        self.qemu_binary = self.find_qemu_binary()
        self.log.info("### using qemu binary: %s", self.qemu_binary)
        self.vm = QEMUMachine(self.qemu_binary)

    def tearDown(self):
        self.vm.shutdown()

