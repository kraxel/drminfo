Name:         drminfo
License:      GPLv2+
Version:      5
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

%description
drminfo - print drm device props
drmtest - simple drm test app

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

%files
%{_bindir}/drm*
%{_bindir}/prime
%{_bindir}/virtiotest
%{_bindir}/fb*
%{_mandir}/man1/drm*.1*
%{_datadir}/%{name}/tests
