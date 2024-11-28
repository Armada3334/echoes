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
$Id: dbmpalette.h 23 2018-01-17 16:28:16Z gmbertani $
*******************************************************************************/

#ifndef DBMPALETTE_H
#define DBMPALETTE_H

#include <QtCore>
#include <QtWidgets>

#include "settings.h"

class DbmPalette : public QFrame
{
    Q_OBJECT

    Settings* as;                   //current application settings

    bool showDbmScale;              //show dbm values
    bool vert;                      //true for vertical palette

    QRect rect;
    QMap<int, QColor> valPalette;   //dBfs vs. color
    QVector<QRgb> colorTable;       //complete dBfs vs. color conversion table

    int maxVal;                     //scale start
    int minVal;                     //scale end
    int dbmStep;                 //dbm scale increment step [dBfs]

    int scaleExt;                  //scale extension

    int brightness;                //offset [dBfs] from minVal (first color) to second color
    int contrast;                  //delta dBfs between adiacent colors. The last color covers
                                    //all the values till maxVal.

    QMap<float,float> bCurve;     //brightness correction curve
    QMap<float,float> cCurve;     //contrast correction curve

    void reset();
    void recalcPalette();
    void buildColorTable();
    QColor bestFgColor(QColor bgColor); //color that best contrasts the given background color

    virtual void paintEvent( QPaintEvent *pe );
    //virtual void resizeEvent ( QResizeEvent* event );

public:
    explicit DbmPalette(Settings* appSettings, int dbmIncr, QWidget *parent = nullptr);
    ~DbmPalette();

    void paletteExtension(int& palBeg, int& palEnd, int& palExc);

    QColor neutralColor();              //a color not included in palette, to be used for tick marks

    void addPoint(QColor color, bool clr = false);
    void refresh();
    void showDbm(bool show);
    void setDbmPalette(int b, int c);
    int getDbmAbsolute(int dBfs);
    int getColorDbm(int dBfs);
    int getDbmPercent(int dBfs);
    void updatePalette();

    QVector<QRgb> getColorTable()
    {
        return colorTable;
    }

signals:

public slots:

};

#endif // DBMPALETTE_H
