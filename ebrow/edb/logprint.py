# This Python file uses the following encoding: utf-8
from __future__ import print_function
import builtins as __builtin__
from datetime import datetime
from pathlib import Path
import os

verbose = False
workingDir = Path(Path.home(), Path("ebrow"))
logfile = workingDir / Path("ebrow.log")

def logInit(isVerbose):
    global verbose, logfile
    nowIso = datetime.now().isoformat().replace(":", "-")
    if isVerbose:
        fileName = f"ebrow-{nowIso}.log"
        verbose = True
    else:
        fileName = "ebrow.log"
    logfile = workingDir / Path(fileName)


def removeLogFile():
    global logfile
    if os.path.exists(logfile):
        os.remove(logfile)


def print(*args, **kwargs):
    """
    allows to inhibit console prints() when
    --verbose switch is not specified
    :param args:
    :param kwargs:
    :return:
    """
    global verbose, logfile
    if verbose:
        utc = datetime.utcnow()
        __builtin__.print(utc, end=' - ')
        __builtin__.print(*args, **kwargs)

    with open(logfile, 'a') as f:
        fprint(*args, **kwargs, file=f)


def fprint(*args, **kwargs):
    """
    allows prints on files, bypassing
    the --verbose switch
    
    :param args: 
    :param kwargs: 
    :return: 
    """
    """
    :param args: 
    :param kwargs: 
    :return: 
    """
    __builtin__.print(*args, **kwargs)
