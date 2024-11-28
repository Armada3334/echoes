#include "setup.h"
#include "xqdir.h"

XQDir::XQDir(const QDir & d) : QDir(d)
{}

XQDir::XQDir(const QString &path) : QDir(path)
{}

XQDir::XQDir(const QString &path, const QString &nameFilter,
      SortFlags sort, Filters filter) :
    QDir(path, nameFilter, sort, filter)
{}



bool XQDir::pushCd( const QString& dest )
{
    stk.push( absolutePath() );
    qDebug() << "pushCd(" << dest << ") from " << absolutePath();
    bool ok = exists(dest);
    qDebug() << dest << " exists=" << ok;
    if(ok == true)
    {
        cd(dest);
    }
    return ok;
}

bool XQDir::popCd()
{
    QString back = stk.pop();
    qDebug() << "popCd() from " << absolutePath() << " back to " << back;
    bool ok = cd(back);
    qDebug() << "current dir: " << currentPath() ;
    return ok;
}


QStringList XQDir::splitPath(QString path)
{
    path = fromNativeSeparators( path );
    return path.split('/');
}


QString XQDir::splitExt(QString filePath)
{
    QStringList splitten = filePath.split('.');
    return splitten.last();
}


bool XQDir::removeDir(QString dirName)
{
    bool result = true;
    QDir dir(dirName);

    if (dir.exists())
    {
#ifdef WINDOZ
        //workaround for QDir::removeRecursively()
        //bug under Windows
        dirName = dirName.replace("/", "\\");
        QString s = QString("rmdir /Q /S %1").arg(dirName);
        int ret = system(qPrintable(s));
        if(ret != 0)
        {
            qWarning() << "failed removing folder " << dirName << " system() error: " << ret;
        }
#else
        foreach(QFileInfo info,
                dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst))
        {
            if (info.isDir())
            {
                result = removeDir(info.absoluteFilePath());
            }
            else
            {
                result = QFile::remove(info.absoluteFilePath());
                if(result == false)
                {
                    qWarning() << "failed removing file " << info.absoluteFilePath();
                }
            }

            if (!result)
            {
                return result;
            }
        }
        result = QDir().rmdir(dirName);
        if(result == false)
        {
            qWarning() << "failed removing folder " << dirName;
        }

#endif

    }
    return result;
}



bool XQDir::copy(QString& from, QString& to, bool overWrite)
{
    QFile src(from);
    QFileInfo srcInfo(src);
    if(overWrite)
	{
	    //removes the destination file if already existing
		QFile dest(to);
		if( dest.exists() )
		{
			if (dest.remove() == false)
			{
				qWarning() << "FAILED overwriting existing file " << to;
				return false;
			}
		}			
	}
	
    if( !src.exists() )
    {
        qWarning() << "FAILED: source file " << from << " doesn't exist";
        return false;
    }

    if( src.copy(to) == false )
    {
        qWarning() << "FAILED copying " << from << " on " << to;
        qWarning() << src.errorString();
        return false;
    }

    //after copying, applies on dest the same birth time of src
    QFile dest(to);
    //works on Windows only: QDateTime localBirthTime = srcInfo.birthTime().toLocalTime();
    QDateTime localModifyTime = srcInfo.lastModified().toLocalTime();
    if(!dest.open(QFile::ReadWrite))
    {
        qWarning() << "FAILED opening " << to << " preserve creation time";
    }
    else if (!dest.setFileTime(localModifyTime, QFileDevice::FileModificationTime))
    {
       qWarning() << "FAILED copying " << from << " creation time (" << localModifyTime << " on " << to;
    }

    return true;
}

bool XQDir::xcopy(QString from, QString to)
{
    QFileInfo fi(from), fo(to);
    qDebug() << "Copying " << from << " to " << to;
    if (fo.isAbsolute() == true && fo.isDir() == false)
    {
        //"to" is a single file,
        if(fi.isAbsolute() == true && fi.isDir() == false)
        {
            //also "from" is a single file
            return copy(from,to);
        }
        else
        {
            qWarning() << "FAILED - cannot copy the folder " << from << " in a simple file " << to;
            return false;
        }
    }
    else if (fo.isDir() == true)
    {
        //"to" is a directory
        if(fi.isFile() == true)
        {
            //and "from" is a file
            XQDir toDir(to);
            return xcopy(from,toDir.absoluteFilePath(fi.fileName()));
        }
        else
        {
            //both parameters are directories. All the content of
            //"from" is copied recursively to "to"
            XQDir fd(from);
            QStringList fromFiles = fd.entryList(
                        (XQDir::Files|XQDir::Dirs|XQDir::NoDotAndDotDot),
                        XQDir::Name );

            QString fromFile;
            foreach(fromFile, fromFiles)
            {
                xcopy( fd.absoluteFilePath(fromFile), to);
            }
        }
    }
    return true;
}

