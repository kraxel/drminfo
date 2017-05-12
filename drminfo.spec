Name:         drminfo
License:      GPLv2+
Version:      4
Release:      1%{?dist}
Summary:      drminfo
Group:        FIXME
URL:          http://www.kraxel.org/blog/linux/%{name}/
Source:       http://www.kraxel.org/releases/%{name}/%{name}-%{version}.tar.gz

Requires:     font(liberationmono)

BuildRequires: meson ninja-build
BuildRequires: libjpeg-devel
BuildRequires: pkgconfig(libdrm)
BuildRequires: pkgconfig(gbm)
BuildRequires: pkgconfig(epoxy)
BuildRequires: pkgconfig(cairo)
BuildRequires: pkgconfig(cairo-gl)
BuildRequires: pkgconfig(pixman-1)

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

%files
%{_bindir}/drm*
%{_mandir}/man1/drm*.1*
