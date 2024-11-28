#!/bin/bash
echo Impostazione della proprieta svn:keyword Id su tutti i file
svn propset svn:keywords "Revision Author Date Id" -R *

