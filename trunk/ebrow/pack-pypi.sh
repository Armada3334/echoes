#!/bin/bash

PASSWORD=$1 ;

if [ -z "$PASSWORD" ]; then
    echo "usage: pack-pypi.sh <password>" ;
    exit -1 ;
fi

echo "generating UI and resources files"
cd edb;
pyuic5 mainwindow.ui -o ui_mainwindow.py --import-from=.;
pyuic5 dateintervaldialog.ui -o ui_dateintervaldialog.py --import-from=.
pyrcc5 ebrow.qrc -o ebrow_rc.py;
cd .. ;

# the package version is stored in pyproject.toml
# it must be copied to mainwindow.py:APPVERSION 
NEWVERSION=$(cat pyproject.toml | grep -a -m1 -e "version =" | cut -d' ' -f3 ) ;
OLDVERSION=$(cat edb/mainwindow.py | grep -a -m1 -e "APPVERSION =" | cut -d' ' -f3 ) ;
echo "Replacing "$OLDVERSION" with "$NEWVERSION" in edb/mainwindow.py";
cmd="s/"$OLDVERSION"/"$NEWVERSION"/";
echo $cmd;
sed -i -e $cmd edb/mainwindow.py ;

# exit 0;


rm -fr packaging ;

echo "creating packaging directories tree" ;
mkdir -pv packaging/ebrow/src/ebrow/edb/resources/icons ;
mkdir -pv packaging/ebrow/src/ebrow/edb/resources/colormaps ;
mkdir -pv packaging/ebrow/src/ebrow/edb/resources/fonts ;

echo "copying files" ;
cp -fv deploy-pypi.sh packaging/ebrow ;
cp -frv edb packaging/ebrow/src/ebrow ;

cp -fv pyproject.toml packaging/ebrow ;
cp -fv README.md packaging/ebrow ;
cp -fv LICENSE.txt packaging/ebrow ;
cp -fv setup.py packaging/ebrow ;

cp -fv README.md packaging/ebrow/src/ebrow ;

cp -frv edb/assets/* packaging/ebrow/src/ebrow/edb/assets ;
cp -fv edb/resources/icons/LICENSES packaging/ebrow/src/ebrow/edb/resources/icons ;
cp -frv edb/resources/icons/*.ico packaging/ebrow/src/ebrow/edb/resources/icons ;
cp -frv edb/resources/icons/*.png packaging/ebrow/src/ebrow/edb/resources/icons ;
cp -frv edb/resources/icons/*.xpm packaging/ebrow/src/ebrow/edb/resources/icons ;
cp -frv edb/resources/colormaps/*.png packaging/ebrow/src/ebrow/edb/resources/colormaps ;

cp -frv edb/resources/fonts/*.ttf packaging/ebrow/src/ebrow/edb/resources/fonts ;
cp -frv edb/resources/fonts/*.otf packaging/ebrow/src/ebrow/edb/resources/fonts ;
cp -frv edb/resources/fonts/*.woff packaging/ebrow/src/ebrow/edb/resources/fonts ;
cp -frv edb/resources/fonts/LICENCE.txt packaging/ebrow/src/ebrow/edb/resources/fonts ;
cd packaging/ebrow ;

echo "building and deploying package" ;
./deploy-pypi.sh $PASSWORD;
cd ../.. ;

echo "done";




