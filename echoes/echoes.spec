#
# spec file for package echoes
#
# Copyright (c) 2019 giuseppe massimo bertani <gmbertani@users.sourceforge.net>
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via https://bugs.opensuse.org/
#

Name:           echoes
Version:        0.62
Release:        1
Summary:        RF spectrograph for meteor scatter
License:        GPL-3.0-or-later
Group:          Productivity/Scientific/Astronomy
Url:            https://sourceforge.net/projects/echoes
Source:         echoes-0.62.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-build


BuildRequires:  pulseaudio
BuildRequires:  pulseaudio-utils
BuildRequires:  libliquid-devel >= 1.3.2
BuildRequires:  hicolor-icon-theme
BuildRequires:  libqt5-qttools-devel
# BuildRequires:  update-desktop-files
BuildRequires:  pkgconfig(Qt5Charts)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(Qt5Multimedia)
BuildRequires:  pkgconfig(Qt5MultimediaWidgets)
BuildRequires:  pkgconfig(libusb-1.0)
BuildRequires:  pkgconfig(SoapySDR)   


%description
Echoes is a RF spectrograph designed for meteor scatter purposes. It relies on SoapySDR library for audio devices and SDR devices support.


%prep

%setup
qmake-qt5 echoes.pro -r -spec linux-g++  AUX_INSTALL_DIR=/usr/share BIN_INSTALL_DIR=/usr/bin APP_VERSION=0.62 BUILD_DATE=`date --iso-8601`

%build
%make_build

%install
%make_install INSTALL_ROOT=%{buildroot}
mkdir -p %{buildroot}%{_datadir}/echoes/langs
install -m 644 .qm/*.qm %{buildroot}%{_datadir}/echoes/langs/
ln -sf /usr/bin/echoes %{buildroot}/usr/bin/console_echoes


%files 
%defattr(-,root,root,-)
%dir %{_datadir}/echoes
%dir %{_datadir}/fonts/truetype
%dir %{_datadir}/echoes/langs

# Eseguibili
/usr/bin/echoes
/usr/bin/console_echoes

# File di supporto installati in modo piatto in /usr/share/echoes/
/usr/share/echoes/license.txt
/usr/share/echoes/echoes.sqlite3

# Traduzioni (file .qm)
%{_datadir}/echoes/langs/*.qm

# File .rts installati in modo piatto (uso il pattern per coprire tutti i file di test):
%{_datadir}/echoes/*.rts
/usr/share/echoes/README.txt

# Desktop file
/usr/share/applications/echoes.desktop

# Fonts
/usr/share/fonts/truetype/Gauge-Oblique.ttf
/usr/share/fonts/truetype/Gauge-Regular.ttf
/usr/share/fonts/truetype/Gauge-Heavy.ttf

# Icone e Suoni
/usr/share/icons/hicolor/32x32/apps/echoes_xpm32.xpm
/usr/share/sounds/ping.wav

%changelog
* Wed Nov 12 2025 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.62-0
- Upstream version 0.62

* Wed Jun 25 2025 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.61-0
- Upstream version 0.61

* Tue Dec 24 2024 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.60-1
- Upstream version 0.60

* Mon Dec 16 2024 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.58-12
- Upstream version 0.58

* Mon Jul 15 2024 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.57-3
- Upstream version 0.57

* Fri May 24 2024 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.56-6
- Upstream version 0.56

* Thu Mar 21 2024 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.55-1
- Upstream version 0.55

* Sun Jan 19 2024 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.59-1
- Upstream version 0.59

* Tue Jan 02 2024 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.54-3
- Upstream version 0.54

* Sun Nov 12 2023 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.53-0
- Upstream version 0.53

* Wed Sep 27 2023 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.52-0
- Upstream version 0.52

* Fri Aug 11 2023 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.51-2
- Upstream version 0.51 BUGFIX#1

* Mon Jul 31 2023 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.51-1
- Upstream version 0.51

* Mon Feb 14 2022 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.50-1
- Upstream version 0.50

* Fri Sep 17 2021 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.33-1
- Upstream version 0.33

* Sat May 01 2021 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.32-1
- Upstream version 0.32

* Sat Apr 10 2021 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.31-1
- Upstream version 0.31

* Thu Mar 18 2021 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.30-1
- Upstream version 0.30

* Sat Mar 06 2021 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.29-1
- Upstream version 0.29

* Tue Mar 02 2021 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.28-1
- Upstream version 0.28

* Sat Jan 18 2021 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.27-1
- Upstream version 0.27

* Sun Dec 23 2018 - giuseppe massimo bertani <gmbertani@users.sourceforge.net> 0.24-0
- first RPMs created under OBS. Upstream version 0.24
