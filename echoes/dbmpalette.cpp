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
$Rev:: 252                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-07-11 07:59:12 +02#$: Date of last commit
$Id: dbmpalette.cpp 252 2018-07-11 05:59:12Z gmbertani $
*******************************************************************************/

#include "setup.h"
#include "dbmpalette.h"

DbmPalette::DbmPalette(Settings* appSettings, int dbmIncr, QWidget *parent) :
    QFrame(parent)
{
    setObjectName("DbmPalette widget");

    MY_ASSERT( nullptr !=  appSettings)
    as = appSettings;

    reset();

    brightness = DEFAULT_BRIGHTNESS;
    contrast = DEFAULT_CONTRAST;

    MY_ASSERT(dbmIncr > 2)
    dbmStep = dbmIncr;


    minVal = static_cast<int>(as->getMinDbfs());
    maxVal = static_cast<int>(as->getMaxDbfs());
    scaleExt = maxVal - minVal;

    //takes all the space in the host widget
    rect = parent->contentsRect();
    resize(rect.width() - 2, rect.height() - 2);
    updatePalette();
}

DbmPalette::~DbmPalette()
{

}

QColor DbmPalette::neutralColor()
{
    switch (as->getPalette())
    {
    case PALETTE_CLASSIC:
        return Qt::cyan;

    default:
    case PALETTE_BW:
        return QColor("yellow");
        break;
    }
}

void DbmPalette::updatePalette()
{

    minVal = static_cast<int>(as->getMinDbfs());
    maxVal = static_cast<int>(as->getMaxDbfs());
    scaleExt = maxVal - minVal;

    if( as->getPalette() == 1 )
    {
        //color palette
        addPoint(Qt::black, true);
        addPoint(Qt::blue);
        addPoint(QColor("purple"));
        addPoint(QColor("orange"));
        addPoint(Qt::yellow);
        addPoint(Qt::white);
    }
    else
    {
        //grayscale palette points
        addPoint(QColor(0x00, 0x00, 0x00), true);
        addPoint(QColor(0x20, 0x20, 0x20));
        addPoint(QColor(0x40, 0x40, 0x40));
        addPoint(QColor(0x60, 0x60, 0x60));
        addPoint(QColor(0x80, 0x80, 0x80));
        addPoint(QColor(0xa0, 0xa0, 0xa0));
        addPoint(QColor(0xc0, 0xc0, 0xc0));
        addPoint(QColor(0xff, 0xff, 0xff));
    }

    //The brightness and contrast sliders are filtered
    //through customized curve functions in order to be
    //effective even at low % values.

    //brightness curve
    //           %in        %out
    bCurve.insert(0.0,      0.0);
    bCurve.insert(20.0,     60.0);
    bCurve.insert(40.0,     80.0);
    bCurve.insert(90.0,     90.0);
    bCurve.insert(100.0,    100.0);

    //contrast curve
    cCurve.insert(0.0,      0.0);
    cCurve.insert(20.0,     50.0);
    cCurve.insert(40.0,     70.0);
    cCurve.insert(60.0,     85.0);
    cCurve.insert(100.0,    92.0);
    //TODO: allow for more customized palettes
    refresh();
    update();
}

int DbmPalette::getDbmAbsolute(int dBfs)
{
    //convert dBfs to absolute positive scale range
    int dBfsAbs = dBfs - minVal;

    if( dBfsAbs < 0 )
    {
        //bottom clipping
        dBfsAbs = 0;
    }
    else if( static_cast<int>(dBfsAbs) >= static_cast<int>(scaleExt) )
    {
        //top clipping
        dBfsAbs = scaleExt - 1;
    }

    return dBfsAbs;
}

int DbmPalette::getColorDbm(int dBfs)
{
    //convert dBfs to absolute positive scale range
    //the value returned is clipped to the highest
    //entry in colorTable (the minimum dBfs shown as white)
    int dBfsAbs = dBfs - minVal;

    int check = dBfsAbs;

    if( dBfsAbs < 0 )
    {
        dBfsAbs = 0;
    }
    else if( static_cast<int>(dBfsAbs) >= colorTable.count() )
    {
        dBfsAbs = colorTable.count() - 1;
    }
    if(dBfsAbs < 0)
    {
        MYINFO << "preventing BUG dBfsAbs=" << dBfsAbs << " dBfs=" << dBfs << " check=" << check;
        dBfsAbs = 0;
    }


    return dBfsAbs;
}

int DbmPalette::getDbmPercent(int dBfs)
{
    //convert dBfs to a percentage based on maxVal-minVal interval:
    //so minVal=0% and maxVal=100%

    int dBfsAbs = getDbmAbsolute(dBfs);
    MY_ASSERT(dBfsAbs >= 0)

    int delta = maxVal - minVal;
    MY_ASSERT(delta > 0)

    int percent = (dBfsAbs * 100) / delta;

    return percent;
}


void DbmPalette::reset()
{
    valPalette.clear();
    colorTable.clear();

    showDbmScale = true;
    vert = false;
}

void DbmPalette::addPoint(QColor color, bool clr)
{
    int dBfs = 0;

    if(clr == true)
    {
        valPalette.clear();
    }

    //MY_ASSERT(contrast > 0);
    int count = valPalette.count();
    if(count == 0)
    {
        dBfs = minVal;
        MYDEBUG << "DbmPalette::addPoint(" << color << ") is the first color beginning at " << dBfs << " dBfs";
    }
    else
    {
        dBfs = minVal + brightness + (((count-1) * contrast )/ 10);
        MYDEBUG << "DbmPalette::addPoint(" << color << ") is the " << count+1 << " color beginning at " << dBfs << " dBfs";

        //if dBfs exceeds maxVal, the color won't be used unless decreasing brightness
    }

    valPalette.insert(dBfs, color);
}

void DbmPalette::paletteExtension(int& palBeg, int& palEnd, int &palExc)
{
    QList<int> dBfsList = valPalette.keys();
    palBeg = dBfsList.first() + (as->getGain() / 10);
    palEnd = dBfsList.last();

    palExc = -palBeg;
    palExc += palEnd;

    MYDEBUG << "palBeg=" << palBeg << " palEnd=" << palEnd << " palExc=" << palExc;

}

void DbmPalette::recalcPalette()
{
    //recalculate the dBfs values when brightness and/or contrast changes

    int dBfs = 0;
    int count = 0;
    QColor color;
    QMap<int, QColor> valPaletteNew;


    minVal = static_cast<int>(as->getMinDbfs());
    maxVal = static_cast<int>(as->getMaxDbfs());
    scaleExt = maxVal - minVal;


    QList<int> dBfsList = valPalette.keys();
    while( dBfsList.isEmpty() == false )
    {
        dBfs = dBfsList.takeFirst();
        color = valPalette.value(dBfs);

        if(count == 0)
        {
            //first element is always at minVal
            dBfs = minVal;
        }
        else
        {
            //Intermediate values must not overlap.
            //The values can exceed maxVal, in that case
            //they won't be used.
            dBfs = minVal + brightness + ((count * contrast)/10) + count;

            //At least the first two values must
            //be in range minVal-maxVal to avoid buildColorTable() crashes
            if(dBfs > maxVal && count == 1)
            {
                dBfs = maxVal;
            }
        }

        valPaletteNew.insert(dBfs, color);
        count++;
    }

    MY_ASSERT( valPalette.count() == valPaletteNew.count() )
    valPalette = valPaletteNew;

}


void DbmPalette::refresh()
{
    QPoint start,end;

    recalcPalette();

    rect = parentWidget()->contentsRect();
    resize( rect.width(), rect.height() );

    if(rect.height() > rect.width())
    {
        //vertical scale, lowest value at bottom, highest at top
        start.setX(rect.height());
        start.setY(rect.width()/2);
        end.setX(0);
        end.setY(rect.width()/2);
        vert = true;
    }
    else
    {
        //horizontal scale, lowest value at left, highest at right
        start.setX(0);
        start.setY(rect.height()/2);
        end.setX(rect.width());
        end.setY(rect.height()/2);
        vert = false;
    }

    buildColorTable();
    update();
}

void DbmPalette::paintEvent( QPaintEvent *pe )
{
    Q_UNUSED(pe)

    int x,y, dBfs, pixDbm;
    QMap<int, int> posPalette;          //dBfs vs. label positions

    QPainter p(this);

    p.setRenderHint(QPainter::TextAntialiasing);

    //how many pixels for each dBfs value
    if(vert == false)
    {
        pixDbm = rect.width() / scaleExt;
    }
    else
    {
        pixDbm = rect.height() / scaleExt;
    }

    QBrush b(Qt::SolidPattern);
    b.setColor( colorTable.at( getColorDbm(maxVal) ) );
    p.fillRect(rect, b);

    QRect segment;
    x = rect.x()+1;
    if(vert == false)
    {
        y = rect.y()+1;
    }
    else
    {
        y = rect.height() - 1;
    }

    for(dBfs = minVal; dBfs <= maxVal; dBfs++)
    {
        if(vert == false)
        {
            segment.setRect( x, y, pixDbm, rect.height()  );
            posPalette.insert(dBfs, x);
            x+= pixDbm;
        }
        else
        {
            segment.setRect( x, y, rect.width(), pixDbm  );
            posPalette.insert(dBfs, y);
            y-= pixDbm;
        }

        b.setColor( colorTable.at( getColorDbm(dBfs) ) );
        p.fillRect(segment, b);
    }

    if(showDbmScale == true)
    {
        //QList<int> dBfsList = valPalette.keys();
        QList<int> dBfsList;
        QColor color;
        QColor textColor;

        QFont font = parentWidget()->font();
        QString vals;

        font = parentWidget()->font();

        for(dBfs = minVal; dBfs <= maxVal; dBfs += dbmStep)
        {
            dBfsList.append(dBfs);
        }

        while( dBfsList.isEmpty() == false )
        {
            dBfs = dBfsList.takeFirst();

            if(vert == true)
            {
                //shrinks the current font to fit into the scale
                //font.setPixelSize( contentsRect().width() );

                //calculate Y position for the scale value
                y = /*contentsRect().height() - */posPalette.value(dBfs);
                x = 8;
            }
            else
            {
                //shrinks the current font to fit into the scale
                //font.setPixelSize( contentsRect().height() );

                //calculate X position for the scale value
                x = posPalette.value(dBfs) ;
                y = contentsRect().height() - 4;
            }
            color = colorTable.at( getColorDbm(dBfs) );
            textColor = bestFgColor(color);

            //vals.sprintf("%+i", dBfs);
            vals = QString("%1%2")
                    .arg(dBfs < 0 ? '-' : '+')
                    .arg(dBfs);

            p.setFont(font);
            p.setPen(textColor);
            if(x >= rect.width() && vert == false)
            {
                QFontMetrics fm(font);
                QRect br = fm.boundingRect(vals);
                x -= br.width();
            }

            if(y >= rect.height() && vert == true)
            {
                QFontMetrics fm(font);
                QRect br = fm.boundingRect(vals);
                y += br.height(); //-=
            }

            p.drawText(x, y, vals);
        }
    }

}

void DbmPalette::setDbmPalette(int b, int c)
{
    MYDEBUG << "b=" << b << " c=" << c;

    //slider percentages are mapped against custom curves
    int bCorrect = static_cast<int>( as->funcGen( &bCurve, static_cast<float>( b ) ) );
    int cCorrect = static_cast<int>( as->funcGen( &cCurve, static_cast<float>( c ) ) );

    //converts percentage values to absolute dBfs
    brightness = ((101-bCorrect) * scaleExt ) / 100;
    contrast   = ((101-cCorrect) * scaleExt ) / 100;

    updatePalette();
}

void DbmPalette::buildColorTable()
{
    uchar r, g, b;
    float kr, kg, kb;
    QColor cFrom, cTo;
    int dbm, dbmFrom, dbmTo, dbmAbs, deltaDbm;

    MYDEBUG << __func__;

    colorTable.clear();
    QMapIterator<int,QColor> i(valPalette);
    QList<int> keys = valPalette.keys();


    i.next();
    cFrom = i.value();
    dbmFrom = i.key();

    dbmAbs = 0;
    while ( i.hasNext() == true )
    {
        i.next();

        cTo = i.value();
        dbmTo = i.key();

        if(dbmTo > maxVal)
        {
            dbmTo = maxVal;                         //max dBfs value
            cTo = valPalette.value( keys.last() );  //the last color in palette value
        }

        deltaDbm = ((dbmTo - minVal) - (dbmFrom - minVal));
        if(deltaDbm > 0)
        {
            kr = ( cTo.red() - cFrom.red() ) /      deltaDbm ;
            kg = ( cTo.green() - cFrom.green() ) /  deltaDbm;
            kb = ( cTo.blue() - cFrom.blue() ) /    deltaDbm;
        }
        else
        {
            break;
        }
        //MYDEBUG << "kR=" << kr << " kG=" << kg << " kB=" << kb;

        //MYDEBUG << "dbmFrom=" << dbmFrom << " colorFrom R=" << cFrom.red() << " colorFrom G=" << cFrom.green() << " colorFrom B=" << cFrom.blue();
        //MYDEBUG << "dbmTo=" << dbmTo << " colorTo R=" << cTo.red() << " colorTo G=" << cTo.green() << " colorTo B=" << cTo.blue();

        deltaDbm = 0;
        for(dbm = dbmFrom; dbm < dbmTo; dbm++)
        {
            dbmAbs++;
            deltaDbm++;

            r = static_cast<uchar>(cFrom.red()   + (kr * deltaDbm));
            g = static_cast<uchar>(cFrom.green() + (kg * deltaDbm));
            b = static_cast<uchar>(cFrom.blue()  + (kb * deltaDbm));

            //MYDEBUG << "dbm=" << dbm << " dbmAbs=" << dbmAbs << " R=" << r << " G=" << g << " B=" << b;

            colorTable.append( qRgb(r,g,b) );
        }

        cFrom = cTo;
        dbmFrom = dbmTo;
    }

    MY_ASSERT(colorTable.count() > 1)
    MYDEBUG << "created a " << colorTable.count() << " elements table.";
}


QColor DbmPalette::bestFgColor(QColor bgColor)
{
    int toGray;

    // see http://stackoverflow.com/questions/946544/good-text-foreground-color-for-a-given-background-color
    //toGray = bgColor.red() * 0.299 + bgColor.green() * 0.587 + bgColor.blue() * 0.144;

    toGray = qGray(bgColor.red(), bgColor.green(), bgColor.blue());

    QColor fgColor = Qt::white;
    if (toGray > 150)
    {
        fgColor = Qt::black;
    }

    return fgColor;
}


void DbmPalette::showDbm(bool show)
{
    showDbmScale = show;
    update();
}
