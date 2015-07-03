
#include <QtCore>
#include <QtWidgets>

#include "ort_global.h"
#include "../Client/rds_configuration.h"
#include "../Client/rds_runtimeinformation.h"
#include "../Client/rds_exechelper.h"
#include "../Client/rds_network.h"

#include "ort_network.h"

ortNetwork::ortNetwork()
{
    connectCmd="";
    serverPath="";
    disconnectCmd="";
    fallbackConnectCmd="";

    appPath="";
}


bool ortNetwork::prepare()
{
    // Buffer the settings from the configuration object
    connectCmd=RTI_CONFIG->ortConnectCmd;
    serverPath=RTI_CONFIG->ortServerPath;
    disconnectCmd=RTI_CONFIG->ortDisconnectCmd;
    fallbackConnectCmd=RTI_CONFIG->ortFallbackConnectCmd;
    connectTimeout=RTI_CONFIG->ortConnectTimeout;

    if (connectTimeout<1)
    {
        connectTimeout=ORT_CONNECT_TIMEOUT;
    }


    // Prepare all needed local directories
    bool error=false;

    // Change directory to application path
    appPath=qApp->applicationDirPath();
    QDir dir;
    dir.setCurrent(appPath);

    if (appPath.contains(" "))
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Invalid Installation Path");
        msgBox.setText("The Yarra client has been installed in an invalid path. The path must not contain any space characters. Please move the program to a different location.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setWindowIcon(ORT_ICON);
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
        return false;
    }

    // Check if directory for logfiles exists
    if (!dir.exists(RDS_DIR_LOG))
    {
        // If not, create it
        dir.mkdir(RDS_DIR_LOG);
    }

    // Check if it has been created successfully
    if (!dir.exists(RDS_DIR_LOG))
    {
        error=true;
    }

    // Check if directory for queued ORT files exists
    if (!dir.exists(ORT_DIR_QUEUE))
    {
        dir.mkdir(ORT_DIR_QUEUE);
    }

    // If the directory could not be created, we need to terminate
    if (!dir.exists(ORT_DIR_QUEUE))
    {
        error=true;
    }

    // Show error message and terminate the client
    if (error)
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Error");
        msgBox.setText("Unable to create required directories. Is the Yarra client running with sufficient user rights?");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setWindowIcon(ORT_ICON);
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
    }

    dir.cd(ORT_DIR_QUEUE);
    queueDir=dir;
    queueDir.setFilter(QDir::Files); // Important to exclude directories from list

    QStringList queuelist=dir.entryList(QDir::Files);
    if (queuelist.count()>0)
    {
        RTI->log("Warning: recoqueue directory was not empty during startup");
        cleanLocalQueueDir();
    }

    return (!error);
}


void ortNetwork::cleanLocalQueueDir()
{
    queueDir.refresh();
    QStringList queuelist=queueDir.entryList(QDir::Files);
    if (queuelist.count()>0)
    {
        for (int i=0; i<queuelist.count(); i++)
        {
            if (!queueDir.remove(queuelist.at(i)))
            {
                RTI->log("WARNING: File could not be deleted from local recoqueue " + queuelist.at(i));
            }
        }
    }
}


bool ortNetwork::openConnection(bool fallback)
{
    if ((fallback) && (fallbackConnectCmd.length()==0))
    {
        // The fallback mode requires that a connection command
        // has been defined.
        return false;
    }

    // Execute the network connect command
    if (connectCmd!="")
    {
        rdsExecHelper execHelper;

        // Connect to the main server. In the second try, connect
        // to the fallback server
        if (fallback)
        {
            execHelper.setCommand(fallbackConnectCmd);
        }
        else
        {
            execHelper.setCommand(connectCmd);
        }

        if (!execHelper.callProcessTimout(connectTimeout))
        {
            RTI->log("Calling the connect command failed: " + connectCmd);
        }
    }

    QString errorMessage="";
    bool error=false;

    if (serverPath=="")
    {
        RTI->log("ERROR: Server path has not been defined.");

        errorMessage="The Yarra server path has not been defined.\nPlease check the configuration.";
        error=true;
    }

    if ((!error) && (!serverDir.exists(serverPath)))
    {
        RTI->log("ERROR: Could not access Yarra server path: " + serverPath);
        RTI->log("ERROR: Check if network drive has been mounted correctly. Or check configuration.");

        errorMessage="Could not access the path of the Yarra server.\nPlease make sure that the network drive can be \nmapped and that the configuration is valid.";
        error=true;
    }

    if ((!error) && (!serverDir.cd(serverPath)))
    {
        RTI->log("ERROR: Could not change to base path of network drive.");
        RTI->setSevereErrors(true);

        errorMessage="Could not enter the Yarra server directory.\nPlease make sure that the network drive can be\nmapped and that the configuration is valid.";

        error=true;
    }

    if ((!error) && ((!serverDir.exists(ORT_MODEFILE)) || (!serverDir.exists(ORT_SERVERFILE))))
    {
        RTI->log("ERROR: ORT mode or server file not found.");
        RTI->setSevereErrors(true);

        errorMessage="Could not find the Yarra configuration files (YarraModes.cfg / YarraServer.cfg).\n\nPlease verify the configuration of the Yarra client and server.";
        error=true;
    }

    if (!error)
    {
        // Try to synchronize local copy of server list (to use it as fallback
        // solution in the future)
        if (serverList.syncServerList(serverPath))
        {
            // If copying the server list was succesul, read the local
            // copy of the list into memory.
            serverList.readLocalServerList();
        }
    }

    // Files should be stored in the server root share. A subdir is not used anymore.
    serverTaskDir=serverDir;

    // Show error message and terminate the client
    if (error)
    {
        // If no fallback connection has been defined or if this
        // is already the fallback try, then show an error message.
        if ((fallbackConnectCmd.length()==0) || (fallback))
        {
            QMessageBox msgBox;
            msgBox.setWindowTitle("Error");
            msgBox.setText(errorMessage);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setWindowIcon(ORT_ICON);
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.exec();
        }
    }

    return (!error);
}


void ortNetwork::closeConnection()
{
    if (disconnectCmd!="")
    {
        rdsExecHelper execHelper;
        execHelper.setCommand(disconnectCmd);

        if (!execHelper.callProcessTimout(connectTimeout))
        {
            RTI->log("Calling the disconnect command failed: " + disconnectCmd);
        }
    }
}


bool ortNetwork::transferQueueFiles()
{
    // Read files in queue directory
    queueDir.refresh();
    fileList=queueDir.entryList();

    bool success=true;
    for (int i=0; i<fileList.count(); i++)
    {
        success=getFileToProcess(i);

        // Try to copy file
        if (success)
        {
            success=copyFile();
        }

        // If sucessfull, verify if copied correctly
        if (success)
        {
            success=verifyTransfer();
        }

        // If sucessfull, remove file from queue directory
        if (success)
        {
            success=removeFile();
        }

        releaseFile();

        if (!success)
        {
            return false;
        }
    }

    fileList.clear();

    return true;
}


bool ortNetwork::getFileToProcess(int index)
{
    if ((index<0) || (index>=fileList.count()))
    {
        RTI->log("ERROR: File transfer list is invalid!");
        return false;
    }

    currentFilename=fileList.at(index);
    RTI->log("Transferring file " + currentFilename);

    if (!queueDir.exists(currentFilename))
    {
        RTI->log("ERROR: File could not be found: " + currentFilename);
        return false;
    }

    QFileInfo fileInfo(queueDir, currentFilename);
    currentFilesize=fileInfo.size();

    return true;
}


bool ortNetwork::copyFile()
{
    QString sourceName=queueDir.absoluteFilePath(currentFilename);
    QString destName=serverTaskDir.absolutePath() + "/" + currentFilename;

    // Check free diskspace on the network drive
    QFileInfo srcinfo(sourceName);
    qint64 diskSpace=RTI->getFreeDiskSpace(serverTaskDir.absolutePath());
    RTI->debug("Disk space on netdrive = " + QString::number(diskSpace));
    RTI->debug("File size is = " +  QString::number(srcinfo.size()));

    if (srcinfo.size() > diskSpace)
    {
        RTI->log("ERROR: Not enough diskspace on network drive to transfer file.");
        RTI->log("ERROR: File size = " + QString::number(srcinfo.size()));
        RTI->log("ERROR: Remaining disk space = " + QString::number(diskSpace));
        RTI->log("Canceling transfer.");
        RTI->setSevereErrors(true);
        return false;
    }

    if (QFile::exists(destName))
    {
        RTI->log("WARNING: File already exists in target folder: " + destName);
        RTI->log("Skipping transfer.");
        return true;
    }

    bool copyError=false;

    // Use separate copy thread and local event loop to keep the application
    // responsive while the data is transferred. This is very important because
    // otherwise problems might arise if the qsingleapplication interface
    // receives messages while it was blocked (might lead to infinite loops)
    {
        rdsCopyThread copyThread;
        copyThread.sourceName=sourceName;
        copyThread.destName=destName;

        QEventLoop q;
        connect(&copyThread, SIGNAL(finished()), &q, SLOT(quit()));

        QTime ti;
        ti.start();

        copyThread.start();
        q.exec();

        // Check if the event loop returned too early.
        if (!copyThread.finishedCopy)
        {
            RTI->log("WARNING: QEventLoop returned too early during copy. Starting secondary loop.");

            // Wait until copying has finished, but restrict the waiting to 1 hour max
            while ((!copyThread.isFinished()) && (ti.elapsed()<RDS_COPY_TIMEOUT))
            {
                RTI->processEvents();
                Sleep(RDS_SLEEP_INTERVAL);
            }

            if (!copyThread.finishedCopy)
            {
                RTI->log("ERROR: Second copy wait loop timed out! Copying file failed.");
                copyError=true;
            }
        }

        copyError=!copyThread.success;
    }

    if (copyError)
    {
        RTI->log("ERROR: Error copying the file!");
        RTI->log("ERROR: Source = " + sourceName);
        RTI->log("ERROR: Destination = " + destName);
        RTI->setSevereErrors(true);
        return false;
    }

    return true;
}


bool ortNetwork::verifyTransfer()
{

    QString destName=serverTaskDir.absolutePath() + "/" + currentFilename;
    QFileInfo fileInfo(destName);

    if (!fileInfo.exists())
    {
        RTI->log("ERROR: Copied file does not exist at target position: " + destName);
        RTI->log("ERROR: Transferring the file was not successful.");
        RTI->setSevereErrors(true);
        return false;
    }

    if (fileInfo.size() != currentFilesize)
    {
        RTI->log("ERROR: File size of copied file does not match!");
        RTI->log("ERROR: Original size = " + QString::number(currentFilesize));
        RTI->log("ERROR: Copy size = " + QString::number(fileInfo.size()));
        RTI->setSevereErrors(true);
        return false;
    }

    RTI->log("File transfer successful.");
    return true;
}


bool ortNetwork::removeFile()
{
    RDS_RETONERR_LOG( queueDir.exists(currentFilename), "ERROR: File for deletion could not be found: " + currentFilename );

    if (!queueDir.remove(currentFilename))
    {
        RTI->log("Couldn't delete file " + currentFilename + ", retrying.");
        Sleep(DWORD(100));
        if (!queueDir.remove(currentFilename))
        {
            RTI->log("ERROR: Deleting file was not successful. File might be locked: " + currentFilename);
        }
    }

    return true;
}


void ortNetwork::releaseFile()
{
    currentFilename="";
    currentFilesize=-1;
}

