#include "yca_mainwindow.h"
#include "yca_global.h"
#include "ui_yca_mainwindow.h"

#include <QDesktopWidget>
#include <QMessageBox>
#include <QTest>

#include "../CloudTools/yct_common.h"
#include "../CloudTools/yct_aws/qtaws.h"


ycaWorker::ycaWorker()
{
    //qInfo() << "Main Thread: " << QThread::currentThreadId();

    processingActive=false;
    currentProcess=Idle;
    currentTaskID="";
    userInvalidShown=false;

    moveToThread(&transferThread);
    transferTimer.moveToThread(&transferThread);
    transferThread.start();
}


void ycaWorker::setParent(ycaMainWindow* myParent)
{
    parent=myParent;
    QMetaObject::invokeMethod(this, "startTimer", Qt::QueuedConnection);
}


void ycaWorker::shutdown()
{
    QMetaObject::invokeMethod(this, "stopTimer", Qt::QueuedConnection);
}


void ycaWorker::trigger()
{
    if (!processingActive)
    {
        QMetaObject::invokeMethod(this, "timerCall", Qt::QueuedConnection);
    }
}


void ycaWorker::startTimer()
{
    this->connect(&transferTimer, SIGNAL(timeout()), SLOT(timerCall()), Qt::DirectConnection);
    transferTimer.setInterval(60000);
    transferTimer.start();
    updateParentStatus();
}


void ycaWorker::stopTimer()
{
    transferTimer.stop();
    transferThread.quit();
}


void ycaWorker::timerCall()
{
    //qInfo() << "Called. Thread: " << QThread::currentThreadId();

    processingActive=true;
    transferTimer.stop();

    ycaTaskList taskList;
    parent->taskHelper.getScheduledTasks(taskList);

    if (!taskList.empty())
    {
        if (!parent->cloud.validateUser(&transferInformation))
        {
            if (!userInvalidShown)
            {
                QMetaObject::invokeMethod(parent, "showNotification", Qt::QueuedConnection,  Q_ARG(QString, "Cannot process cases. User account is missing payment information or has been diabled."));

                // Avoid that the message is shown during every timer event
                userInvalidShown=true;
            }
        }
        else
        {
            QMetaObject::invokeMethod(parent, "showIndicator", Qt::QueuedConnection);

            currentProcess=Upload;
            updateParentStatus();

            // Only upload 10 cases per time, then first download the recon
            int uploadCount=0;

            while ((!taskList.isEmpty()) && (uploadCount<10))
            {
                if (!parent->cloud.uploadCase(taskList.takeFirst(),&transferInformation))
                {
                    // TODO: Error handling
                }
                uploadCount++;
            }

            // Update list again? Maybe new cases have been added meanwhile

            QMetaObject::invokeMethod(parent, "hideIndicator", Qt::QueuedConnection);
        }
    }

    // TODO: Check status of incoming jobs
    // TODO: Download incoming jobs on at a time
    currentProcess=Download;
    updateParentStatus();


    // TODO: Push downloaded jobs to destination
    // TODO: Retrieve storage location for each job
    currentProcess=Storage;
    updateParentStatus();
    QTest::qSleep(5000);

    currentProcess=Idle;
    updateParentStatus();

    transferTimer.start();
    processingActive=false;

}


void ycaWorker::updateParentStatus()
{
    QString text="Unknown";

    switch (currentProcess)
    {
    default:
    case Idle:
        text="<span style=\" font-weight:600; color:#E0A526;\">Idle</span>";
        break;
    case Upload:
        text="<span style=\" font-weight:600; color:#580f8b;\">Uploading tasks...</span>";
        break;
    case Download:
        text="<span style=\" font-weight:600; color:#580f8b;\">Downloading results...</span>";
        break;
    case Storage:
        text="<span style=\" font-weight:600; color:#580f8b;\">Storing results...</span>";
        break;
    }

    QMetaObject::invokeMethod(parent, "showStatus", Qt::QueuedConnection,  Q_ARG(QString, text));
}


ycaMainWindow::ycaMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ycaMainWindow)
{
    ui->setupUi(this);
    shuttingDown=false;

    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::MSWindowsFixedSizeDialogHint;
    flags &= ~Qt::WindowContextHelpButtonHint;
    setWindowFlags(flags);

    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction("YarraCloud Agent v" + QString(YCA_VERSION));
    trayIconMenu->addSeparator();
    trayIconMenu->addAction("Show window",this, SLOT(showNormal()));
    trayItemShutdown=trayIconMenu->addAction("Shutdown",this, SLOT(callShutDown()));

    QIcon icon = YCA_ICON;
    setWindowIcon(icon);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(icon);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setToolTip("YarraCloud Agent");
    trayIcon->show();

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

    connect(trayIcon, SIGNAL(messageClicked()), this, SLOT(show()));


    ui->versionLabel->setText("Version " + QString(YCA_VERSION) + ", Build date " + QString(__DATE__));

    // Center the window on the screen
    setGeometry(QStyle::alignedRect(Qt::LeftToRight,Qt::AlignCenter,size(),
                                    qApp->desktop()->availableGeometry()));

    indicator.setMainDialog((QDialog*) this);

    cloud.setConfiguration(&config);
    config.loadConfiguration();
    taskHelper.setCloudInstance(&cloud);

    if (!cloud.createCloudFolders())
    {
        QMessageBox msgBox(0);
        msgBox.setWindowTitle("Yarra Cloud Agent");
        msgBox.setText("Unable to prepare folder for cloud reconstruction.<br>Please check you local client installation.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setWindowIcon(YCA_ICON);
        msgBox.setIcon(QMessageBox::Information);
        msgBox.exec();

        transferWorker.shutdown();
        shuttingDown=true;
        QTimer::singleShot(0, qApp, SLOT(quit()));
        return;
    }

    QDir appDir(qApp->applicationDirPath());
    if (!appDir.exists("YCA_helper.exe"))
    {
        QMessageBox msgBox(0);
        msgBox.setWindowTitle("Yarra Cloud Agent");
        msgBox.setText("Unable to find folder application YCA_helper.exe.<br>Please check you local client installation.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setWindowIcon(YCA_ICON);
        msgBox.setIcon(QMessageBox::Information);
        msgBox.exec();

        transferWorker.shutdown();
        shuttingDown=true;
        QTimer::singleShot(0, qApp, SLOT(quit()));
        return;
    }

    transferWorker.setParent(this);
    taskHelper.clearTaskList(taskList);
}


ycaMainWindow::~ycaMainWindow()
{
    delete ui;
}


void ycaMainWindow::closeEvent(QCloseEvent* event)
{
    if (trayIcon->isVisible())
    {
        hide();
        event->ignore();
    }
}


void ycaMainWindow::callShutDown(bool askConfirmation)
{
    // TODO: Check if there are pending uploads/downloads

    if (askConfirmation)
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Shutdown Agent?");
        msgBox.setText("Do you really want to shutdown the agent? Outgoing jobs and finished reconstructions will not be transfered anymore.");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        msgBox.setWindowIcon(YCA_ICON);
        int ret = msgBox.exec();

        if (ret==QMessageBox::Yes)
        {
            transferWorker.shutdown();
            qApp->quit();
        }
    }
    else
    {
        qApp->quit();
    }
}


void ycaMainWindow::showIndicator()
{
    indicator.showIndicator();
}


void ycaMainWindow::hideIndicator()
{
    indicator.hideIndicator();
}


void ycaMainWindow::callSubmit()
{
    transferWorker.trigger();
}


void ycaMainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        showNormal();
        break;
    default:
        break;
    }
}

void ycaMainWindow::on_closeButton_clicked()
{
    hide();
}


void ycaMainWindow::on_closeContextButton_clicked()
{
    QMenu infoMenu(this);
    infoMenu.addAction("Restart", this,  SLOT(callShutDown()));
    infoMenu.addAction("Shutdown", this, SLOT(callShutDown()));
    infoMenu.exec(ui->closeContextButton->mapToGlobal(QPoint(0,0)));
}


void ycaMainWindow::on_statusRefreshButton_clicked()
{
    mutex.lock();
    taskHelper.getAllTasks(taskList, true, false);
    mutex.unlock();

    ui->activeTasksTable->setRowCount(taskList.count());
    ui->activeTasksTable->setColumnCount(5);

    ui->activeTasksTable->setHorizontalHeaderItem(0,new QTableWidgetItem("Status"));
    ui->activeTasksTable->setHorizontalHeaderItem(1,new QTableWidgetItem("Patient"));
    ui->activeTasksTable->setHorizontalHeaderItem(2,new QTableWidgetItem("ACC"));
    ui->activeTasksTable->setHorizontalHeaderItem(3,new QTableWidgetItem("MRN"));
    ui->activeTasksTable->setHorizontalHeaderItem(4,new QTableWidgetItem("ID"));

    for (int i=0; i<taskList.count(); i++)
    {
        ui->activeTasksTable->setItem(i,0,new QTableWidgetItem(""));
        ui->activeTasksTable->setItem(i,1,new QTableWidgetItem(taskList.at(i)->patientName));
        ui->activeTasksTable->setItem(i,2,new QTableWidgetItem(taskList.at(i)->acc));
        ui->activeTasksTable->setItem(i,3,new QTableWidgetItem(taskList.at(i)->mrn));
        ui->activeTasksTable->setItem(i,4,new QTableWidgetItem(taskList.at(i)->uuid));
    }


/*
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QtAWSRequest awsRequest(config.key, config.secret);
    QtAWSReply reply=awsRequest.sendRequest("POST", "api.yarracloud.com", "v1/modes",
                                            QByteArray(), YCT_API_REGION, QByteArray(), QStringList());

    ui->activeTasksTable->setRowCount(1);
    ui->activeTasksTable->setColumnCount(1);

    if (!reply.isSuccess())
    {
        ui->activeTasksTable->setItem(0,0,new QTableWidgetItem(QString("Error")));
    }
    else
    {
        ui->activeTasksTable->setItem(0,0,new QTableWidgetItem(QString(reply.replyData())));
    }

    QApplication::restoreOverrideCursor();
*/
}


void ycaMainWindow::on_pushButton_5_clicked()
{
    transferWorker.trigger();
    //indicator.showIndicator();
}


void ycaMainWindow::on_pushButton_3_clicked()
{
    indicator.hideIndicator();
}


void ycaMainWindow::showNotification(QString text)
{
    trayIcon->showMessage("YarraCloud", text, QSystemTrayIcon::NoIcon, 20000);
}


void ycaMainWindow::showStatus(QString text)
{
    ui->statusLabel->setText(text);
}

