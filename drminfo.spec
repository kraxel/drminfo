Name:         drminfo
License:      GPLv2+
Version:      0
Release:      1%{?dist}
Summary:      drminfo
Group:        FIXME
URL:          http://www.kraxel.org/blog/linux/%{name}/
Source:       http://www.kraxel.org/releases/%{name}/%{name}-%{version}.tar.gz

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
install drminfo %{buildroot}%{_bindir}/drminfo
install drmtest %{buildroot}%{_bindir}/drmtest

%files
%{_bindir}/drminfo
%{_bindir}/drmtest
