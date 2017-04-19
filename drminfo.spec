Name:         drminfo
License:      GPLv2+
Version:      1
Release:      1%{?dist}
Summary:      drminfo
Group:        FIXME
URL:          http://www.kraxel.org/blog/linux/%{name}/
Source:       http://www.kraxel.org/releases/%{name}/%{name}-%{version}.tar.gz

Requires:     font(liberationmono)

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
make drminfo drmtest

%install
install -d %{buildroot}%{_bindir}
install -d %{buildroot}%{_mandir}/man1
install drminfo %{buildroot}%{_bindir}/drminfo
install drmtest %{buildroot}%{_bindir}/drmtest
install -m644 drminfo.man %{buildroot}%{_mandir}/man1/drminfo.1
install -m644 drmtest.man %{buildroot}%{_mandir}/man1/drmtest.1

%files
%{_bindir}/drminfo
%{_bindir}/drmtest
