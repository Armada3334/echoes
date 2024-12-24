# This is an example PKGBUILD file. Use this as a start to creating your own,
# and remove these comments. For more information, see 'man PKGBUILD'.
# NOTE: Please fill out the license field for your package! If it is unknown,
# then please put 'unknown'.

# Maintainer: giuseppe massimo bertani <gmbertani@users.sourceforge.net>
pkgname=echoes
pkgver=0.59
pkgrel=1
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

#md5sum of echoes-0.59.tar.gz
#to be recalculated with makepkg -g or md5sum
md5sums=(d746054f508417316f008060ce4f1277)


md5sums=(ce2a94f3adb4e5a5d39ce9cf77c849f2)
md5sums=(adafcfb40711e03d70ab7616349c7ebc)
md5sums=(14cfb2af1f5f864e5bbbca165e513f1a)
