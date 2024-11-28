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
$Id: waterfallwidget.cpp 351 2018-10-21 22:13:47Z gmbertani $
*******************************************************************************/


#include "settings.h"
#include "dbmpalette.h"
#include "waterfallwidget.h"

WaterfallWidget::WaterfallWidget(Settings* appSettings, DbmPalette* dbPal, QWidget *parent) :
    QWidget(parent)
{
    setObjectName("WaterfallWidget widget");
    MY_ASSERT( nullptr !=  appSettings)
    MY_ASSERT( nullptr !=  dbPal)

    as = appSettings;
    dp = dbPal;

    //self resizes to match the entire contents area
    //of parent widget

    int newWidth = parent->contentsRect().width();
    int newHeight = parent->contentsRect().height();
    resize( newWidth, newHeight );

    //the waterfall is drawn into a QImage
    //initially, its size is equal to widget's size (viewport)
    img = new QImage( size(), IMAGE_FORMAT );
    MY_ASSERT( nullptr !=  img)

    colorTable = dp->getColorTable();
    img->setColorCount( colorTable.count() );
    img->setColorTable( colorTable );
    img->fill(Qt::black);

    imgCopy = new QImage( size(), IMAGE_FORMAT );
    MY_ASSERT( nullptr !=  imgCopy)
    imgCopy->fill(Qt::black);

    pixXticks = 0;
    hzMajTicks = 0;
    pixYticks = 0;
    yTicksCount = 0;

    hzPerPixel = 0;

    lowest = 0;
    highest = 0;
    intervalHz = 0;
    rangeHi = 0;
    rangeLo = 0;

    startScan = 0;
    endScan = img->width()-1;

    init = true;
}


WaterfallWidget::~WaterfallWidget()
{
    delete img;
    delete imgCopy;
}

void WaterfallWidget::mousePressEvent(QMouseEvent *event)
{
    int hz = static_cast<int>((event->x() * hzPerPixel)) + lowest;
    QString ts;
    if(tsq.size() < height())
    {
        //the queue must be full before showing a correct timestamp
        ts = tr("measuring...");
    }
    else
    {
        ts = tsq.at( height() - event->y() );
    }
    MYINFO << "event (" << event->x() << ", " << event->y() << ") hz=" << hz << " timestamp=" << ts;
    QString newTip = QString("%1 Hz\n%2 s").arg(hz).arg(ts);
    setToolTip(newTip);
    if(!as->getTooltips())
    {
        //display tooltip
        QToolTip::showText( mapToGlobal( event->pos() ), newTip);
    }

}

void WaterfallWidget::clean()
{
    img->fill(Qt::black);
}


void WaterfallWidget::setGrid(int xTicks, int yTicks, int fStep, int fMinStep,  bool sizeChange)
{
    Q_UNUSED(sizeChange)

    //the horizontal grid needs to be redrawn only when
    //offset, zoom or tune changes.

    MY_ASSERT(yTicks >= MIN_SCROLLING_RULER_TICKS)

    if( parentWidget()->isVisible() == true &&
        (init == true || sizeChange == true ) )
    {

        //resizes widget according to parent size
        int w = parentWidget()->contentsRect().width();
        int h = parentWidget()->contentsRect().height();
        MYDEBUG << "Parent widget resizing to: " << w << " x " << h;
        resize( w, h );

        //resizing the images too
        delete img;
        img = new QImage( contentsRect().size(), IMAGE_FORMAT );
        MY_ASSERT( nullptr !=  img)

        colorTable = dp->getColorTable();
        img->setColorCount( colorTable.count() );
        img->setColorTable( colorTable );
        img->fill(Qt::black);

        delete imgCopy;
        imgCopy = new QImage( contentsRect().size(), IMAGE_FORMAT );
        MY_ASSERT( nullptr !=  imgCopy)
        imgCopy->fill(Qt::black);

        MYDEBUG << "WaterfallWidget image size=" << size();
        init = false;
    }

    //range boundaries
    intervalHz = as->getPeakIntervalFreq();
    as->getZoomedRange(lowest, highest);
    int halfIntvl = (((highest - lowest) / 2) * as->getDetectionRange()) / 100;
    rangeLo = intervalHz - halfIntvl;
    rangeHi = intervalHz + halfIntvl;

    //updates vertical time grid parameters

    pixYticks = yTicks;
    yTicksCount = 0;

    //------------horizontal tick marks (frequency)

    pixXticks = xTicks;
    hzMajTicks = fStep;
    hzMinTicks = fMinStep;
//setgrid ruler
    hzPerPixel = as->getHpp();



    //first non-zoomed pixel in scan:
    startScan = (img->width() / 2) - (img->width() / 2);

    //last non-zoomed pixel in scan:
    endScan = img->width() - startScan;

    MYINFO << "lowest (zoomLo)=" << lowest << " highest (zoomHi)=" << highest
           << " startScan=" << startScan << " endScan=" << endScan;

    MYINFO << "Hz per pixel (hpp)=" << hzPerPixel;
    update();
}


bool WaterfallWidget::refresh( QString& ts,  QList<QPointF>& scanPixels )
{
    QPainter p;
    bool sync = false;
    int dBfsAbs = 0;
    QRgb argb;

    timeStamp = ts;
    tsq.push_back(timeStamp);
    if(tsq.size() > height())
    {
        tsq.pop_front();
    }


    scan = reinterpret_cast<QRgb*>( img->scanLine(0) );
    foreach(QPointF pixel, scanPixels)
    {
        dBfsAbs = static_cast<int>(dp->getColorDbm( static_cast<int>(pixel.y())));
        if( dBfsAbs >= colorTable.count() )
        {
            dBfsAbs =  colorTable.count()-1 ;
        }

        argb = colorTable.at(dBfsAbs);
        scan[ static_cast<int>(pixel.x()) ] = argb;
    }


    yTicksCount++;
    if( yTicksCount > pixYticks )
    {
        sync = true;

        yTicksCount = 0;

        QPen pen;
        QPoint p1,p2;
        float xm = 0.0;

        p.begin(img);
        pen.setColor( dp->neutralColor() );
        //pen.setColor(Qt::white);
        p.setPen(pen);
        p.drawText( 0, 10, timeStamp );

        if(as->getSec() != 0)
        {
            //draws the scrolling horizontal time grid
            pen.setStyle(Qt::DashLine);
            p.setPen(pen);
            p.drawLine( 0, 0, img->width(), 0 );
        }

        if(as->getHz() != 0)
        {
            //pen.setColor(Qt::black);
            pen.setColor( dp->neutralColor() );
            pen.setStyle(Qt::SolidLine);
            p.setPen(pen);

            for( int hz = lowest; hz < highest; hz++ )
            {
                if( (hz % hzMajTicks) == 0 )
                {
                    //major ticks
                    xm = as->pround( (hz - lowest) / hzPerPixel );
                    p1.setX( static_cast<int>(xm) );
                    p2.setX( static_cast<int>(xm) );
                    p1.setY(0);
                    p2.setY(5);
                    p.drawLine(p1,p2);
                }
            }
        }
        p.end();
    }

    update();

    //scrolling

    QRect src(0,0, img->width(), img->height()-1);
    MYDEBUG << "IMG->WIDTH:" << img->width();
    p.begin(img);
    *imgCopy = img->copy(src);
    p.drawImage( 0, 1, *imgCopy );
    p.end();

    //clearing scan line buffer to accept next scan data
    scan = reinterpret_cast<QRgb*>( img->scanLine(0) );
    argb = colorTable.at(0);
    for(int x = 0; x < img->width(); x++)
    {
        scan[x] = argb;
    }

    colorTable = dp->getColorTable();
    return sync;
}

void WaterfallWidget::paintEvent( QPaintEvent *pe )
{
    Q_UNUSED(pe)       //avoids warning unused-parameter

    //for now, the waterfall orientation is fixed, scrolling downward
    QPainter p(this);
    //update waterfall
    //target,image,source,flags Qt::ColorOnly
    p.drawImage( 1, 1, *img );


    //peak detection interval boundaries

    if(as->getBoundaries() != 0)
    {
        QPen pen;
        pen.setStyle(Qt::DashLine);
        pen.setColor(dp->neutralColor());
        pen.setWidth(1);
        p.setPen(pen);

        int pixLo = (rangeLo - lowest) / as->getHpp();
        p.drawLine(pixLo, 0, pixLo, height() );
        int pixHi = (rangeHi - lowest) / as->getHpp();
        p.drawLine(pixHi, 0, pixHi, height() );
    }

}



void WaterfallWidget::takeShot( QString shotFileName )
{
    MYINFO << "takeShot(" << shotFileName << ")";
    img->save(shotFileName);
}
