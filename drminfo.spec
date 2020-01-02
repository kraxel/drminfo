Name:         drminfo
License:      GPLv2+
Version:      7
Release:      1%{?dist}
Summary:      drminfo
Group:        FIXME
URL:          http://www.kraxel.org/blog/linux/%{name}/
Source:       http://www.kraxel.org/releases/%{name}/%{name}-%{version}.tar.gz

Requires:     font(liberationmono)

BuildRequires: meson ninja-build gcc
BuildRequires: libjpeg-devel
BuildRequires: pkgconfig(libdrm)
BuildRequires: pkgconfig(gbm)
BuildRequires: pkgconfig(epoxy)
BuildRequires: pkgconfig(cairo)
BuildRequires: pkgconfig(pixman-1)
BuildRequires: pkgconfig(gtk+-3.0)
BuildRequires: pkgconfig(libsystemd)
BuildRequires: pkgconfig(xcb)
BuildRequires: pkgconfig(xcb-randr)
BuildRequires: pkgconfig(libudev)
BuildRequires: pkgconfig(libinput)

%description
drminfo - print drm device props
drmtest - some drm test app
egltest - opengl info and test app
fbinfo - print some fbdev device info
fbtest - simple fbdev device test
prime - some dma-buf sharing tests
virtiotest - some virtio-gpu tests

%prep
%setup -q

%build
export CFLAGS="%{optflags}"
meson --prefix=%{_prefix} build-rpm
ninja-build -C build-rpm

%install
export DESTDIR=%{buildroot}
ninja-build -C build-rpm install
mkdir -p %{buildroot}%{_datadir}/%{name}
cp -a tests %{buildroot}%{_datadir}/%{name}

mkdir -p %{buildroot}/etc/bash_completion.d
for tool in drminfo drmtest egltest fbinfo fbtest gtktest prime virtiotest
do
	build-rpm/$tool --complete-bash \
		>> %{buildroot}/etc/bash_completion.d/drminfo
done

%files
/etc/bash_completion.d/drminfo
%{_bindir}/drm*
%{_bindir}/prime
%{_bindir}/virtiotest
%{_bindir}/egltest
%{_bindir}/fb*
%{_mandir}/man1/drm*.1*
%{_datadir}/%{name}/tests
