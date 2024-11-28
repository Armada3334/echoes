#ifndef XQDIR_H
#define XQDIR_H

#include <QtCore>
#include <qdir.h>

class XQDir : public QDir
{
    QStack<QString> stk;

public:
    //XQDir();
    XQDir(const QDir &d);
    XQDir(const QString &path = QString());
    XQDir(const QString &path, const QString &nameFilter,
          SortFlags sort = SortFlags(Name | IgnoreCase), Filters filter = AllEntries);


    ///
    /// \brief pushCd
    /// \param dest
    /// \return
    ///
    bool pushCd( const QString& dest );

    ///
    /// \brief popCd
    /// \return
    ///
    bool popCd();
#if 0
    ///
    /// \brief join
    /// \param elements
    /// \param toAbsolute
    /// \return
    ///
    /// joins the elements to form a path, relative or absolute
    QString join(QStringList elements, bool makeAbsolute = true)
    {
        //tbd
    }
#endif

    ///
    /// \brief splitPath
    /// \param path
    /// \return
    /// subdivide a path in its elements
    QStringList splitPath(QString path);

    ///
    /// \brief splitExt
    /// \param path
    /// \return the estension of a file
    ///
    QString splitExt(QString filePath);

    ///
    /// \brief removeDir
    /// \param dirName
    /// \return
    /// recursive directories and file removal
    /// works like QDir::removeRecursively()
    /// but allows a better control of operations
    bool removeDir(QString dirName);

    ///
    /// \brief copy
    /// \param from
    /// \param to
    /// \param overWrite
    /// \return
    /// execute a nonstatic copy, in order to get
    /// the appropriate errorString in case of error
    bool copy(QString& from, QString& to, bool overWrite = true);

    ///
    /// \brief xcopy
    /// \param from
    /// \param to
    /// \return
    /// recursive copy of entire directories
    bool xcopy(QString from, QString to);



};

#endif // XQDIR_H
