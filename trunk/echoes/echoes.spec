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
Version:        0.58
Release:        10
Summary:        RF spectrograph for meteor scatter
License:        GPL-3.0-or-later
Group:          Productivity/Scientific/Astronomy
Url:            https://sourceforge.net/projects/echoes
Source:         echoes-0.58.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-build


BuildRequires:  pulseaudio
BuildRequires:  pulseaudio-utils
BuildRequires:  libliquid-devel >= 1.3.2
BuildRequires:  hicolor-icon-theme
BuildRequires:  update-desktop-files
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
qmake-qt5 echoes.pro -r -spec linux-g++  AUX_INSTALL_DIR=/usr/share BIN_INSTALL_DIR=/usr/bin APP_VERSION=0.58 BUILD_DATE=`date --iso-8601`

%build
%make_build

%install
%make_install INSTALL_ROOT=%{buildroot}
ln -sf /usr/bin/echoes %{buildroot}/usr/bin/console_echoes
%suse_update_desktop_file -c -r echoes Echoes "Meteor scatter spectrograph application" echoes echoes_xpm32 "Science;Astronomy"


%files 
%defattr(-,root,root,-)
%dir %{_datadir}/echoes
%dir %{_datadir}/fonts/truetype
/usr/bin/echoes
/usr/bin/console_echoes
/usr/share/echoes/license.txt
/usr/share/echoes/English.qm
/usr/share/echoes/Italian.qm
/usr/share/echoes/TEST_PERIODIC.rts
/usr/share/echoes/README.txt
/usr/share/echoes/TEST_CONTINUOUS.rts
/usr/share/echoes/TEST_AUTOMATIC.rts
/usr/share/echoes/TEST_CLIENT_AUTOMATIC.rts
/usr/share/echoes/TEST_AUTOMATIC_RSP1.rts
/usr/share/echoes/TEST_AUTOMATIC_AIRSPY.rts
/usr/share/echoes/echoes.sqlite3
# /usr/share/echoes/echoes_logo.png
/usr/share/applications/echoes.desktop
/usr/share/fonts/truetype/Gauge-Oblique.ttf
/usr/share/fonts/truetype/Gauge-Regular.ttf
/usr/share/fonts/truetype/Gauge-Heavy.ttf
/usr/share/icons/hicolor/32x32/apps/echoes_xpm32.xpm
/usr/share/sounds/ping.wav


