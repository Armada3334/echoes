/******************************************************************************

    Echoes is a RF spectrograph for SDR devices designed for meteor scatter
    Copyright (C) 2018-2022  Giuseppe Massimo Bertani gmbertani(a)users.sourceforge.net


    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.
    

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, http://www.gnu.org/copyleft/gpl.html






*******************************************************************************
$Rev:: 23                       $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-01-17 17:28:16 +01#$: Date of last commit
$Id: waterfallwidget.h 23 2018-01-17 16:28:16Z gmbertani $
*******************************************************************************/

#ifndef WATERFALLWIDGET_H
#define WATERFALLWIDGET_H

#include <QtWidgets>
#include <QtCore>

class DbmPalette;
class Settings;

class WaterfallWidget : public QWidget
{
    Q_OBJECT

    QImage* img;                //waterfall full image
    QImage* imgCopy;            //waterfall copy for scroll

    QVector<QRgb> colorTable;
    DbmPalette* dp;
    Settings* as;

    bool    init;

    int     pixXticks;      //number of freq.tickmarks in waterfall

    int     hzMajTicks;     //frequency step for major ticks
    int     hzMinTicks;     //frequency step for minor ticks

    int     pixYticks;      //pixels count between successive time tickmarks
    int     yTicksCount;    //number of time tickmarks that can fit in waterfall

    float   hzPerPixel;

    int     lowest;         //scale lowest value at current WFzoom [Hz]
    int     highest;        //scale highest value at current WFzoom [Hz]
    int     intervalHz;       //center of peak detection interval [Hz]
    int     rangeLo;          //event detection range start [Hz]
    int     rangeHi;          //event detection range end [Hz]


    int     startScan;      //starting of horizontal scan (0 at full res)
    int     endScan;        //end of horizontal scan (img->width at full res)

    QString timeStamp;      //timestamp of the latest scan drawn

    QQueue<QString> tsq;    //queue of timestamps of displayed scans

    QRgb*   scan;           //horizontal scan pointer


    virtual void paintEvent ( QPaintEvent *pe );
    virtual void mousePressEvent(QMouseEvent *event);
    //virtual void resizeEvent ( QResizeEvent* event );

public:

    ///
    /// \brief WaterfallWidget
    /// \param appSettings
    /// \param dbPal
    /// \param parent
    ///
    explicit WaterfallWidget(Settings* appSettings, DbmPalette* dbPal, QWidget *parent);
    ~WaterfallWidget();

    ///
    /// \brief setGrid
    /// \param xTicks
    /// \param yTicks
    /// \param fStep
    /// \param fMinStep
    /// \param hzFrom
    /// \param hzTo
    /// \param sizeChange
    ///
    void setGrid(int xTicks, int yTicks, int fStep, int fMinStep, bool sizeChange = false);


    ///
    /// \brief refresh
    /// \param ts
    /// \return
    ///
    bool refresh(QString& ts , QList<QPointF> &scanPixels);

    ///
    /// \brief takeShot
    /// \param shotFileName
    ///
    void takeShot( QString shotFileName );

    ///
    /// \brief clean
    ///
    void clean();

    ///
    /// \brief scans
    /// \return
    ///
    int scans() const
    {
        return img->height();
    }


signals:

public slots:


};

#endif // WATERFALLWIDGET_H
