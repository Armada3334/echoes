#!/bin/sh
#LINUX only

#echoes-xx.yy.tar.gz must already exist, it won't be created here
echo "current dir = "$PWD;
source ./setver.sh ;
pushd . ;
dist="echoes-"$appVersion ;
buildDate=`date +"%Y/%m/%d_%H:%M:%S"` ;
sedCmd="s/"$oldVersion"/"$appVersion"/g" ;
echo "dist="$dist " build date="$buildDate" sedCmd="$sedCmd ; 
cp debian.rules debian.rules.bak ;
sed -e$sedCmd debian.rules.bak > debian.rules ;  
cp echoes.dsc echoes.dsc.bak ;
sed -e$sedCmd echoes.dsc.bak > echoes.dsc ;  
echo "BUILDING for xUbuntu_17.10 appVersion="$appVersion " buildDate="$buildDate ;
osc build --local-package --define='APP_VERSION $appVersion' --define='BUILD_DATE $buildDate' --trust-all-projects xUbuntu_17.10 x86_64 *.dsc > build.log &
tail -f build.log ;
