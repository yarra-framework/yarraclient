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
    percent=qBound(0, percent, 100);

    // The total this is measured against can grow mid-transfer (e.g. an
    // alternating/batched update discovering more bytes - adjustment scans
    // not known upfront - as each cycle's real queue directory contents are
    // added in), which would otherwise make the bar visibly jump backwards
    // every time that happens. Never let it regress within one dialog's
    // lifetime; a genuinely new transfer gets a fresh dialog starting at 0.
    if (percent < ui->progressBar->value())
    {
        return;
    }

    ui->progressBar->setValue(percent);
}


void rdsCopyDialog::setProgressCount(int completed, int total)
{
    ui->progressCount->setText(QString("%1 / %2").arg(completed).arg(total));
}


void rdsCopyDialog::setScanningAllowed(bool allowed)
{
    if (allowed)
    {
        ui->textLabel->setText("<html><head/><body><p>Transferring data to remote server. <span style=\"color:#1BDD1B;\"><b>Scanning is allowed.</b></span></p></body></html>");
    }
    else
    {
        ui->textLabel->setText("<html><head/><body><p>Exporting and transferring data to remote server. <span style=\"color:#DD392B;\"><b>Don't start a new scan yet.</b></span></p></body></html>");
    }
}
