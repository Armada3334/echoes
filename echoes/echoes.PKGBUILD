# This is an example PKGBUILD file. Use this as a start to creating your own,
# and remove these comments. For more information, see 'man PKGBUILD'.
# NOTE: Please fill out the license field for your package! If it is unknown,
# then please put 'unknown'.

# Maintainer: giuseppe massimo bertani <gmbertani@users.sourceforge.net>
pkgname=echoes
pkgver=0.61
pkgrel=0
epoch=
pkgdesc="Echoes is a RF spectrograph designed for meteor scatter purposes. It relies on SoapySDR library for audio devices and SDR devices support."
arch=('i686' 'x86_64' 'armv6l' 'armv6h' 'armv7l' 'armv7h')
url="https://sourceforge.net/projects/echoes"
license=('GPL-3.0-or-later')
groups=()
#rtl-sdr upstream https://github.com/steve-m/librtlsdr/releases/tag/0.6.0/librtlsdr-0.6.0.tar.gz
depends=('qt5-charts' 'liquid-dsp>=1.3.2' 'soapysdr' 'libusb' 'qt5-multimedia')
makedepends=()
checkdepends=()
optdepends=('gnuplot: for console mode reporting')
provides=()
conflicts=()
replaces=()
backup=()
options=()
install=
changelog=
source=("$pkgname-$pkgver.tar.gz")

noextract=()
md5sums=()
validpgpkeys=()

build() {
	cd "$pkgname-$pkgver"
	qmake echoes.pro -r -spec linux-g++  AUX_INSTALL_DIR=/usr/share BIN_INSTALL_DIR=/usr/bin APP_VERSION=${APP_VERSION} BUILD_DATE=${BUILD_DATE}
	make
}

package() {
	cd "$pkgname-$pkgver"
	make INSTALL_ROOT="$pkgdir/" install
	ln -sf /usr/bin/echoes "$pkgdir/usr/bin/console_echoes"
}

#md5sum of echoes-0.61.tar.gz
#to be recalculated with makepkg -g or md5sum
md5sums=(4ff3a46b83d6a1bf1c2f775af132eb8f)
md5sums=(7b9af04f4f8fc504fe39819a2b823a4e)
md5sums=(242d0335552b890d8346c6cd8bb39aed)
