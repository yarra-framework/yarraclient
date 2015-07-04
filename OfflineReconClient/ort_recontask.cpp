#include "ort_recontask.h"
#include "ort_global.h"

ortReconTask::ortReconTask()
{
    raid=0;

    scanFile="";
    adjustmentFiles.clear();
    accNumber="";
    emailNotifier="";
    reconMode="";
    systemName="";
    taskCreationTime=QDateTime::currentDateTime();
    patientName="";
    scanProtocol="";
    reconName="";
    paramValue=0;
    requiredServerType="";
    selectedServer="";

    errorMessageUI="";
    reconTaskFailed=false;
    fileAlreadyExists=false;

    highPriority=false;

    RTI->log("Received reconstruction request (ORT client " + QString(ORT_VERSION) + ")");
}


void ortReconTask::setInstances(rdsRaid* raidinstance, ortNetwork* networkinstance)
{
    raid=raidinstance;
    network=networkinstance;
}


bool ortReconTask::exportDataFiles(int fileID, ortModeEntry* mode)
{
    if ((raid==0) || (network==0))
    {
        return false;
    }

    adjustmentFiles.clear();

    QString modeSuffix="";
    if (mode->paramDescription!="")
    {
        modeSuffix=QString::number(paramValue);
    }

    if (!raid->saveSingleFile(fileID, mode->requiresAdjScans, mode->idName, scanFile, adjustmentFiles, modeSuffix))
    {
        reconTaskFailed=true;
        RTI->log("ERROR: Export of raid data failed.");

        if (raid->ortMissingDiskspace)
        {
            errorMessageUI="Insufficient disk space available on local drive to export data.";
        }
        else
        {
            errorMessageUI="Local export of scan data failed.";
        }
        return false;
    }

    return true;
}


bool ortReconTask::transferDataFiles()
{
    if ((raid==0) || (network==0))
    {
        return false;
    }

    QFileInfo fileInfo(network->queueDir, scanFile);
    if (!fileInfo.exists())
    {
        reconTaskFailed=true;
        RTI->log("ERROR: Could not find previously exported file.");
        errorMessageUI="Exported files could not be found.";
        return false;
    }

    scanFileSize=fileInfo.size();
    if (scanFileSize==0)
    {
        reconTaskFailed=true;
        RTI->log("ERROR: Size of scan if file is not valid.");
        errorMessageUI="Size of exported files invalid.";
        return false;
    }

    // Check if file any of the files already exist in queue dir. If so, inform user.
    if (network->serverTaskDir.exists(scanFile))
    {
        fileAlreadyExists=true;
        RTI->log("NOTE: File already exists on server.");
        return false;
    }


    if (!network->transferQueueFiles())
    {
        reconTaskFailed=true;
        RTI->log("ERROR: Transfer of files to server failed.");
        errorMessageUI="Transfer to Yarra server failed.";
        return false;
    }

    return true;
}


bool ortReconTask::generateTaskFile()
{
    if ((raid==0) || (network==0))
    {
        return false;
    }

    QString taskFilename=scanFile;
    taskFilename.truncate(taskFilename.indexOf("."));
    QString lockFilename=taskFilename;
    lockFilename+=ORT_LOCK_EXTENSION;
    taskFilename+=ORT_TASK_EXTENSION;
    if (highPriority)
    {
        taskFilename+=ORT_TASK_EXTENSION_PRIO;
    }

    taskCreationTime=QDateTime::currentDateTime();

    QLockFile lockFile(network->serverTaskDir.filePath(lockFilename));

    if (!lockFile.isLocked())
    {
        // Create write lock for the task file (to avoid that the server might
        // access the file is not entirely written)
        lockFile.lock();

        // Scoping for the lock file
        {
            // Create the task file
            QSettings taskFile(network->serverTaskDir.filePath(taskFilename), QSettings::IniFormat);

            // Write the entries
            taskFile.setValue("Task/ReconMode", reconMode);
            taskFile.setValue("Task/ACC", accNumber);
            taskFile.setValue("Task/EMailNotification", emailNotifier);
            taskFile.setValue("Task/ScanFile", scanFile);
            taskFile.setValue("Task/AdjustmentFilesCount", adjustmentFiles.count());
            taskFile.setValue("Task/PatientName", patientName);
            taskFile.setValue("Task/ScanProtocol", scanProtocol);
            taskFile.setValue("Task/ReconName", reconName);
            taskFile.setValue("Task/ParamValue", paramValue);
            taskFile.setValue("Task/RequiredServerType", requiredServerType);

            taskFile.setValue("Information/SystemName", systemName);
            taskFile.setValue("Information/ScanFileSize", scanFileSize);
            taskFile.setValue("Information/TaskDate", taskCreationTime.date().toString(Qt::ISODate));
            taskFile.setValue("Information/TaskTime", taskCreationTime.time().toString(Qt::ISODate));
            taskFile.setValue("Information/SelectedServer", selectedServer);


            // Write the list of adjustment files as strings
            for (int i=0; i<adjustmentFiles.count(); i++)
            {
                taskFile.setValue("AdjustmentFiles/"+QString(i), adjustmentFiles.at(i));
            }

            // Flush the entries into the file
            taskFile.sync();

            RTI->processEvents();
        }

        // Free the lock file
        lockFile.unlock();

        RTI->log("Generated task file. Done with submission.");
    }
    else
    {
        reconTaskFailed=true;
        RTI->log("ERROR: Can't lock task file for submission.");
        errorMessageUI="Creation of task file failed.";
        return false;
    }

    return true;
}

