#include "rds_operationwindow.h"
#include "rds_configurationwindow.h"
#include "ui_rds_operationwindow.h"

#include "rds_global.h"
#include "rds_exechelper.h"


rdsOperationWindow::rdsOperationWindow(QWidget *parent, bool isFirstRun) :
    QDialog(parent),
    ui(new Ui::rdsOperationWindow)
{
    forceTermination=false;
    ui->setupUi(this);
    log.setLogWidget(ui->logEdit);    

    trayIconMenu = new QMenu(this);
    trayItemTransferNow=trayIconMenu->addAction("Transfer data now",this, SLOT(callManualUpdate()));
    trayIconMenu->addSeparator();
    trayIconMenu->addAction("Show status window",this, SLOT(showNormal()));
    trayItemConfiguration=trayIconMenu->addAction("Configuration",this, SLOT(callConfiguration()));
    trayIconMenu->addSeparator();
    trayItemShutdown=trayIconMenu->addAction("Shut down service",this, SLOT(callShutDown()));

    QIcon icon = RDS_ICON;
    setWindowIcon(icon);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(icon);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setToolTip("Yarra Data Transfer Client");
    trayIcon->show();

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

    connect(ui->shutdownButton, SIGNAL(clicked()), this, SLOT(callShutDown()));
    connect(ui->configurationButton, SIGNAL(clicked()), this, SLOT(callConfiguration()));
    connect(ui->hideButton, SIGNAL(clicked()), this, SLOT(hide()));
    connect(ui->logfileButton, SIGNAL(clicked()), this, SLOT(callShowLogfile()));
    connect(ui->transferButton, SIGNAL(clicked()), this, SLOT(callManualUpdate()));
    connect(ui->postponeButton, SIGNAL(clicked()), this, SLOT(callPostpone()));

    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::MSWindowsFixedSizeDialogHint;
    flags &= ~Qt::WindowContextHelpButtonHint;
    setWindowFlags(flags);

    setUIModeIdle();

    config.loadConfiguration();   

    if (!config.isConfigurationValid())
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Configuration invalid");
        msgBox.setText("The service has not been configured correctly. Please validate the configuration and restart the service.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setWindowIcon(RDS_ICON);
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();

        forceTermination=true;
        QTimer::singleShot(0, this, SLOT(callConfiguration()));
    }
    else
    {
        log.start();
        RTI->setLogInstance(&log);
        RTI->setConfigInstance(&config);
        RTI->setNetworkInstance(&network);
        RTI->setRaidInstance(&raid);
        RTI->setControlInstance(&control);
        RTI->setWindowInstance(this);

        // Send the serial number as source ID. If not available, fall back to the given name.
        QString scannerID=RTI_CONFIG->infoSerialNumber;
        if (scannerID=="0")
        {
            scannerID=RTI_CONFIG->infoName;
        }
        RTI_NETLOG.configure(RTI_CONFIG->logServerPath, EventInfo::SourceType::RDS, scannerID, RTI_CONFIG->logApiKey);

        // Start polling the Yarra mailbox server, if enabled
        if (RTI_CONFIG->mailboxEnabled)
        {
            log.log("Mailbox notifications enabled");
            mailbox.start();
        }

        // If the DNS lookup failed, set timer to retry at later time
        if ((RTI_NETLOG.isConfigurationError()) && (!RTI_CONFIG->logServerPath.isEmpty()))
        {
            QTimer::singleShot(RDS_NETLOG_RETRYINTERVAL, this, SLOT(retryNetlogConfiguration()));
        }

        // Notify the process controller about the start of the service
        control.setStartTime();
        updateInfoUI();

        log.log("System "+config.infoName+" / Serial # "+config.infoSerialNumber);

        // Send the version number and name along with the boot notification
        QString dataString="<data>";
        dataString+="<version>"       +QString(RDS_VERSION)                   +"</version>";
        dataString+="<name>"          +RTI_CONFIG->infoName                   +"</name>";
        dataString+="<path>"          +RTI->getAppPath()                      +"</path>";
        dataString+="<system_model>"  +RTI_CONFIG->infoScannerType            +"</system_model>";
        dataString+="<system_version>"+RTI->getSyngoMRVersionString()         +"</system_version>";
        dataString+="<system_vendor>Siemens</system_vendor>";
        dataString+="<time>"          +QDateTime::currentDateTime().toString()+"</time>";
        dataString+="</data>";
        RTI_NETLOG.postEvent(EventInfo::Type::Boot,EventInfo::Detail::Information,EventInfo::Severity::Success,"Ver "+QString(RDS_VERSION),dataString);

        // Start the timer for triggering updates. Checks update condition only every
        // minute to prevent undesired system load
        controlTimer.setInterval(RDS_TIMERINTERVAL);
        connect(&controlTimer, SIGNAL(timeout()), this, SLOT(checkForUpdate()));
        controlTimer.start();

        if (RTI_CONFIG->logSendHeartbeat)
        {
            heartbeatTimer.setInterval(RDS_HEARTBEAT_INTERVAL);
            connect(&heartbeatTimer, SIGNAL(timeout()), this, SLOT(sendHeartbeat()));
            heartbeatTimer.start();
        }

        if (RTI_CONFIG->infoShowIcon)
        {
            if (!RTI_CONFIG->startCmds.isEmpty())
            {                
                iconWindow.showStartupCommandsOption();
            }
            if (RTI_CONFIG->cloudSupportEnabled)
            {
                iconWindow.showCloudWindowOption();
            }
            if (RTI_CONFIG->infoFileExplorerItem)
            {
                iconWindow.showFileExplorerOption();
            }

            // Only show the ORT launcher if the ORT client has been configured
            if (QFile::exists(RTI->getAppPath() + "/ort.ini"))
            {
                iconWindow.showORTOption();
            }

            // Only show the Diagnostics launcher if the Diagnostics tool has been deployed
            if (QFile::exists(RTI->getAppPath() + "/Diagnostics.exe"))
            {
                iconWindow.showDiagnosticsOption();
            }

            iconWindow.show();

            if (RTI_NETLOG.isConfigurationError())
            {
                iconWindow.setError();
            }
        }

        // Indicate usage of custom timeouts
        if (RTI_CONFIG->infoRAIDTimeout!=RDS_RAIDSTORE_TIMEOUT)
        {
            log.log("Using custom timeout for RAID store: "+QString::number(RTI_CONFIG->infoRAIDTimeout)+" ms");
        }
        if (RTI_CONFIG->infoCopyTimeout!=RDS_COPY_TIMEOUT)
        {
            log.log("Using custom timeout for network copy: "+QString::number(RTI_CONFIG->infoCopyTimeout)+" ms");
        }

        if ((isFirstRun) && (!RTI_CONFIG->startCmds.isEmpty()))
        {            
            QTimer::singleShot(1,this,SLOT(runStartCmds()));
        }
    }

    if (!config.netDriveLocalBufferPath.isEmpty())
    {
        if ((!raid.setLocalBufferPath(config.netDriveLocalBufferPath)) || (!network.setLocalBufferPath(config.netDriveLocalBufferPath)))
        {
            QMessageBox msgBox;
            msgBox.setWindowTitle("Configuration invalid");
            msgBox.setText("Unable to access configured local buffer path. Raw data storage will not be possible. Please review configuration.");
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setWindowIcon(RDS_ICON);
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.exec();

            forceTermination=true;
            QTimer::singleShot(0, this, SLOT(callConfiguration()));
        }                
    }

    if (raid.isPatchedRaidToolMissing())
    {
        RTI->log("ERROR: Patched Raid Tool for VD13 is missing.");
        RTI->log("ERROR: Shutting down service.");
        QTimer::singleShot(0, this, SLOT(callImmediateShutdown()));
    }
}


rdsOperationWindow::~rdsOperationWindow()
{
    RTI->setLogInstance(0);
    log.finish();
    delete ui;
}


void rdsOperationWindow::closeEvent(QCloseEvent *event)
{
    if (trayIcon->isVisible())
    {
        hide();
        event->ignore();
    }
}


void rdsOperationWindow::keyPressEvent(QKeyEvent* event)
{
    if ((event->key()==Qt::Key_D) && (event->modifiers()==Qt::ControlModifier))
    {
        QInputDialog dbgDialog;
        dbgDialog.setInputMode(QInputDialog::TextInput);
        dbgDialog.setWindowIcon(RDS_ICON);
        dbgDialog.setWindowTitle("Password");
        dbgDialog.setLabelText("Debug Password:");
        dbgDialog.setTextEchoMode(QLineEdit::Password);

        Qt::WindowFlags flags = dbgDialog.windowFlags();
        flags |= Qt::MSWindowsFixedSizeDialogHint;
        flags &= ~Qt::WindowContextHelpButtonHint;
        dbgDialog.setWindowFlags(flags);

        if ((dbgDialog.exec()!=QDialog::Rejected) && (dbgDialog.textValue()==RDS_DBGPASSWORD))
        {
            debugWindow.show();
        }
    }

    QDialog::keyReleaseEvent(event);
}


void rdsOperationWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
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


void rdsOperationWindow::callShutDown()
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Shut down service?");
    msgBox.setText("Do you really want to shut down the service? Raw data will not be transfered and might be lost if the service is inactive for longer time.");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setWindowIcon(RDS_ICON);
    int ret = msgBox.exec();

    if (ret==QMessageBox::Yes)
    {
        log.log("Shutdown on user request.");
        log.flush();
        // Send event in non-async mode

        QNetworkReply::NetworkError networkError;
        int networkStatusCode=0;
        RTI_NETLOG.postEventSync(networkError, networkStatusCode, EventInfo::Type::Shutdown,EventInfo::Detail::Information,EventInfo::Severity::Success, "", "", 5000);

        RTI->setMode(rdsRuntimeInformation::RDS_QUIT);
        qApp->quit();
    }
}


void rdsOperationWindow::callImmediateShutdown()
{
    log.log("Immediate shutdown.");
    log.flush();
    RTI->setMode(rdsRuntimeInformation::RDS_QUIT);
    qApp->quit();
}


void rdsOperationWindow::callConfiguration()
{
    if (rdsConfigurationWindow::checkAccessPassword())
    {
        RTI->setMode(rdsRuntimeInformation::RDS_CONFIGURATION);
        qApp->quit();
    }
    else
    {
        // If the configuration password is wrong and the system has not
        // been configured yet, then shut down the program. Otherwise,
        // the user would end up in an infinite loop.
        if (forceTermination)
        {
            RTI->setMode(rdsRuntimeInformation::RDS_QUIT);
            qApp->quit();
        }
    }
}


void rdsOperationWindow::callShowLogfile()
{
    RTI->flushLog();

    // Call notepad and show the log file
    QString cmdLine="notepad.exe";
    QStringList args;
    args.append(RTI->getAppPath()+"/"+RTI->getLogInstance()->getLogFilename());

    QProcess::startDetached(cmdLine, args);
}


void rdsOperationWindow::setUIModeProgress()
{
    ui->transferButton->setEnabled(false);
    ui->logfileButton->setEnabled(false);
    ui->configurationButton->setEnabled(false);
    ui->shutdownButton->setEnabled(false);

    ui->postponeButton->setEnabled(true);

    // Disable items in the context menu of the tray icon
    trayItemTransferNow->setEnabled(false);
    trayItemConfiguration->setEnabled(false);
    trayItemShutdown->setEnabled(false);
}


void rdsOperationWindow::setUIModeIdle()
{
    ui->transferButton->setEnabled(true);
    ui->logfileButton->setEnabled(true);
    ui->configurationButton->setEnabled(true);
    ui->shutdownButton->setEnabled(true);

    ui->postponeButton->setEnabled(false);

    // Enable items in the context menu of the tray icon
    trayItemTransferNow->setEnabled(true);
    trayItemConfiguration->setEnabled(true);
    trayItemShutdown->setEnabled(true);
}


void rdsOperationWindow::checkForUpdate()
{
    // Stop the timer, ask the controller to check
    // if an update is needed. If so, switch UI
    // control to update mode, perform the update,
    // switch UI back and start the timer again.
    controlTimer.stop();

    if (control.isUpdateNeeded())
    {
        setUIModeProgress();
        control.setActivityWindow(this->isHidden());
        control.performUpdate();
        setUIModeIdle();
    }

    controlTimer.start();
}


void rdsOperationWindow::callManualUpdate()
{
    if (control.getState()==rdsProcessControl::STATE_IDLE)
    {
        controlTimer.stop();

        setUIModeProgress();
        control.setActivityWindow(this->isHidden());
        control.performUpdate();
        setUIModeIdle();

        controlTimer.start();
    }
}


void rdsOperationWindow::triggerManualUpdate()
{
    QMetaObject::invokeMethod(this, "callManualUpdate", Qt::QueuedConnection);
}


void rdsOperationWindow::callPostpone()
{
    RTI->setPostponementRequest(true);
}


void rdsOperationWindow::updateInfoUI()
{
    QString systemName=RTI_CONFIG->infoName + " / " + RTI_CONFIG->infoSerialNumber;
    ui->InfoValueSystem->setText(systemName);

    if (RTI_CONTROL->getState()==RTI_CONTROL->STATE_IDLE)
    {
        if (RTI_CONTROL->isWaitingFirstUpdate())
        {
            ui->InfoValueUpdate->setText("Waiting");
        }
        else
        {
            ui->InfoValueUpdate->setText(RTI_CONTROL->getLastUpdateTime().toString());
        }

        QFont f=ui->InfoValueError->font();
        f.setPointSize(8);
        ui->InfoValueError->setFont(f);

        if (RTI->isSevereErrors())
        {
            ui->InfoValueError->setText("<b>Severe errors occured during update approach.</b>");

            if (!this->isVisible())
            {
                iconWindow.setError();
            }
        }
        else
        {
            ui->InfoValueError->setText("");
        }
    }
    else
    {
        QFont f=ui->InfoValueError->font();
        f.setPointSize(10);
        ui->InfoValueError->setFont(f);

        ui->InfoValueUpdate->setText("Active");

        if (RTI_CONTROL->getState()==RTI_CONTROL->STATE_NETWORKTRANSFER)
        {
            ui->InfoValueError->setText("<span style=""color:#E0A526;""><b>Transfer running...<br>It is safe to scan now.</b></span>");
            //hide();
        }
        else
        {
            ui->InfoValueError->setText("<span style=""color:#E0A526;""><b>Update running...<br>Don't start new scans at this time!</b></b></span>");
        }
    }
}


void rdsOperationWindow::runStartCmds()
{
    RTI->log("Executing start commands...");

    rdsExecHelper exec;
    QString cmdLine="";

    for (int i=0; i<RTI_CONFIG->startCmds.count(); i++)
    {
        cmdLine=RTI_CONFIG->startCmds.at(i);

        if (!cmdLine.isEmpty())
        {
            if (!exec.run(cmdLine))
            {
                RTI->log("ERROR: Execution of run commmand failed '" + cmdLine + "'");

                // Indicate the error in the top icon
                if (!RTI->getWindowInstance()->isVisible())
                {
                    RTI->getWindowInstance()->iconWindow.setError();
                }
            }
        }
    }

    RTI->log("Done.");
}


void rdsOperationWindow::retryNetlogConfiguration()
{
    RTI->log("Retrying domain validation for logserver");

    bool retryAgain=false;

    if (control.getState()==rdsProcessControl::STATE_IDLE)
    {
        if (RTI_NETLOG.retryDomainValidation())
        {
            RTI->log("Domain validation succesful.");

            // Clear error in the top icon
            if (!RTI->getWindowInstance()->isVisible())
            {
                RTI->getWindowInstance()->iconWindow.clearError();
            }
        }
        else
        {
            RTI->log("Domain validation not succesful.");
            retryAgain=true;
        }
    }
    else
    {
        RTI->log("Update ongoing. Trying again later.");
        retryAgain=true;
    }

    if (retryAgain)
    {
        QTimer::singleShot(RDS_NETLOG_RETRYINTERVAL, this, SLOT(retryNetlogConfiguration()));
    }
}


void rdsOperationWindow::sendHeartbeat()
{
    heartbeatTimer.stop();

    if (control.getState()==rdsProcessControl::STATE_IDLE)
    {
        RTI_NETLOG.postEvent(EventInfo::Type::Heartbeat,EventInfo::Detail::Diagnostics,EventInfo::Severity::Success,"","");
    }

    heartbeatTimer.start();
}

