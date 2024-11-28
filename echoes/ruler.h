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
$Rev:: 101                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-02-16 23:58:03 +01#$: Date of last commit
$Id: ruler.h 101 2018-02-16 22:58:03Z gmbertani $
*******************************************************************************/


#ifndef RULER_H
#define RULER_H

#include <math.h>
#include <QtWidgets>
#include <QtGui>
#include <QtCore>


#include "settings.h"
#include "notchpopup.h"

#define PIXEL_TOLERANCE 5

///tickmarks orientation in degrees
enum RULER_ORIENT
{
    RO_DOWN  = 0,        ///downward  (horizontal ruler)
    RO_LEFT  = 90,       ///leftward  (vertical ruler)
    RO_UP    = 180,      ///upward    (horizontal ruler)
    RO_RIGHT = 270       ///rightward (vertical ruler)

};

///dragging operations on range limit markers
enum DRAGGING_MARKER
{
    DM_NONE,
    DM_LEFT,
    DM_RIGHT
};

class Ruler : public QWidget
{
    Q_OBJECT

protected:

    QImage*         img;            ///ruler image
    QRect           rect;           ///container's rect

    Settings*       as;             ///current application settings
    QList<Notch>    notches;        ///notch filters

    RULER_ORIENT    ro;             ///ruler orintation

    QPoint          origin;         ///drawing origin
    QLocale         loc;

    int             lowBound;         ///scale begin value [Hz]
    int             highBound;        ///scale end value [Hz]
    int             wfCoverage;       ///scale coverage [Hz]
    int             rangeLo;          ///event detection range start [Hz]
    int             rangeHi;          ///event detection range end [Hz]
    int             halfIntvl;        ///half of detection interval width in hz
    int             clickTolerance;     ///hz tolerance while clicking and drag something on the ruler
    int             intervalHz;       ///center of peak detection interval [Hz]
    int             centerX;        ///X coordinate of the draggable central needle
    int             rangeXl;        ///X coordinate of the draggable left interval marker
    int             rangeXr;        ///X coordinate of the draggable right interval marker

    int             pixMinTicks;    ///pixels for minor ticks
    int             pixMajTicks;    ///pixels for major ticks (1:10)
    int             hzMajTicks;     ///Hz every major tick
    int             hzMinTicks;     ///Hz every minor tick
    float           hzPerPixel;     ///Hz per pixel
    QString         defaultTip;     ///tooltip displayed when no notch is hovered

    NotchPopup*     pup;

    bool isDraggingScale;      ///status dragging the frequency scale
    bool isDraggingNeedle;     ///status dragging the central needle
    bool hasDraggedByButtons;  ///the dragging has been performed via +/- buttons

    DRAGGING_MARKER draggingMarker;             ///range marker under dragging

    int    draggingNotchIndex;      ///index of the notch under dragging, -1=NONE

    virtual void    paintEvent( QPaintEvent *pe ) override;
    virtual void    mouseMoveEvent(QMouseEvent *event) override;
    virtual void    mousePressEvent(QMouseEvent *event) override;
    //virtual void    mouseReleaseEvent(QMouseEvent *event);
    virtual void    contextMenuEvent(QContextMenuEvent *event) override;
    virtual void    wheelEvent(QWheelEvent *event) override;
    virtual void    keyPressEvent( QKeyEvent* e ) override;

public:
    explicit Ruler(Settings* appSettings, RULER_ORIENT orient = RO_DOWN, QWidget *parent = nullptr);
    ~Ruler() override;

     void setGrid(int maxTicks, int hzFrom, int hzTo);

     int getMajHz() const
     {
        //audio spectra binning x2 compensation
        return ( as->isAudioDevice() ) ? hzMajTicks * 2 : hzMajTicks;
     }

     int getMajPix() const
     {
        return pixMajTicks;
     }

     int getMinHz() const
     {
        //audio spectra binning x2 compensation
        return ( as->isAudioDevice() ) ? hzMinTicks * 2 : hzMinTicks;
     }

     int getMinPix() const
     {
        return pixMinTicks;
     }

signals:

protected slots:
    void slotUpdateNotches(int index);
};

#endif // RULER_H
