#ifndef RDS_COPYDIALOG_H
#define RDS_COPYDIALOG_H

#include <QDialog>
#include <QtWidgets>

namespace Ui {
class rdsCopyDialog;
}

class rdsCopyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit rdsCopyDialog(QWidget *parent = 0);
    ~rdsCopyDialog();

public slots:
    void setProgress(int percent);

private:
    Ui::rdsCopyDialog *ui;
};

#endif // RDS_COPYDIALOG_H
