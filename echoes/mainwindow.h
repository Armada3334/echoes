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
$Rev:: 367                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-11-23 18:22:34 +01#$: Date of last commit
$Id: mainwindow.h 367 2018-11-23 17:22:34Z gmbertani $
*******************************************************************************/



#ifndef MAINWINDOW_H
#define MAINWINDOW_H


#include <QMainWindow>
#include <QtCore>
#include <QtWidgets>
#include "settings.h"

extern QThread* ctrlThread;


namespace Ui {
    class MainWindow;
}

class Control;
class Radio;
class Waterfall;

enum GUItabs
{
    GT_GLOBAL,
    GT_DEVICE,
    GT_FFT,
    GT_OUTPUT,
    GT_STORAGE,
    GT_PREFERENCES,
    GT_TOTAL
};


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:


    ///
    /// \brief MainWindow
    /// \param appSettings
    /// \param appControl
    /// \param parent
    ///
    explicit MainWindow(Settings* appSettings, Control* appControl, QWidget *parent = nullptr);
    ~MainWindow();


public slots:
    //status bar
    void slotShowStatus(const QString& status, int appState = AST_NONE );
    void slotStart();
    void slotStop();
    void showBusy();    //shows hourglass cursor
    void showFree(bool force = false);    //shows arrow cursor
    void slotAcqStopped();
    void slotAcqStarted();


protected slots:

    //command pushbuttons
    void slotOpenConfig();
    void slotSave();
    void slotSaveAs();
    void slotAboutDialog();
    void slotQuit();


    //LCD tune pushbuttons
    void slotTuneAdd10M();
    void slotTuneAdd1M();
    void slotTuneAdd100k();
    void slotTuneAdd10k();
    void slotTuneAdd1k();
    void slotTuneSub10M();
    void slotTuneSub1M();
    void slotTuneSub100k();
    void slotTuneSub10k();
    void slotTuneSub1k();

    //clicking on LCD tune
    void slotEnterTune(const QPoint& pos);


    //bypassing subsampler
    void slotSetSsBypass( int value );

    //variable elements comboboxes
    bool slotSetDevice      ( const QString &text );
    void slotSetSampleRate  ( int val );
    void slotSetSampleRateRange  ( const QString& text );
    void slotSetBandwidth    ( int val );
    void slotSetBandwidthRange ( const QString& text );
    void slotSetGain        ( int newGain );
    void slotSetAutoGain    ( int autoGain );
    void slotSetAntenna     ( const QString& text );
    void slotSetFreqRange   ( const QString& text );
    void slotSetDsBandwidth   ( const QString& text );
    void slotIncResolution();
    void slotDecResolution();


    //subwindows control checkboxes
    void slotReopen(const QString *dev);
    void slotAdjustResolution();
    void slotSetSbThresholdsParam();
    void slotValidateUpThreshold();
    void slotValidateDnThreshold();

    void slotUpdateSysInfo();
    void slotFocusChanged(QWidget* old, QWidget* now);
    void slotSetAcqMode( E_ACQ_MODE am );
    void slotSetScanMode( E_SCAN_INTEG si );

    //initial GUI setup
    void slotGUIinit();


signals:
    ///
    /// \brief stateChanged
    ///
    void stateChanged(int);

    ///
    /// \brief notifyReady
    ///
    void notifyReady();

    ///
    /// \brief acquisitionStatus
    /// \param go
    ///
    void acquisitionStatus(bool go);

    ///
    /// \brief acquisitionAbort
    /// \param what
    ///
    void acquisitionAbort(int what);


private:
    Ui::MainWindow *ui;

protected:
    Settings* as;
    Control*  ac;
    Radio* r;
    Waterfall*  wf;
    QTimer*   sysCheck;
    QLocale loc;
    QVector< QList<QWidget*>  >guiWidgetsList;
    QCursor* busyCur;
    QCursor* freeCur;

    QRecursiveMutex  protectionMutex;    ///<generic mutex to protect against concurrent calls from Control thread

    qint64     tune;             ///LCD tune value
    int       dsBw;             ///bandwidth value

    qint64    minTuneHz;    ///lower limit of the current tune frequency
    qint64    maxTuneHz;    ///higher limit of the current tune frequency

    int ast;                ///acquisition status

    bool init;              ///program just started
    bool outputEnabled;     ///acquisition mode set
    bool deviceChanged;     ///device has changed manually while initing
    bool shutdownDetected;  ///program termination required by Windows OS


    bool SRRchanged;        ///the sample rate range selection has been changed (slot called)
    bool SRchanged;         ///the sample rate spinbox has been changed (slot called)
    bool BBRchanged;        ///the bandwidth range selection has been changed (slot called)
    bool BBchanged;         ///the bandwidth spinbox has been changed (slot called)

    static int busyInst;           ///instance counter for showBusy() calls

    ///
    /// \brief GUIcontrolsLogic
    /// checks the interactions on sample rate and
    /// bandwidth GUI controls keeping their settings
    /// congruent
    void GUIcontrolsLogic();

    ///
    /// \brief refreshDownsamplingFreqs
    /// the downsampling frequencies must be
    /// the bandwidth divided by multiples of 2
    /// in order to avoid artifacts formation.
    /// So the combobox must be refilled with
    /// updated values each time the bandwidth changes.
    bool calcDownsamplingFreqs();

    ///
    /// \brief blockAllSignals
    /// \param block
    /// \return previous blocking status
    /// prevents I/O controls
    /// (comboboxes, spinboxes, checkboxes...)
    /// to send signals
    /// when modified programmatically
    bool blockAllSignals(bool block);

    ///
    /// \brief setTitle
    ///Formats the titlebar
    void setTitle();

    virtual void moveEvent ( QMoveEvent * event ) override;
    virtual void resizeEvent ( QResizeEvent* event ) override;
    virtual void closeEvent(QCloseEvent *event) override;
    virtual bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

    ///
    /// \brief eventFilter
    /// \param obj
    /// \param event
    /// \return
    //to disable tooltips globally
    ///
    bool eventFilter(QObject *obj, QEvent *event) override;

    ///
    /// \brief GUIrefresh
    ///
    void GUIrefresh();

    ///
    /// \brief localize
    /// \param sl
    ///
    void localizeRanges( QStringList& sl );

    ///
    /// \brief localizeRange
    /// \param sl
    ///
    void localizeRange( QString& s );

    ///
    /// \brief delocalizeRanges
    /// \param sl
    ///
    void delocalizeRange( QString& s );

};

#endif // MAINWINDOW_H
