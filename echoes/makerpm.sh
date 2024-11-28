#!/bin/sh
#LINUX only
#creates the echoes.vv.rr.tar.gz file containing pristine sources for RPM building
#$Id: makerpm.sh 178 2018-04-29 16:48:44Z gmbertani $

source ./setver.sh ;
pushd . ;
dist="echoes-"$appVersion ;
#buildDate=`date +"%Y/%m/%d_%H:%M:%S"` ;
sedCmd="s/"$oldVersion"/"$appVersion"/g" ;
echo "dist="$dist " sedCmd="$sedCmd ; 

cp echoes.spec echoes.spec.bak ;
sed -e$sedCmd echoes.spec.bak > echoes.spec ;  

source ./maketgz.sh $dist;
echo "current dir = "$PWD;
echo "setting %_topdir to "$PWD"/RPMBUILD in ~/.rpmmacros eventually replacing the existing file";
rm ~/.rpmmacros ;
echo "%_topdir "$PWD"/RPMBUILD" > ~/.rpmmacros ;
cp ../echoes*.*.tar.gz ./RPMBUILD/SOURCES ;
cp echoes.spec ./RPMBUILD/SPECS ;
#echo "appVersion="$appVersion " buildDate="$buildDate ; 
#rpmbuild -v -ba  --define="APP_VERSION $appVersion" --define="BUILD_DATE $buildDate" --buildroot $PWD/RPMBUILD/BUILDROOT  $PWD/RPMBUILD/SPECS/echoes.spec ;
rpmbuild -v -ba --buildroot $PWD/RPMBUILD/BUILDROOT  $PWD/RPMBUILD/SPECS/echoes.spec ;
