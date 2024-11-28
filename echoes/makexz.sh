#!/bin/sh
#LINUX only
#creates the echoes.vv.rr.tar.gz file containing pristine sources for RPM building
#$Id: makexz.sh 172 2018-04-28 15:38:15Z gmbertani $

source ./setver.sh ;
pushd . ;
dist="echoes-"$appVersion ;
buildDate=`date +"%Y/%m/%d_%H:%M:%S"` ;
sedCmd="s/"$oldVersion"/"$appVersion"/g"
export APP_VERSION=$appVersion ;
export BUILD_DATE=$buildDate ;
echo "dist="$dist " build date="$BUILD_DATE" sedCmd="$sedCmd ; 

cp echoes.PKGBUILD echoes.PKGBUILD.bak ;
sed -e$sedCmd echoes.PKGBUILD.bak > echoes.PKGBUILD ;  

source ./maketgz.sh $dist;
cp ../echoes*.tar.gz ./PKGBUILD/echoes ;
cp echoes.PKGBUILD ./PKGBUILD/echoes ;
cd PKGBUILD/echoes ;
rm -f PKGBUILD ;
mv echoes.PKGBUILD PKGBUILD ;
MAKEPKG=/usr/bin/makepkg ;
PACKAGE=$dist".tar.gz";
if [[ -f "$FILE" ]]; then
    $MAKEPKG -g >> PKGBUILD ; 
    $MAKEPKG ;
else
    echo $MAKEPKG" not found, updating the md5 sum of "$PACKAGE;
    MD5=$(md5sum $PACKAGE | cut -d' ' -f1)
    echo "md5sums=("$MD5")" > $PACKAGE".md5" ;
    cp PKGBUILD PKGBUILD.bak
    cat PKGBUILD.bak $PACKAGE".md5" > PKGBUILD ;
    cp PKGBUILD ../../echoes.PKGBUILD	
fi
echo "DONE";



