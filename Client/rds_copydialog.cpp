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
