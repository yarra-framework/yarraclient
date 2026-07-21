#include "rds_configurationwindow.h"
#include "ui_rds_configurationwindow.h"

#include <QtWidgets>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QTcpSocket>

#include "rds_global.h"
#include <../NetLogger/netlogger.h>
#include "rds_network.h"


rdsConfigurationWindow::rdsConfigurationWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::rdsConfigurationWindow)
{
    ui->setupUi(this);
    setWindowIcon(RDS_ICON);
    setWindowTitle("Yarra - RDS Configuration");

    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::MSWindowsFixedSizeDialogHint;
    flags &= ~Qt::WindowContextHelpButtonHint;
    setWindowFlags(flags);

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(saveSettings()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(close()));

    connect(ui->updateCombobox, SIGNAL(currentIndexChanged(int)), this, SLOT(callUpdateModeChanged(int)));

    connect(ui->protAddButton, SIGNAL(clicked()), this, SLOT(callAddProt()));
    connect(ui->protRemoveButton, SIGNAL(clicked()), this, SLOT(callRemoveProt()));
    //connect(ui->protListWidget, SIGNAL(activated()), this, SLOT(callShowProt()));

    connect(ui->logServerTestConnectionButton, SIGNAL(clicked()), this, SLOT(callLogServerTestConnection()));

    ui->tabWidget->setCurrentIndex(0);
    callUpdateModeChanged(ui->updateCombobox->currentIndex());

    ui->networkModeCombobox->setCurrentIndex(0);
    ui->networkStackedWidget->setCurrentIndex(0);

    QString versionText="Version " + QString(RDS_VERSION) + ", Build date " + QString(__DATE__);
#ifdef NETLOGGER_DISABLE_DOMAIN_VALIDATION
    versionText += ", Domain Validation OFF";
#endif
    ui->versionLabel->setText(versionText);

    // Center the window on the screen
    setGeometry(QStyle::alignedRect(Qt::LeftToRight,Qt::AlignCenter,size(),
                                    qApp->desktop()->availableGeometry()));

    readConfiguration();

    ui->logServerStatusLabel->setText("");
}


rdsConfigurationWindow::~rdsConfigurationWindow()
{
    delete ui;
}


void rdsConfigurationWindow::closeEvent(QCloseEvent *event)
{
    RTI->setMode(rdsRuntimeInformation::RDS_OPERATION);
    qApp->quit();
}


bool rdsConfigurationWindow::checkAccessPassword()
{
    // Skip the password prompt entirely in simulation mode, since there's no
    // real scanner/data to protect and it just slows down testing.
    if (RTI->isSimulation())
    {
        return true;
    }

    // Password check to prevent unauthorized users to change
    // the settings
    QInputDialog pwdDialog;
    pwdDialog.setInputMode(QInputDialog::TextInput);
    pwdDialog.setWindowIcon(RDS_ICON);
    pwdDialog.setWindowTitle("Password");
    pwdDialog.setLabelText("Configuration Password:");
    pwdDialog.setTextEchoMode(QLineEdit::Password);

    Qt::WindowFlags flags = pwdDialog.windowFlags();
    flags |= Qt::MSWindowsFixedSizeDialogHint;
    flags &= ~Qt::WindowContextHelpButtonHint;
    pwdDialog.setWindowFlags(flags);

    if (pwdDialog.exec()==QDialog::Rejected)
    {
        return false;
    }
    else
    {
        return (pwdDialog.textValue().toLower()==QString(RDS_PASSWORD).toLower());
    }
}


void rdsConfigurationWindow::saveSettings()
{
    storeConfiguration();
    close();
}


void rdsConfigurationWindow::callUpdateModeChanged(int index)
{
    int stackPage=0;

    switch (index)
    {
    case rdsConfiguration::UPDATEMODE_STARTUP:
    case rdsConfiguration::UPDATEMODE_MANUAL:
    default:
        stackPage=0;
        break;

    case rdsConfiguration::UPDATEMODE_FIXEDTIME:
    case rdsConfiguration::UPDATEMODE_STARTUP_FIXEDTIME:
        stackPage=1;
        break;

    case rdsConfiguration::UPDATEMODE_PERIODIC:
        stackPage=2;
        break;
    }

    ui->updateStackedWidget->setCurrentIndex(stackPage);
}


void rdsConfigurationWindow::readConfiguration()
{
    ui->softwareVersionEdit->setText(RTI->getSyngoMRVersionString());
    ui->serialNumberEdit->setText(config.infoSerialNumber);

    config.loadConfiguration();

    // Transfer configuration to UI controls
    ui->systemNameEdit->setText(config.infoName);
    ui->indicatorCheckbox->setChecked(config.infoShowIcon);

    ui->time1Edit->setTime(config.infoUpdateTime1);
    ui->time2Edit->setTime(config.infoUpdateTime2);
    ui->time3Edit->setTime(config.infoUpdateTime3);

    ui->time2Checkbox->setChecked(config.infoUpdateUseTime2);
    ui->time3Checkbox->setChecked(config.infoUpdateUseTime3);

    ui->jitterTimesCheckbox->setChecked(config.infoJitterTimes);
    ui->jitterSpinbox->setValue(config.infoJitterWindow);

    //NOTE: Currently, the software only supports the network drive mode
    ui->networkModeCombobox->setCurrentIndex(0);

    ui->networkDrivePathEdit->setText(config.netDriveBasepath);
    ui->networkDriveReconnectCmd->setText(config.netDriveReconnectCmd);
    ui->networkDriveDisconnectCmd->setText(config.netDriveDisconnectCmd);    
    ui->networkDriveCreatePath->setChecked(config.netDriveCreateBasepath);
    ui->networkRemoteConfigLabelEdit->setText(config.netRemoteConfigFile);
    ui->networkRerunStartupCmdsCheckbox->setChecked(config.netDriveStartupCmdsAfterFail);
    ui->networkBufferPathEdit->setText(config.netDriveLocalBufferPath);
    ui->networkAlternatingDiskLimitSpinbox->setValue(config.netAlternatingDiskLimitGb);
    ui->networkWarningDiskLimitSpinbox->setValue(config.netWarningDiskLimitGb);

    ui->updateCombobox->setCurrentIndex(config.infoUpdateMode);
    ui->updatePeriodCombobox->setCurrentIndex(config.infoUpdatePeriodUnit);
    ui->updatePeriodSpinbox->setValue(config.infoUpdatePeriod);

    ui->logServerPathEdit->setText(config.logServerPath);
    ui->logServerApiKeyEdit->setText(config.logApiKey);
    ui->logServerSendHeartbeatsCheckbox->setChecked(config.logSendHeartbeat);
    ui->logServerSendScansCheckbox->setChecked(config.logSendScanInfo);
    ui->logServerPushFrequencySpinbox->setValue(config.logUpdateFrequency);
    ui->logServerPushFrequencyUnitCombobox->setCurrentIndex(config.logUpdateFrequencyUnit);

    ui->startCmdEdit->clear();
    ui->startCmdEdit->setPlainText(config.startCmds.join('\n'));

    updateProtocolList();

    if (config.getProtocolCount()>0)
    {
        ui->protListWidget->setCurrentRow(0);
    }
    callShowProt();
}


void rdsConfigurationWindow::storeConfiguration()
{
    config.infoName=ui->systemNameEdit->text();
    config.infoShowIcon=ui->indicatorCheckbox->isChecked();

    config.infoUpdateTime1=ui->time1Edit->time();
    config.infoUpdateTime2=ui->time2Edit->time();
    config.infoUpdateTime3=ui->time3Edit->time();

    config.infoUpdateUseTime2=ui->time2Checkbox->isChecked();
    config.infoUpdateUseTime3=ui->time3Checkbox->isChecked();

    config.infoJitterTimes=ui->jitterTimesCheckbox->isChecked();
    config.infoJitterWindow=ui->jitterSpinbox->value();

    config.netMode=ui->networkModeCombobox->currentIndex();
    config.netDriveBasepath=ui->networkDrivePathEdit->text();
    config.netDriveReconnectCmd=ui->networkDriveReconnectCmd->text();
    config.netDriveDisconnectCmd=ui->networkDriveDisconnectCmd->text();
    config.netDriveCreateBasepath=ui->networkDriveCreatePath->isChecked();
    config.netRemoteConfigFile=ui->networkRemoteConfigLabelEdit->text();
    config.netDriveStartupCmdsAfterFail=ui->networkRerunStartupCmdsCheckbox->isChecked();
    config.netDriveLocalBufferPath=ui->networkBufferPathEdit->text();
    config.netAlternatingDiskLimitGb=ui->networkAlternatingDiskLimitSpinbox->value();
    config.netWarningDiskLimitGb=ui->networkWarningDiskLimitSpinbox->value();

    config.logServerPath=ui->logServerPathEdit->text();
    config.logApiKey=ui->logServerApiKeyEdit->text();
    config.logSendHeartbeat=ui->logServerSendHeartbeatsCheckbox->isChecked();
    config.logSendScanInfo=ui->logServerSendScansCheckbox->isChecked();
    config.logUpdateFrequency=ui->logServerPushFrequencySpinbox->value();
    config.logUpdateFrequencyUnit=ui->logServerPushFrequencyUnitCombobox->currentIndex();

    config.infoUpdateMode=ui->updateCombobox->currentIndex();
    config.infoUpdatePeriodUnit=ui->updatePeriodCombobox->currentIndex();
    config.infoUpdatePeriod=ui->updatePeriodSpinbox->value();

    config.startCmds=ui->startCmdEdit->toPlainText().split('\n');       

    // Write the configuration to the ini file
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    config.saveConfiguration();  
    QApplication::restoreOverrideCursor();
}


void rdsConfigurationWindow::updateProtocolList()
{
    ui->protListWidget->clear();

    for (int i=0; i<config.getProtocolCount(); i++)
    {
        QString name="";
        QString filter="";
        bool saveAdjustData=false;
        bool anonymizeData=false;
        bool smallFiles=false;
        bool remotelyDefined=false;

        config.readProtocol(i, name, filter, saveAdjustData, anonymizeData, smallFiles, remotelyDefined);
        ui->protListWidget->addItem(name);
    }

    bool uiControlsEnabled=true;
    if (config.getProtocolCount()==0)
    {
        uiControlsEnabled=false;
    }

    ui->protListWidget->setEnabled(uiControlsEnabled);
    ui->protNameEdit->setEnabled(uiControlsEnabled);
    ui->protFilterEdit->setEnabled(uiControlsEnabled);
    ui->protAdjustCheckbox->setEnabled(uiControlsEnabled);
    ui->protAnonymizeCheckbox->setEnabled(uiControlsEnabled);
    ui->protNameLabel->setEnabled(uiControlsEnabled);
    ui->protFilterLabel->setEnabled(uiControlsEnabled);
    ui->protSmallFilesCheckbox->setEnabled(uiControlsEnabled);
}


void rdsConfigurationWindow::callAddProt()
{
    config.addProtocol("Untitled", "_RDS", false, false, false, false);
    updateProtocolList();
    ui->protListWidget->setCurrentRow(ui->protListWidget->count()-1);
}


void rdsConfigurationWindow::callRemoveProt()
{
    int itemToDelete=ui->protListWidget->currentRow();

    if (itemToDelete!=-1)
    {
        config.deleteProtocol(itemToDelete);
        updateProtocolList();
    }
}


void rdsConfigurationWindow::callUpdateProt()
{
    int index=ui->protListWidget->currentRow();

    if ((index!=-1) && (index<config.getProtocolCount()))
    {
        config.updateProtocol(index, ui->protNameEdit->text(), ui->protFilterEdit->text(), ui->protAdjustCheckbox->isChecked(),
                              ui->protAnonymizeCheckbox->isChecked(), ui->protSmallFilesCheckbox->isChecked(), false);
        ui->protListWidget->item(index)->setText(ui->protNameEdit->text());
    }

    //updateProtocolList();
}


void rdsConfigurationWindow::callShowProt()
{
    int index=ui->protListWidget->currentRow();

    if ((index!=-1) && (index<config.getProtocolCount()))
    {
        QString name="";
        QString filter="";
        bool saveAdjustData=false;
        bool anonymizeData=false;
        bool smallFiles=false;
        bool remotelyDefined=false;

        config.readProtocol(index, name, filter, saveAdjustData, anonymizeData, smallFiles, remotelyDefined);

        ui->protNameEdit->setText(name);
        ui->protFilterEdit->setText(filter);
        ui->protAdjustCheckbox->setChecked(saveAdjustData);
        ui->protAnonymizeCheckbox->setChecked(anonymizeData);
        ui->protSmallFilesCheckbox->setChecked(smallFiles);
    }
}


void rdsConfigurationWindow::on_protListWidget_currentRowChanged(int currentRow)
{
    callShowProt();
}


void rdsConfigurationWindow::on_protNameEdit_textEdited(const QString &arg1)
{
    callUpdateProt();
}


void rdsConfigurationWindow::on_protFilterEdit_textEdited(const QString &arg1)
{
    callUpdateProt();
}


void rdsConfigurationWindow::on_protAdjustCheckbox_toggled(bool checked)
{
    callUpdateProt();
}


void rdsConfigurationWindow::on_protAnonymizeCheckbox_toggled(bool checked)
{
    callUpdateProt();
}


void rdsConfigurationWindow::on_networkModeCombobox_currentIndexChanged(int index)
{
  ui->networkStackedWidget->setCurrentIndex(index);
}


void rdsConfigurationWindow::on_networkFilePathButton_clicked()
{
    QString newPath=ui->networkDrivePathEdit->text();
    newPath=QFileDialog::getExistingDirectory(this, "Select Base Path", newPath);
    if (newPath!="")
    {
        ui->networkDrivePathEdit->setText(newPath);

        QMessageBox msgBox;
        msgBox.setWindowIcon(RDS_ICON);
        msgBox.setWindowTitle("Network Drive Path");
        msgBox.setText("<b>NOTE:</b> Ensure that the network drive will be <b>reconnected automatically</b> after restart of the system. "\
                       "Otherwise transfering the raw data will not be possible.");
        msgBox.exec();
    }
}


void rdsConfigurationWindow::on_networkRemoteConfigurationButton_clicked()
{
    QString newFile=QFileDialog::getOpenFileName(this,"Select Remote Configuration File","","*.ini");

    if (!newFile.isNull())
    {
        ui->networkRemoteConfigLabelEdit->setText(newFile);
    }
}


void rdsConfigurationWindow::on_protSmallFilesCheckbox_toggled(bool checked)
{
    callUpdateProt();
}


void rdsConfigurationWindow::callLogServerTestConnection()
{
    // Reset previous output
    ui->logServerStatusLabel->setText("Testing connection...");
    RTI->processEvents();

    const QString errorPrefix="<span style=""color:#E5554F;""><strong>ERROR:</strong></span>&nbsp;&nbsp;";

    QString output="";
    bool error=false;

    QString serverPath=ui->logServerPathEdit->text();
    int serverPort=8080;

    if (serverPath.isEmpty())
    {
        output=errorPrefix + "No server address entered.";
        ui->logServerStatusLabel->setText(output);

        return;
    }

    // Check if the entered address contains a port specifier
    int colonPos=serverPath.indexOf(":");

    if (colonPos!=-1)
    {
        int cutChars=serverPath.length()-colonPos;

        QString portString=serverPath.right(cutChars-1);
        serverPath.chop(cutChars);

        // Convert port string into number and validate
        bool portValid=false;

        int tempPort=portString.toInt(&portValid);

        if (portValid)
        {
            serverPort=tempPort;
        }
    }

    QString localHostname="";
    QString localIP="";

    // Open socket connection to see if the server is active and to
    // determine the local IP address used for routing to the server.
    QTcpSocket socket;
    socket.connectToHost(serverPath, serverPort);

    if (socket.waitForConnected(10000))
    {
        localIP=socket.localAddress().toString();
    }
    else
    {
        output += errorPrefix + "Unable to connect to server.";
        error=true;
    }

    socket.disconnectFromHost();

#ifndef NETLOGGER_DISABLE_DOMAIN_VALIDATION

    // Lookup the hostname of the local client from the DNS server
    if (!error)
    {
        localHostname=NetLogger::dnsLookup(localIP);

        if (localHostname.isEmpty())
        {
            output += errorPrefix + "Unable to resolve local hostname.<br /><br />IP = " + localIP;
            output += "<br />Check local DNS server settings.";
            error=true;
        }
    }

    // Lookup the hostname of the log server from the DNS server
    if (!error)
    {
        serverPath=NetLogger::dnsLookup(serverPath);

        if (serverPath.isEmpty())
        {
            output += errorPrefix + "Unable to resolve server name.<br /><br />";
            output += "Check local DNS server settings.";
            error=true;
        }
    }

    // Compare if the local system and the server are on the same domain
    if (!error)
    {
        QStringList localHostString =localHostname.toLower().split(".");
        QStringList serverHostString=serverPath.toLower().split(".");

        if ((localHostString.count()<2) || (serverHostString.count()<2))
        {
            output += errorPrefix + "Error resolving hostnames.<br /><br />";
            output += "Local host: " + localHostname.toLower() + " ("+localIP+")<br />";
            output += "Log server: " + serverHostString.join(".");
            error=true;
        }
        else
        {
            if ((localHostString.at(localHostString.count()-1)!=(serverHostString.at(serverHostString.count()-1)))
               || (localHostString.at(localHostString.count()-2)!=(serverHostString.at(serverHostString.count()-2))))
            {
                output += errorPrefix + "Server not in local domain.<br /><br />";
                output += "Local host: " + localHostname.toLower() + "<br />";
                output += "Log server: " + serverPath.toLower() + "<br />";
                error=true;
            }
        }
    }

#endif

    // Check if the server responds to the test entry point
    if (!error)
    {        
        QUrlQuery data;
        QNetworkReply::NetworkError net_error;
        int http_status=0;        

        // Create temporary instance for testing the logserver connection
        NetLogger testLogger;

        // Read the full server path entered into the UI control again. The path returned from the DNS
        // might differ if an DNS alias is used. In this case, the certificate will be incorrect.
        serverPath         =ui->logServerPathEdit->text();
        QString apiKey     =ui->logServerApiKeyEdit->text();
        QString name       ="ConnectionTest";
        QString errorString="n/a";

        testLogger.configure(serverPath, EventInfo::SourceType::RDS, name, apiKey, true);
        data.addQueryItem("api_key",apiKey);
        bool success=testLogger.postData(data, NETLOG_ENDPT_TEST, net_error, http_status, errorString);

        if (!success)
        {
            if (http_status==0)
            {
                output += errorPrefix + "No response from server (Error: "+errorString+").<br /><br />";

                switch (net_error)
                {
                case QNetworkReply::NetworkError::SslHandshakeFailedError:
                    output += "Is SSL certificate installed / valid?";
                    break;
                case QNetworkReply::NetworkError::AuthenticationRequiredError:
                    output += "Is API key correct?";
                    break;
                default:
                    output += "Is server running? Is port correct?";
                    break;
                }
            }
            else
            {
                output += errorPrefix + "Server rejected request (Status: "+QString::number(http_status)+").<br /><br />Is API key corrent?";
            }

            error=true;
        }
    }

    if (!error)
    {
        output="<span style=""color:#40C1AC;""><strong>Success.</strong></span>";
    }

    ui->logServerStatusLabel->setText(output);
}


void rdsConfigurationWindow::on_networkBufferPathButton_clicked()
{
    QString newPath=ui->networkBufferPathEdit->text();
    newPath=QFileDialog::getExistingDirectory(this, "Select Local Buffer Path", newPath);
    if (newPath!="")
    {
        ui->networkBufferPathEdit->setText(newPath);

        QMessageBox msgBox;
        msgBox.setWindowIcon(RDS_ICON);
        msgBox.setWindowTitle("Local Buffer Path");
        msgBox.setText("<b>NOTE:</b> The local buffer path should only be modified in specific cases. <br /><br />"\
                       "Ensure that the local buffer path is always available (e.g., if using a USB drive). "\
                       "Otherwise exporting the raw data will not be possible.");
        msgBox.exec();
    }
}


void rdsConfigurationWindow::on_networkBufferPathClearButton_clicked()
{
    ui->networkBufferPathEdit->clear();
}

void rdsConfigurationWindow::on_networkRemoteConfigResetButton_clicked()
{
    ui->networkRemoteConfigLabelEdit->clear();
}
