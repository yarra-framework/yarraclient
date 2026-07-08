#include "rds_copydialog.h"
#include "ui_rds_copydialog.h"

#include <QDesktopWidget>

rdsCopyDialog::rdsCopyDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::rdsCopyDialog)
{
    ui->setupUi(this);

    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::MSWindowsFixedSizeDialogHint;
    flags |= Qt::FramelessWindowHint;
    flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);

    QPalette p = palette();
    p.setColor(QPalette::Highlight, QColor(88,15,139) );
    ui->progressBar->setPalette(p);

    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignRight | Qt::AlignBottom, size(), qApp->primaryScreen()->availableGeometry()));

    connect(ui->dismissButton, &QPushButton::clicked, this, &rdsCopyDialog::close);
}

rdsCopyDialog::~rdsCopyDialog()
{
    delete ui;
}


void rdsCopyDialog::setProgress(int percent)
{
    ui->progressBar->setValue(qBound(0, percent, 100));
}


void rdsCopyDialog::setProgressCount(int completed, int total)
{
    ui->progressCount->setText(QString("%1 / %2").arg(completed).arg(total));
}


void rdsCopyDialog::setScanningAllowed(bool allowed)
{
    if (allowed)
    {
        ui->textLabel->setText("<html><head/><body><p>Transferring raw data to a remote server.<br><span style=\"color:#1B7A1B;\"><b>Scanning is allowed.</b></span></p></body></html>");
    }
    else
    {
        ui->textLabel->setText("<html><head/><body><p>Transferring raw data to a remote server.<br><span style=\"color:#C0392B;\"><b>Do not start a new scan yet.</b></span></p></body></html>");
    }
}
