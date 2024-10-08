#ifndef SAC_MAINWINDOW_H
#define SAC_MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include "sac_network.h"
#include "../OfflineReconClient/ort_modelist.h"
#include "../Client/rds_log.h"

#include "../OfflineReconClient/ort_returnonfocus.h"
#include "../CloudTools/yct_configuration.h"
#include "../CloudTools/yct_api.h"
#include "../CloudTools/yct_prepare/yct_twix_anonymizer.h"


namespace Ui
{
    class sacMainWindow;
}


struct Task
{
    QString     taskID;
    QString     scanFilename;
    int         paramValue;
    QString     patientName;
    QString     accNumber;
    QString     mode;
    QString     modeReadable;
    QString     notification;
    QString     taskFilename;
    QString     lockFilename;
    QString     protocolName;
    QDateTime   taskCreationTime;
    qint64      scanFileSize;

    QString     uuid;
    bool        cloudReconstruction;

    QStringList additionalFiles;
    QStringList additionalFilesOriginalName;
};


enum TaskPriority
{
    Normal=0,
    Night,
    HighPriority
};


class sacMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit sacMainWindow(QWidget *parent, bool isConsole);
    ~sacMainWindow();

    bool restartApp;
    bool isConsole;

    sacNetwork  network;
    ortModeList modeList;
    rdsLog      log;

    yctConfiguration  cloudConfig;
    yctAPI            cloud;

    bool firstFileDialog;
    int defaultMode;

    bool paramVisible;
    bool additionalFilesVisible;

    QString filename;

    void checkValues();
    void findDefaultMode();

    int detectMode(QString protocol);

    bool didStart;

    Task task;
    bool generateTaskFile(Task& a_task, bool cloudRecon=false);
    void analyzeDatFile(QString filename, QString& detectedPatname, QString& detectedProtocol);
    bool submitFileOfBatch(QString file_path, QString file_name, QString mode, QString notification, TaskPriority priority);
    bool handleBatchFile(QString file);
    bool submitBatch(QStringList files, QStringList modes, QString notify, TaskPriority priority);
    void updateDialogHeight();
    bool readBatchFile(QString fileName, QStringList& files, QStringList& modes, QString& notify, TaskPriority& priority);
    bool copyAdditionalFiles(QString taskID);

    bool processCloudRecon();
    bool showCloudProblem(QString text);

private slots:
    void on_selectFileButton_clicked();
    void on_cancelButton_clicked();
    void on_sendButton_clicked();
    void on_modeCombobox_currentIndexChanged(int index);
    void on_taskIDEdit_editingFinished();
    void on_logoLabel_customContextMenuRequested(const QPoint &pos);
    void showLogfile();
    void showConfiguration();
    void showBatchDialog();
    void showFirstConfiguration();

    void on_clearAdditionalFilesButton_clicked();

    void on_selectAdditionalFilesButton_clicked();

private:
    Ui::sacMainWindow *ui;
    ortReturnOnFocus returnFocusHelper;
    QString additionalFilesError;
};


#endif // SAC_MAINWINDOW_H
