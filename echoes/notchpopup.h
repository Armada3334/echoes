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


#ifndef NOTCHPOPUP_H
#define NOTCHPOPUP_H

#include <QtCore>
#include <QtWidgets>
#include "settings.h"


namespace Ui {
class NotchPopup;
}



class NotchPopup : public QDialog
{
    Q_OBJECT

private:
    Ui::NotchPopup *ui;

protected:
    Settings*   as;
    Notch       nt;
    bool        brandNew;
    int         index;

    //virtual void showEvent(QShowEvent *event);


protected slots:
    void slotDeleteNotch();
    void slotOk();
    void slotCancel();
    void slotFreqChange(int freq);
    void slotWidthChange(int width);

public:
    ///
    /// \brief NotchPopup
    /// \param n
    /// \param appSettings
    /// \param parent
    ///
    explicit NotchPopup(Notch n, Settings *appSettings, QWidget *parent = 0);
    ~NotchPopup();

    Notch getNotch() const
    {
        return nt;
    }

    void setIndex(int i)
    {
        index = i;
    }

    int getIndex() const
    {
        return index;
    }

signals:
    void notchChanged(int idx);

};

#endif // NOTCHPOPUP_H
