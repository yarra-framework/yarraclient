#ifndef SAC_CONFIGURATIONDIALOG_H
#define SAC_CONFIGURATIONDIALOG_H

#include <QDialog>

namespace Ui {
class sacConfigurationDialog;
}

class sacMainWindow;

class sacConfigurationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit sacConfigurationDialog(QWidget *parent = 0);
    ~sacConfigurationDialog();

    void prepare(sacMainWindow* mainWindowPtr);

    bool closeMainWindow;

private slots:
    void on_cancelButton_clicked();

    void on_saveButton_clicked();

private:
    Ui::sacConfigurationDialog *ui;

    sacMainWindow* mainWindow;

};

#endif // SAC_CONFIGURATIONDIALOG_H
