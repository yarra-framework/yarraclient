#ifndef SAC_BATCHDIALOG_H
#define SAC_BATCHDIALOG_H

#include <QDialog>
#include <QStringListModel>
#include <QFileDialog>
#include "../OfflineReconClient/ort_modelist.h"
#include "sac_mainwindow.h"


namespace Ui {
class sacBatchDialog;
}


class sacBatchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit sacBatchDialog(QWidget *parent = 0);
    ~sacBatchDialog();
    QStringListModel *files;
    QStringListModel *modes;

    sacMainWindow* mainWindow;
    void prepare(QList<ortModeEntry*> modes, QString notification, int defaultMode);

    QList<ortModeEntry*> modesInfo;

private slots:
    void on_addFileButton_clicked();
    void on_removeFileButton_clicked();
    void on_addModeButton_clicked();
    void on_removeModeButton_clicked();
    void on_importBatchFileButton_clicked();
    void on_exportBatchFileButton_clicked();

public:
    Ui::sacBatchDialog *ui;

};


#endif // SAC_BATCHDIALOG_H
