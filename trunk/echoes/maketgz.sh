#!/bin/sh
#LINUX only
#creates the echoes.vv.rr.tar.gz file containing pristine sources for RPM building
#$Id: maketgz.sh 178 2018-04-29 16:48:44Z gmbertani $
dist=$1 ;
make clean ;
pushd .
cd .. ;
rm -fr $dist ;
mkdir $dist ;
cp echoes/echoes.pro $dist
cp echoes/*.cpp $dist
cp echoes/*.qrc $dist
cp echoes/*.qss $dist
cp echoes/*.h $dist
cp echoes/*.ui $dist
cp echoes/*.sh $dist 
cp -r echoes/sounds $dist
cp -r echoes/langs $dist
cp -r echoes/icons $dist
cp -r echoes/fonts $dist
cp -r echoes/deps/db/echoes.sqlite3 $dist ;
mkdir $dist/docs ;
mkdir $dist/docs/tests ;

cp echoes/docs/*.pdf $dist/docs
cp echoes/docs/*.txt $dist/docs
cp echoes/docs/tests/*.rts $dist/docs/tests
cp echoes/license.txt $dist
cp echoes/echoes.desktop $dist

cp echoes/echoes.spec $dist 
cp echoes/echoes.PKGBUILD $dist 
cp echoes/COPYING $dist 
cp echoes/echoes.PKGBUILD $dist 

tar cvzf $dist.tar.gz $dist
popd
