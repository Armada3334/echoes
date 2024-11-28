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
$Rev:: 67                       $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-02-07 23:36:02 +01#$: Date of last commit
$Id: ruler.h 67 2018-02-07 22:36:02Z gmbertani $
*******************************************************************************/

#include "notchpopup.h"
#include "ui_notchpopup.h"

NotchPopup::NotchPopup(Notch n,  Settings* appSettings, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NotchPopup)
{
    ui->setupUi(this);

    MY_ASSERT( nullptr !=  appSettings)
    as = appSettings;

    nt = n;

    brandNew = false;
    if(nt.width == 0)
    {
        brandNew = true;
        nt.width = DEFAULT_NOTCH_WIDTH;
    }

    if(brandNew == true)
    {
        ui->tbDel->setEnabled(false);
        connect( ui->tbOk, SIGNAL( pressed() ), this, SLOT( slotOk() ) );
        connect( ui->tbCancel, SIGNAL( pressed() ), this, SLOT( slotCancel() ) );
    }
    else
    {
        ui->tbDel->setEnabled(true);
        connect( ui->tbOk, SIGNAL( pressed() ), this, SLOT( slotOk() ) );
        connect( ui->tbCancel, SIGNAL( pressed() ), this, SLOT( slotCancel() ) );
        connect( ui->tbDel, SIGNAL( pressed() ), this, SLOT( slotDeleteNotch() ) );
    }

    int min, max;
    as->getZoomedRange(min, max);
    ui->sbFreq->setMinimum(min);
    ui->sbFreq->setMaximum(max);
    ui->sbFreq->setValue(nt.freq);
    ui->hsBand->setValue(nt.width);

    ui->hsBand->setMinimum(DEFAULT_NOTCH_WIDTH);

    int zoomLo, zoomHi, coverage;
    as->getZoomedRange(zoomLo, zoomHi);
    coverage = zoomHi - zoomLo;
    ui->hsBand->setMaximum(coverage);
    //ui->hsBand->setPageStep(coverage/10);
    //ui->hsBand->setSingleStep(coverage/100);

    connect( ui->sbFreq, SIGNAL( valueChanged(int) ), this, SLOT( slotFreqChange(int) ) );
    connect( ui->hsBand, SIGNAL( valueChanged(int) ), this, SLOT( slotWidthChange(int) ) );


}

void NotchPopup::slotFreqChange(int freq)
{
    nt.freq = freq;
    nt.begin = nt.freq - (nt.width / 2);
    nt.end = nt.freq + (nt.width / 2);

    emit notchChanged(index);
}

void NotchPopup::slotWidthChange(int width)
{
    nt.width = width;
    nt.begin = nt.freq - (nt.width / 2);
    nt.end = nt.freq + (nt.width / 2);

    emit notchChanged(index);
}


void NotchPopup::slotDeleteNotch()
{
    nt.width = 0;

    //confirmation not requested
    accept();
}


void NotchPopup::slotOk()
{
    nt.freq = ui->sbFreq->value();
    nt.width = ui->hsBand->value();
    nt.begin = nt.freq - (nt.width / 2);
    nt.end = nt.freq + (nt.width / 2);
    accept();
}

void NotchPopup::slotCancel()
{
    reject();
}

NotchPopup::~NotchPopup()
{
    delete ui;
}
