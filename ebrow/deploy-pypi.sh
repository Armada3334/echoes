#!/bin/bash
PASSWORD=$1 ;

if [ -z "$PASSWORD" ]; then
    echo "usage: deploy-pypi.sh <password>" ;
    exit -1 ;
fi

# building package
rm -fr dist/*
python3 -m build 

# loading packages on PyPi repo
python3 -m twine upload -u __token__ -p $PASSWORD --verbose --repository pypi dist/*

