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
$Rev:: 351                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-10-22 00:13:47 +02#$: Date of last commit
$Id: ruler.cpp 351 2018-10-21 22:13:47Z gmbertani $
*******************************************************************************/


#include "ruler.h"

Ruler::Ruler(Settings* appSettings, RULER_ORIENT orient, QWidget *parent) :
    QWidget(parent)
{
    setObjectName("Ruler widget");
    MY_ASSERT( nullptr !=  appSettings)

    as = appSettings;
    ro = orient;

    pixMinTicks   = 0;
    pixMajTicks   = 0;
    hzMajTicks  = 0;
    hzMinTicks  = 0;
    hzPerPixel  = 0;
    clickTolerance = 5;
    rangeLo = 0;
    rangeHi = 0;
    halfIntvl = 0;
    isDraggingNeedle = false;
    isDraggingScale = false;
    hasDraggedByButtons = false;
    draggingMarker = DM_NONE;
    draggingNotchIndex = -1;

    rangeXl = 0;
    rangeXr = 0;

    lowBound      = 0;
    highBound     = 0;
    wfCoverage = 0;
    rect = parent->contentsRect();

    //self resizes to match the entire contents area
    //of parent widget
    resize( rect.width(), rect.height() );

    notches = as->getNotches();

    setContextMenuPolicy( Qt::DefaultContextMenu );
    setMouseTracking(true);
    defaultTip = toolTip();
    intervalHz = as->getPeakIntervalFreq();
    intervalHz += as->getEccentricity();
    pup = nullptr;

}

Ruler::~Ruler()
{
    //delete img;

}

void Ruler::slotUpdateNotches(int index)
{
    MY_ASSERT(pup != nullptr)

    Notch nn = pup->getNotch();
    notches.replace(index, nn);
    MYINFO << "updating notch #" << index+1;
    as->setNotches(notches);
    update();
}



void Ruler::contextMenuEvent(QContextMenuEvent *event)
{
    int hz = static_cast<int>(event->x() * hzPerPixel)  +lowBound;
    Notch n;

    int index = -1;

    //Checks if the mouse has been clicked near an already
    //defined notch, otherwise considers it's a new one.


    for(int i = 0; i < notches.count(); i++)
    {
        n = notches.at(i);
        if(hz > (n.begin - clickTolerance) &&
           hz < (n.end + clickTolerance))
        {
            //opens an existing one
            index = i;
            break;
        }
    }

    if( index == -1 )
    {
        //no marker found at this frequency
        //asks to create a brand new one
        n.freq = hz;
        n.width = 0;
        n.begin = hz;
        n.end = hz;

        //keeps the list ordered by  freq
        for(int i = 0; i < notches.count(); i++)
        {
            Notch nn = notches.at(i);
            if(n.freq < nn.freq)
            {
                notches.insert(i, n);
                MYINFO << "inserted new notch as #" << index+1;
                index = i;
                break;
            }
        }

        if(index == -1)
        {
            index = notches.count();
            MYINFO << "appended new notch as #" << index+1;
            notches.append(n);
        }

    }

    if(isDraggingNeedle || draggingNotchIndex != -1 || draggingMarker != DM_NONE)
    {
        isDraggingNeedle = false;
        draggingNotchIndex = -1;
        draggingMarker = DM_NONE;
        MYINFO << "drag end";
    }

    pup = new NotchPopup(n, as, this);
    pup->setWindowFlags(Qt::Tool|Qt::FramelessWindowHint);
    pup->move(event->globalPos().x(), event->globalPos().y() + 30);
    pup->setIndex(index);

    connect( pup, SIGNAL( notchChanged(int) ), this, SLOT( slotUpdateNotches(int) ) );
    int result = pup->exec();

    if( result == QDialog::Accepted )
    {
        n = pup->getNotch();
        if( n.width != 0 )
        {
            slotUpdateNotches(index);
        }
        else
        {
            //removes the element from the list
            MYINFO << "deleting notch #" << pup->getIndex();
            notches.removeAt( pup->getIndex() );
        }
    }
    else
    {
        //aborting notch
        MYINFO << "aborting notch #" << pup->getIndex();
        notches.removeAt( pup->getIndex() );
    }

    as->setNotches(notches);
    update();

    delete pup;
    pup = nullptr;
}


void Ruler::setGrid(int maxTicks, int hzFrom, int hzTo)
{
    MYINFO << __func__ << "(maxTicks=" << maxTicks << ", hzFrom=" << hzFrom << ", hzTo=" << hzTo;

    rect = parentWidget()->contentsRect();
    resize( rect.width(), rect.height() );
    notches = as->getNotches();

    lowBound =  hzFrom;
    highBound = hzTo;

    intervalHz = as->getPeakIntervalFreq();

    wfCoverage = highBound - lowBound;

    int rawHzMajTicks = static_cast<int>( as->pround( wfCoverage / maxTicks) );

    //the major ticks hz must be a value multiple of 10 with base 1, 2 or 5
    //i.e. 0.1 0.2 0.5 1 2 5 10 20 50 100 200 500...
    // so rawHzMajTicks must be rounded with this criteria

    int basis[4]={1,2,5,1};
    int inferior = 0, superior = 0;
    for(int tenPow = -1; tenPow < 7; tenPow++)
    {
        for(int baseIdx=0; baseIdx < 3; baseIdx++)
        {
            inferior = basis[baseIdx] * pow(10, tenPow);
            superior = basis[baseIdx+1] * pow(10, (baseIdx < 2)? tenPow : tenPow+1 );
            if(inferior <= rawHzMajTicks && superior >= rawHzMajTicks)
            {
                //range found, now must choose the closest of the two values
                if((rawHzMajTicks - inferior) < (superior - rawHzMajTicks))
                {
                    hzMajTicks = inferior;
                }
                else
                {
                    hzMajTicks = superior;
                }
            }
        }
    }


    hzMinTicks = hzMajTicks / 10;

    //converting hz to pixels:
    hzPerPixel = static_cast<float>(wfCoverage) / contentsRect().width();  //Hz per pixel
    as->setHpp( hzPerPixel );
    clickTolerance = 5 * static_cast<int>(hzPerPixel);

    pixMinTicks = static_cast<int>( as->pround( hzMinTicks ) / hzPerPixel );     //pixels per minor tick
    pixMajTicks = static_cast<int>( as->pround( hzMajTicks ) / hzPerPixel );     //pixels per major tick

    MYINFO << "pixMinTicks=" << pixMinTicks << " pixMajTicks=" << pixMajTicks
           << " hzPerPixel=" << hzPerPixel << " width=" << width();

    halfIntvl = ((wfCoverage / 2) * as->getDetectionRange()) / 100;

    rangeLo = intervalHz - halfIntvl;
    rangeHi = intervalHz + halfIntvl;

    update();
}

void Ruler::paintEvent( QPaintEvent *pe )
{
    Q_UNUSED(pe) //avoids warning unused parameter

    QPainter p(this);

    int xm = 0, oldXm = 0;
    int hsw = 0;

    p.setFont( parentWidget()->font() );

    QPoint p1,p2,p3;
    QString hzString;
    QFontMetrics fm( p.font() );
    QImage qi;

    QPen penText, penLine;
    penLine.setColor(Qt::gray);
    penText.setColor(Qt::cyan);

    Notch n;

    if(ro == RO_DOWN || ro == RO_UP)
    {        
        //drawing markers
        MYDEBUG << "-->paintEvent lowBound=" << lowBound << " highBound=" << highBound << " intervalHz=" << intervalHz
        << " eccentricity=" << as->getEccentricity() << "hpp=" << hzPerPixel << " wfCoverage=" << wfCoverage;
        for( int hz = lowBound; hz <= highBound; hz++ )
        {
            xm = static_cast<int>( as->pround( (hz - lowBound) / hzPerPixel ) ) + 2;
            p1.setX(xm);
            p2.setX(xm);
            p3.setX(xm);

            if(hz == rangeLo)
            {
                //left peak threshold limit
                qi.load(":rangeLo16");
                rangeXl = xm;
                p.drawImage(xm - qi.width(), 2, qi);
            }

            if(hz == rangeHi)
            {
                //right peak threshold limit
                qi.load(":rangeHi16");
                rangeXr = xm;
                p.drawImage(xm, 2, qi);
            }

            if(hz == intervalHz)
            {
                //peak interval frequency red marker
                qi.load(":index");
                centerX = xm;
                p.drawImage(xm - (qi.width()/2), 2, qi);
                if(isDraggingNeedle)
                {
                    MYINFO << "OK-->intervalHz=" << intervalHz;
                }
            }
        }


        //adding notches
        QPixmap marker(":/notch");
        MY_ASSERT(marker.isNull() == false)
        penLine.setColor(Qt::red);
        penLine.setWidth(3);
        p.setPen( penLine );
        foreach (n, notches)
        {
            xm = static_cast<int>(as->pround( (n.freq - lowBound) / hzPerPixel )) - (marker.width() / 2);
            p1.setX(xm);
            p1.setY(0);

            //center marker
            p.drawPixmap(p1, marker);

            //notch width

            xm = static_cast<int>( as->pround( (n.begin - lowBound) / hzPerPixel ) );
            p2.setX( xm );
            p2.setY( 2 );
            xm = static_cast<int>( as->pround( (n.end - lowBound) / hzPerPixel ) );
            p3.setX( xm );
            p3.setY( 2 );

            p.drawLine(p2,p3);

        }

        //drawing scale
        penLine.setColor(Qt::gray);
        penText.setColor(Qt::cyan);
        penLine.setWidth(1);

        //left side
        for( int hz = intervalHz; hz >= lowBound; hz-- )
        {
            xm = static_cast<int>( as->pround( (hz - lowBound) / hzPerPixel ) ) + 2;
            p1.setX(xm);
            p2.setX(xm);
            p3.setX(xm);


            if( fmod(hz, hzMajTicks) == 0.0 )
            {
                //major ticks

                p1.setY(0);
                p2.setY(rect.height());
                p.setPen(penLine);
                p.drawLine(p1,p2);

                hzString = loc.toString(hz);

                hsw = (fm.boundingRect( hzString ).width() / 2) + 3;

                if( abs(xm - oldXm) > (2 * hsw))
                {
                    //avoids numbers cluttering
                    p3.setX( xm - hsw + 3 );
                    p3.setY(rect.height() - 16);
                    p.setPen(penText);
                    p.drawText( p3, hzString );     //strings are always drawn horizontal
                    oldXm = xm;
                }

            }
            else if( fmod(hz, 5 * hzMinTicks) == 0.0 )
            {
                //half ticks
                p1.setY(rect.height() - 2);
                p2.setY(rect.height() - 15);
                p.setPen(penLine);
                p.drawLine(p1,p2);
            }
            else if( fmod(hz, hzMinTicks) == 0.0 )
            {
                //minor ticks
                p1.setY(rect.height() - 2);
                p2.setY(rect.height() - 8);
                p.setPen(penLine);
                p.drawLine(p1,p2);
            }

        }

        //right side
        for( int hz = intervalHz; hz <= highBound; hz++ )
        {
            xm = static_cast<int>( as->pround( (hz - lowBound) / hzPerPixel ) ) + 2;
            p1.setX(xm);
            p2.setX(xm);
            p3.setX(xm);


            if( fmod(hz, hzMajTicks) == 0.0 )
            {
                //major ticks

                p1.setY(0);
                p2.setY(rect.height());
                p.setPen(penLine);
                p.drawLine(p1,p2);

                hzString = loc.toString(hz);

                hsw = (fm.boundingRect( hzString ).width() / 2) + 3;

                if(abs(xm - oldXm) > (2 * hsw))
                {
                    //avoids numbers cluttering
                    p3.setX( xm - hsw + 3 );
                    p3.setY(rect.height() - 16);
                    p.setPen(penText);
                    p.drawText( p3, hzString );     //strings are always drawn horizontal
                    oldXm = xm;
                }

            }
            else if( fmod(hz, 5 * hzMinTicks) == 0.0 )
            {
                //half ticks
                p1.setY(rect.height() - 2);
                p2.setY(rect.height() - 15);
                p.setPen(penLine);
                p.drawLine(p1,p2);
            }
            else if( fmod(hz, hzMinTicks) == 0.0 )
            {
                //minor ticks
                p1.setY(rect.height() - 2);
                p2.setY(rect.height() - 8);
                p.setPen(penLine);
                p.drawLine(p1,p2);
            }

        }


    }

    if(ro == RO_LEFT || ro == RO_RIGHT)
    {
        //todo
    }

}

void Ruler::mousePressEvent(QMouseEvent *event)
{
    int pixelTolerance = PIXEL_TOLERANCE;
    int x1 = event->x() - pixelTolerance;
    int x2 = event->x() + pixelTolerance;

    if( draggingNotchIndex == -1 &&
        draggingMarker == DM_NONE &&
        isDraggingNeedle == false &&
        isDraggingScale == false)
    {

        if( centerX >= x1 &&
            centerX <= x2)
        {
            isDraggingNeedle = true;
            MYINFO << "start dragging center needle";
            setFocus();
            return;
        }
        else if( rangeXl >= x1 &&
                 rangeXl <= x2 )
        {
            draggingMarker = DM_LEFT;
            MYINFO << "start dragging left marker";
            setFocus();
            return;
        }
        else if( rangeXr >= x1 &&
                 rangeXr <= x2 )
        {
            draggingMarker = DM_RIGHT;
            MYINFO << "start dragging right marker";
            setFocus();
            return;
        }
        else
        {
            int hz1 = static_cast<int>((x1 * hzPerPixel) + lowBound);
            int hz2 = static_cast<int>((x2 * hzPerPixel) + lowBound);
            Notch n;
            for (int idx = 0; idx < notches.count(); idx++)
            {
                n = notches.at(idx);
                if( n.freq >= hz1 && n.freq <= hz2 )
                {
                    draggingNotchIndex = idx;
                    MYINFO << "start dragging notch#" << idx+1 << " freq=" << n.freq;
                    setFocus();
                    return;
                }
            }

            //TODO: dragging scale
        }
    }
    else if(isDraggingNeedle || draggingNotchIndex != -1 || draggingMarker != DM_NONE)
    {
        isDraggingNeedle = false;
        draggingNotchIndex = -1;
        draggingMarker = DM_NONE;
        hasDraggedByButtons = false;
        MYINFO << "drag end";
    }
}

void Ruler::mouseMoveEvent(QMouseEvent *event)
{
    if(hasDraggedByButtons)
    {
        isDraggingNeedle = false;
        draggingNotchIndex = -1;
        draggingMarker = DM_NONE;
        hasDraggedByButtons = false;
        MYINFO << "drag end";
        return;
    }

    int hz = static_cast<int>((event->x() * hzPerPixel)) + lowBound;
    if(isDraggingNeedle)
    {
        int centerHz = (wfCoverage/2) + lowBound;
        int ecc = hz - centerHz;

        if( ((hz - lowBound) > halfIntvl) &&
            ((highBound - hz) > halfIntvl ))
        {
            as->setEccentricity( ecc );
            MYINFO << "Dragging eccentricity = " << ecc;
        }
    }
    else if(draggingMarker == DM_LEFT || draggingMarker == DM_RIGHT)
    {
        float diffHz = fabs(intervalHz - hz);
        int newTrange = (diffHz / wfCoverage) * 100 * 2;
        halfIntvl = static_cast<int>(diffHz);
        if( hz > lowBound &&
            hz < highBound )
        {
            as->setDetectionRange( newTrange );
            MYINFO << "Dragging markers to " << newTrange << "%";
        }
    }
    else if(draggingNotchIndex != -1)
    {
        Notch n = notches.at(draggingNotchIndex);
        if( hz > lowBound &&
            hz < highBound )
        {
            //int diff = (n.freq - hz);
            n.freq = hz;
            n.begin = hz - (n.width / 2);
            n.end   = hz + (n.width / 2);
            notches.replace(draggingNotchIndex, n);
            as->setNotches(notches);
            MYINFO << "Dragging notch " << draggingNotchIndex+1;
        }
    }
    else
    {
        //if the mouse is hovering a notch marker
        //a tooltip with its parameters is showed
        Notch n;
        QString newTip;

        for(int i = 0; i < notches.count(); i++)
        {
            n = notches.at(i);
            if(hz > (n.begin - clickTolerance) &&
               hz < (n.end + clickTolerance))
            {
                newTip = QString(tr("Notch#%1\nat %2 Hz\n%3 Hz wide"))
                        .arg(i+1).arg(n.freq).arg(n.width);
                setToolTip(newTip);
                if(!as->getTooltips())
                {
                    //display tooltip
                    QToolTip::showText( mapToGlobal( event->pos() ), newTip);
                }
                return;

            }
        }


        if(hz > (rangeLo - 2*clickTolerance) &&
           hz < rangeLo)
        {
            newTip = tr("Start of event detection range");
            setToolTip(newTip);
            if(!as->getTooltips())
            {
                //display tooltip
                QToolTip::showText( mapToGlobal( event->pos() ), newTip);
            }
            return;
        }

        if(hz > rangeHi &&
           hz < (rangeHi + 2*clickTolerance))
        {
            newTip = tr("End of event detection range");
            setToolTip(newTip);
            if(!as->getTooltips())
            {
                //display tooltip
                QToolTip::showText( mapToGlobal( event->pos() ), newTip);
            }
            return;
        }

        setToolTip(defaultTip);
    }

    update();
}

void Ruler::wheelEvent(QWheelEvent *event)
{
    if( event->angleDelta().isNull() )
    {
        return;
    }

    //Most mouse types work in steps of 15 degrees,
    //in which case the delta value is a multiple of 120;
    //i.e., 120 units * 1/8 = 15 degrees
    QPoint p = event->angleDelta();

    //consider only vertical wheels
    int numDegrees = p.y() / 8;

    int numSteps = numDegrees / 15;

    int hz = static_cast<int>((event->MY_POSF.x() * hzPerPixel));
    hz += lowBound;
    Notch n;
    for(int i = 0; i < notches.count(); i++)
    {
        n = notches.at(i);
        if(hz > (n.begin - clickTolerance) &&
           hz < (n.end + clickTolerance))
        {         
            if(numSteps != 0)
            {
                //every mouse step is +/-1% of the waterfall coverage
                int hzPerStep = wfCoverage / 100;
                int hzDelta = numSteps * hzPerStep;
                int newWidth = n.width + hzDelta;
                if(newWidth >= hzPerStep)
                {
                    //resizes the notch
                    n.width = newWidth;
                    n.begin = n.freq - (n.width / 2);
                    n.end = n.freq + (n.width / 2);
                    notches.replace(i, n);
                    as->setNotches(notches);
                    MYINFO << "Resized notch#" << i+1 << " to " << newWidth << " Hz";
                }
                else
                {
                    //deletes the notch
                    notches.removeAt(i);
                    as->setNotches(notches);
                    MYINFO << "Deleted notch#" << i+1;
                }
                update();
            }

            event->accept();
            return;
        }

    }

    int pixelTolerance = PIXEL_TOLERANCE;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    int x1 = event->position().x() - pixelTolerance;
    int x2 = event->position().x() + pixelTolerance;
#else
    int x1 = event->x() - pixelTolerance;
    int x2 = event->x() + pixelTolerance;
#endif
    if( centerX >= x1 &&
        centerX <= x2 )
    {
        //every mouse step is +/-1% of the waterfall coverage
        int hzPerStep = wfCoverage / 100;
        float hzDelta = numSteps * hzPerStep;
        int newTrange = ((hzDelta / wfCoverage) * 100 * 2) + as->getDetectionRange();
        if(newTrange < 2)
        {
            //minimum value alloweed: 2%
            newTrange = 2;
        }
        if(newTrange > 100)
        {
            //maximum value alloweed: 100%
            newTrange = 100;
        }
        halfIntvl = static_cast<int>(hzDelta);
        if( hz > lowBound &&
            hz < highBound )
        {
            as->setDetectionRange( newTrange );
            MYINFO << "Dragging markers to " << newTrange << "%";
        }
        event->accept();
        return;
    }



    event->ignore();
}

void Ruler::keyPressEvent( QKeyEvent* e )
{
    if(isDraggingNeedle)
    {
        int ecc = as->getEccentricity();
        switch(e->key())
        {
        case Qt::Key_Plus:
            hasDraggedByButtons = true;
            MYINFO << "++fine adjusting eccentricity: " << ecc;
            ecc++;
            as->setEccentricity( ecc );
            MYINFO << " to " << ecc;
            e->accept();
            break;

        case Qt::Key_Minus:
            hasDraggedByButtons = true;
            MYINFO << "--fine adjusting eccentricity: " << ecc;
            ecc--;
            as->setEccentricity( ecc );
            MYINFO << " to " << ecc;
            e->accept();
            break;

        default:
            e->ignore();
            break;
        }
    }
}

/*
void Ruler::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    if(isDraggingNeedle || draggingNotchIndex != -1 || draggingMarker != DM_NONE)
    {
        isDraggingNeedle = false;
        draggingNotchIndex = -1;
        draggingMarker = DM_NONE;
        MYINFO << "drag end";
    }
}
*/
