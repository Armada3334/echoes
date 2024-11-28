# This Python file uses the following encoding: utf-8
from __future__ import print_function
import builtins as __builtin__
from datetime import datetime
from pathlib import Path
import os

verbose = False
workingDir = Path(Path.home(), Path("ebrow"))
logfile = workingDir / Path("ebrow.log")


def removeLogFile():
    if os.path.exists(logfile):
        os.remove(logfile)


def logPrintVerbose(wantVerbose: bool = False):
    global verbose
    verbose = wantVerbose


def print(*args, **kwargs):
    """
    allows to inhibit console prints() when
    --verbose switch is not specified
    :param args:
    :param kwargs:
    :return:
    """
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
