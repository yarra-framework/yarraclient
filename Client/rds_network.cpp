#if defined(Q_OS_WIN)
    #include <QtCore/QLibrary>
    #include <QtCore/qt_windows.h>
#endif
#if defined(Q_OS_UNIX)
    #include <time.h>
#endif

#include "rds_network.h"
#include "rds_global.h"
#include "rds_exechelper.h"

#include <QElapsedTimer>

#ifdef YARRA_APP_RDS
    #include "rds_checksum.h"
    #include "rds_copydialog.h"
    #include "rds_processcontrol.h"
#endif


rdsNetwork::rdsNetwork()
{
    queueDir.setCurrent(RTI->getAppPath() + "/" + RDS_DIR_QUEUE);
    queueDir.setFilter(QDir::Files);
    currentFilename ="";
    currentProt     ="";
    currentFilesize =-1;
    currentTimeStamp="";

    connectionActive=false;

#ifdef YARRA_APP_RDS
    copyDialog=0;

    // Initial assumption for the copy dialog's progress estimate, replaced
    // outright by the first real measurement (see copyFile()) and refined
    // from there on.
    estimatedBytesPerSec=20*1024*1024; // 20 MB/s
    hasMeasuredThroughput=false;

    transferTotalBytes=0;
    transferBytesDone=0;
    transferFilesDone=0;

    overallTransferActive=false;
    overallScansDone=0;
    overallScansTotal=0;

    phaseScansBefore=0;
    phaseScansThisCycle=0;
    phaseScansTotal=1;
#endif
}


rdsNetwork::~rdsNetwork()
{
}


#ifdef YARRA_APP_RDS
void rdsNetwork::beginOverallTransfer()
{
    overallTransferActive=true;

    transferTotalBytes=0;
    transferBytesDone=0;
    transferFilesDone=0;

    overallScansDone=0;
    overallScansTotal=0;

    phaseScansBefore=0;
    phaseScansThisCycle=0;
    phaseScansTotal=1;

    // Own the dialog for the whole loop instead of letting transferFiles()
    // flicker it closed and reopened every cycle. Every network phase during
    // an overall-tracked loop uses STATE_NETWORKTRANSFER_ALTERNATING, so
    // scanning is unsafe for its entire duration - no need to recheck this
    // per cycle the way a standalone transfer does.
    copyDialog=new rdsCopyDialog();
    copyDialog->setScanningAllowed(false);
    copyDialog->show();
}


void rdsNetwork::endOverallTransfer()
{
    overallTransferActive=false;

    if (copyDialog!=0)
    {
        copyDialog->setProgress(100);
        copyDialog->close();
        RDS_FREE(copyDialog);
    }
}


void rdsNetwork::setScanPhase(int scansBeforeThisCycle, int scansThisCycle, int totalScans)
{
    phaseScansBefore=scansBeforeThisCycle;
    phaseScansThisCycle=scansThisCycle;
    phaseScansTotal=totalScans>0 ? totalScans : 1;

    // Show where the overall bar stands as of the start of this cycle (0%
    // progress within it) immediately, rather than waiting for the first
    // byte to actually copy.
    if (copyDialog!=0)
    {
        copyDialog->setProgress(computeDisplayPercent(0, 1));
    }
}


void rdsNetwork::setScanningAllowed(bool allowed)
{
    if (copyDialog!=0)
    {
        copyDialog->setScanningAllowed(allowed);
    }
}


void rdsNetwork::setScanProgress(int scansDone, int totalScans)
{
    overallScansDone=scansDone;
    overallScansTotal=totalScans;

    if (copyDialog!=0)
    {
        copyDialog->setProgressCount(overallScansDone, overallScansTotal);
    }
}


int rdsNetwork::computeDisplayPercent(qint64 bytesDone, qint64 totalBytes)
{
    double localFraction=(totalBytes>0) ? (double(bytesDone)/double(totalBytes)) : 1.0;

    if (overallTransferActive)
    {
        // Assume each scan takes an equal share of the overall bar (no
        // reliable total byte count exists - see setScanPhase()), and use
        // the current cycle's own byte progress just to animate smoothly
        // within its slice.
        double overallFraction=(phaseScansBefore + phaseScansThisCycle*localFraction) / double(phaseScansTotal);
        return int(qMin(99.0, overallFraction*100.0));
    }
    else
    {
        return int(qMin(99.0, localFraction*100.0));
    }
}
#endif


bool rdsNetwork::setLocalBufferPath(QString bufferPath)
{
    if (bufferPath.isEmpty())
    {
        return true;
    }

    if (!queueDir.cd(bufferPath))
    {
        RTI->log("ERROR: Selected LocalBufferPath does not exist");
        RTI_NETLOG.postEvent(EventInfo::Type::Boot, EventInfo::Detail::Start, EventInfo::Severity::Error, "Selected LocalBufferPath does not exist");
        return false;
    }

    return true;
}


bool rdsNetwork::isLocalBufferPathValid()
{
    return queueDir.exists();
}


bool rdsNetwork::openConnection()
{
    queueDir.refresh();

    // Open connection to network drive
    if (RTI_CONFIG->isNetworkModeDrive())
    {
        // Call reconnect cmd entered into configuration
        runReconnectCmd();

        // Test if network path exists
        if (!networkDrive.exists(RTI_CONFIG->netDriveBasepath))
        {
            // If the network base path should not be created (default), abort with error message
            if (!RTI_CONFIG->netDriveCreateBasepath)
            {
                RTI->log("Error: Could not access network drive path: " + RTI_CONFIG->netDriveBasepath);
                RTI->log("Error: Check if network drive has been mounted. Or check configuration.");
                RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::Diagnostics,EventInfo::Severity::FatalError,
                             "Error: Could not access network drive path", RTI_CONFIG->netDriveBasepath);
                return false;
            }
            else
            {
                // If the option is enabled, try to create it
                if (!networkDrive.mkpath(RTI_CONFIG->netDriveBasepath))
                {
                    RTI->log("Error: Unable to create network drive path: " + RTI_CONFIG->netDriveBasepath);
                    RTI->log("Error: Check configuration and network permissions.");
                    RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::FileTransfer,EventInfo::Severity::FatalError,
                                 "Error: Unable to create network drive path", RTI_CONFIG->netDriveBasepath);

                    return false;
                }
                else
                {
                    if (!networkDrive.exists(RTI_CONFIG->netDriveBasepath))
                    {
                        RTI->log("Error: Creating the network drive path was not successful: " + RTI_CONFIG->netDriveBasepath);
                        RTI->log("Error: Check configuration and network permissions.");
                        RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::Diagnostics,EventInfo::Severity::FatalError,
                                     "Error: Creating the network drive path was not successful", RTI_CONFIG->netDriveBasepath);

                        return false;
                    }
                }
            }
        }

        // Change to network drive
        RTI->debug("Network base path is :" + RTI_CONFIG->netDriveBasepath);
        if (!networkDrive.cd(RTI_CONFIG->netDriveBasepath))
        {
            RTI->log("ERROR: Could not change to base path of network drive.");
            RTI->setSevereErrors(true);
            RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::Diagnostics,EventInfo::Severity::FatalError,
                         "Could not change to base path of network drive");
            return false;
        }

        // Look if the folder for the system exists, if not create it
        if (!networkDrive.exists(RTI_CONFIG->infoName))
        {
            RTI->debug("Target folder does not exist. Creating it: " + RTI_CONFIG->infoName);

            if (!networkDrive.mkdir(RTI_CONFIG->infoName))
            {
                RTI->log("Error: Error creating system folder on network drive.");
                RTI->log("Error: Rights for writing and creating files might not exist.");
                RTI->log("Error: Check configuration of network drive.");
                RTI->setSevereErrors(true);
                RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::Diagnostics,EventInfo::Severity::FatalError,
                             "Error creating system folder on network drive");
                return false;
            }
        }

        networkDrive.cd(RTI_CONFIG->infoName);
        RTI->debug("Selected storage path is :" + networkDrive.absolutePath());
    }

    connectionActive=true;
    return true;
}


void rdsNetwork::closeConnection()
{  
    runDisconnectCmd();
    connectionActive=false;
}


bool rdsNetwork::transferFiles()
{
#ifdef YARRA_APP_RDS
    // During an overall-tracked (alternating/batched) loop, beginOverallTransfer()
    // already created and is showing a dialog that stays open for the whole
    // loop, instead of flickering closed and reopened every cycle - so only
    // create/show one here for a standalone (non-overall-tracked) transfer.
    if (!overallTransferActive)
    {
        copyDialog=new rdsCopyDialog();

        // Alternating mode interleaves RAID exports with per-file network
        // transfers for the whole update, so it isn't safe to start a new scan
        // during any part of it - not just while actually pulling data off the
        // RAID. Match the same state check the operation window's status text
        // uses (rds_operationwindow.cpp), so the two never contradict each other.
        copyDialog->setScanningAllowed(RTI_CONTROL->getState()!=rdsProcessControl::STATE_NETWORKTRANSFER_ALTERNATING);

        copyDialog->show();
    }
#endif

    // Calling getQueueCount will also refresh the queue directory list.
    if (getQueueCount()==0)
    {
#ifdef YARRA_APP_RDS
        if (!overallTransferActive)
        {
            RDS_FREE(copyDialog);
        }
#endif
        return true;
    }

    // Read files in directory
    fileList=queueDir.entryList();

#ifdef YARRA_APP_RDS
    // Track progress within this cycle's own queue directory contents.
    // In overall-tracked mode this is only used to animate smoothly within
    // the current scan phase's slice of the bar (see setScanPhase()/
    // computeDisplayPercent()) - not as a global total - so it's always
    // scoped to just this cycle, whether standalone or part of a larger loop.
    transferTotalBytes=0;
    for (int i=0; i<fileList.count(); i++)
    {
        transferTotalBytes+=QFileInfo(queueDir, fileList.at(i)).size();
    }
    transferBytesDone=0;
    transferFilesDone=0;

    if (copyDialog!=0)
    {
        if (overallTransferActive)
        {
            copyDialog->setProgressCount(overallScansDone, overallScansTotal);
        }
        else
        {
            copyDialog->setProgressCount(transferFilesDone, fileList.count());
        }
    }
#endif

    bool success=true;
    for (int i=0; i<fileList.count(); i++)
    {
        success=getFileToProcess(i);

        //RTI->log("DBG: Read file info");

        // Try to copy file
        if (success)
        {
            success=copyFile();
        }

        //RTI->log("DBG: Copied file");

        // If sucessfull, verify if copied correctly
        if (success)
        {
            success=verifyTransfer();
        }

        //RTI->log("DBG: Verified");

        // If sucessfull, remove file from queue directory
        if (success)
        {
            success=removeFile();
        }

        //RTI->log("DBG: Deleted file");

#ifdef YARRA_APP_RDS
        // Count this file toward the overall transfer regardless of whether
        // it succeeded, so the aggregate progress keeps moving forward and
        // doesn't stall on a single failed file.
        if (currentFilesize>0)
        {
            transferBytesDone+=currentFilesize;
        }

        transferFilesDone++;

        if (copyDialog!=0)
        {
            if (overallTransferActive)
            {
                // The authoritative scan count only advances once this whole
                // cycle's files have all finished (see setScanProgress()),
                // since scans and files don't map 1:1 (adjustment scan
                // bundling). Interpolate a provisional count from this
                // cycle's own file progress in the meantime, so the label
                // moves within a multi-scan cycle instead of jumping only at
                // its end - it always lands exactly on the authoritative
                // value once transferFilesDone reaches fileList.count().
                int interpolatedScansDone=phaseScansBefore
                    + int((qint64(phaseScansThisCycle)*transferFilesDone)/fileList.count());
                copyDialog->setProgressCount(interpolatedScansDone, overallScansTotal);
            }
            else
            {
                copyDialog->setProgressCount(transferFilesDone, fileList.count());
            }
        }
#endif

        QString temp_filename = currentFilename;
        releaseFile();

        //RTI->log("DBG: Released file");

        RTI->processEvents();

        if (!success) {
            RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::FileTransfer,EventInfo::Severity::Error,
                         "Error transferring a file", temp_filename);
        }
        //RTI->log("DBG: Processed events");
    }

    //RTI->log("DBG: Left loop");

#ifdef YARRA_APP_RDS
    // Finishing this cycle's queue directory doesn't mean the whole update
    // is done when an alternating/batched loop is tracking overall progress
    // across multiple cycles - only force a clean 100%/full-count finish for
    // a standalone (non-overall-tracked) transfer.
    if (!overallTransferActive)
    {
        copyDialog->setProgress(100);
        copyDialog->setProgressCount(fileList.count(), fileList.count());
    }
#endif

    fileList.clear();

#ifdef YARRA_APP_RDS
    // In overall-tracked mode, the dialog stays open across cycles -
    // beginOverallTransfer()/endOverallTransfer() own its lifetime instead.
    if (!overallTransferActive)
    {
        copyDialog->close();
        RDS_FREE(copyDialog);
    }
#endif

    return true;
}


int rdsNetwork::getQueueCount()
{
    queueDir.refresh();
    return queueDir.count();
}


bool rdsNetwork::isQueueEmpty()
{    
    return (getQueueCount()==0);
}


bool rdsNetwork::getFileToProcess(int index)
{
    if ((index<0) || (index>=fileList.count()))
    {
        RTI->log("Error: File transfer list is invalid!");
        RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::FileTransfer,EventInfo::Severity::Error,
                     "File transfer list is invalid");

        return false;
    }

    currentFilename=fileList.at(index);
    RTI->log("Transfering file " + currentFilename + "...");

    if (!queueDir.exists(currentFilename))
    {
        RTI->log("Error: File could not be found: " + currentFilename);
        RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::FileTransfer,EventInfo::Severity::Error,
                     "File could not be found", currentFilename);
        return false;
    }

    int sepPos=currentFilename.indexOf(RTI_SEPT_CHAR);

    if (sepPos==-1)
    {
        RTI->log("Error: File name does not contain protocol name: " + currentFilename);
        RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::FileTransfer,EventInfo::Severity::Error,
                     "File name does not contain protocol name", currentFilename);
        return false;
    }

    currentProt=currentFilename;
    currentProt.truncate(sepPos);
    QFileInfo fileInfo(queueDir, currentFilename);
    currentFilesize=fileInfo.size();

    return true;
}


bool rdsNetwork::copyFile()
{
    if (RTI_CONFIG->isNetworkModeDrive())
    {
        // Check if protocol directory exists
        if (!networkDrive.exists(currentProt))
        {
            if (!networkDrive.mkdir(currentProt))
            {
                RTI->log("Error: Creating protocol directory not possible: " + currentProt);
                RTI->setSevereErrors(true);
                RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::FileTransfer,EventInfo::Severity::Error,
                             "Creating protocol directory not possible", currentProt);
                return false;
            }
        }

        QString sourceName=queueDir.absoluteFilePath(currentFilename);
        QString destName=networkDrive.absolutePath() + "/" + currentProt + "/" + currentFilename;
        currentTimeStamp="";

        // Check free diskspace on the network drive
        QFileInfo srcinfo(sourceName);
        qint64 diskSpace=RTI->getFreeDiskSpace(networkDrive.absolutePath());
        RTI->debug("Disk space on netdrive = " + QString::number(diskSpace));
        RTI->log("Size of source file is " + QString::number(srcinfo.size()));

        if (srcinfo.size() > diskSpace)
        {
            RTI->log("ERROR: Not enough diskspace on network drive to transfer file.");
            RTI->log("ERROR: File size = " + QString::number(srcinfo.size()));
            RTI->log("ERROR: Remaining disk space = " + QString::number(diskSpace));
            RTI->log("Canceling transfer.");
            RTI->setSevereErrors(true);
            RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::LowDiskSpace,EventInfo::Severity::FatalError,
                         "Insufficient disk space on network drive");
            return false;
        }

        if (QFile::exists(destName))
        {
            RTI->log("WARNING: File already exists in target folder: " + destName);

            // Create a time stamp for copying the file under a different name
            currentTimeStamp="_"+QDateTime::currentDateTime().toString("ddMMyyhhmmss");

            // If a file also exists with this name, append ms to the time stamp
            if (QFile::exists(destName+currentTimeStamp))
            {
                 currentTimeStamp="_"+QDateTime::currentDateTime().toString("ddMMyyhhmmsszzz");
            }

            // If also this file exists, something must be wrong
            if (QFile::exists(destName+currentTimeStamp))
            {
                RTI->log("ERROR: Unable to create unique filename: " + destName);
                RTI->log("Something must be wrong with the file system.");
                RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::FileTransfer,EventInfo::Severity::FatalError,
                             "Unable to create unique filename",destName);

                return false;
            }

            // Add the time stamp to the file name
            destName=destName+currentTimeStamp;
            RTI->log("Appending time stamp to filename: " + destName);
        }

        int timeout=RDS_COPY_TIMEOUT;

#ifdef YARRA_APP_RDS
        // Calculate the MD5 checksum prior to copying to diagnose sporadic file corruptions.
        if (RDS_LOG_CHECKSUM)
        {
            QString checksum=rdsChecksum::getChecksum(sourceName);
            RTI->log("Local MD5 checksum is " + checksum);
        }

        // Overwrite the default timeout for the copy operation with a user setting (if not defined,
        // it will just use the default value)
        timeout=RTI_CONFIG->infoCopyTimeout;
#endif

        bool copyError=false;
        QString fileError;
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

            QElapsedTimer ti;
            ti.start();

#ifdef YARRA_APP_RDS
            // QFile::copy() doesn't report real progress, so estimate this
            // file's own completion from its size and the throughput
            // measured from previous transfers (see below). That estimate is
            // folded into this cycle's own progress (bytes done from already-
            // completed files in this cycle, plus this file's estimated
            // partial progress, over this cycle's total size) via
            // computeDisplayPercent(), which maps it into the whole update's
            // progress when an overall-tracked loop is active (see
            // setScanPhase()) or uses it directly otherwise. Simulated scan
            // files copy at real local-disk speed, which would make the
            // learned throughput estimate (and the bar) race ahead
            // immediately - use a fixed, visibly slow duration instead so
            // there's something to actually watch while testing.
            qint64 estimatedMs=RTI->isSimulation() ? 4000
                : qMax(qint64(500), (srcinfo.size()*1000)/estimatedBytesPerSec);

            QTimer progressTimer;
            if ((copyDialog!=0) && (transferTotalBytes>0))
            {
                // Show where the bar stands as of the start of this file (0%
                // progress within it) immediately, rather than waiting for
                // the timer's first tick.
                copyDialog->setProgress(computeDisplayPercent(transferBytesDone, transferTotalBytes));

                connect(&progressTimer, &QTimer::timeout, this, [this, &ti, &srcinfo, estimatedMs]()
                {
                    qint64 fileBytesEstimate=qMin(srcinfo.size(), (srcinfo.size()*ti.elapsed())/estimatedMs);
                    copyDialog->setProgress(computeDisplayPercent(transferBytesDone+fileBytesEstimate, transferTotalBytes));
                });
                progressTimer.start(200);
            }
#endif

            copyThread.start();
            q.exec();

            // Check if the event loop returned too early.
            if (!copyThread.finishedCopy)
            {
                RTI->log("Warning: QEventLoop returned too early during copy. Starting secondary loop.");

                // Wait until copying has finished, but restrict the waiting to 1 hour max
                while ((!copyThread.isFinished()) && (ti.elapsed()<timeout))
                {
                    RTI->processEvents();
                    Sleep(RDS_SLEEP_INTERVAL);
                }

                if (!copyThread.finishedCopy)
                {
                    RTI->log("Error: Second copy wait loop timed out! Copying file failed.");
                    copyError=true;
                }
            }

#ifdef YARRA_APP_RDS
            // The real copy above finishes almost instantly for simulated
            // scan files, which would make the dialog (and its progress bar)
            // flash by too fast to see or interact with. Stretch the visible
            // wait out to the estimated duration used for the progress bar,
            // continuing to process events - and the progress timer along
            // with them - so the bar keeps climbing and the dismiss button
            // stays responsive. Never runs outside simulation; real transfers
            // are already paced by their own actual copy speed.
            if (RTI->isSimulation())
            {
                while (ti.elapsed()<estimatedMs)
                {
                    RTI->processEvents();
                }
            }
#endif

#ifdef YARRA_APP_RDS
            // Refine the throughput estimate from this transfer so the next
            // file's progress bar tracks reality more closely. Ignore very
            // short transfers, where timer granularity/filesystem caching
            // effects make the measurement unreliable, and clamp the result
            // to sane bounds so one outlier can't skew future estimates too
            // far. The initial guess is a placeholder only - the first real
            // measurement replaces it outright rather than being blended
            // with it, so the second file already uses the exact throughput
            // measured from the first. From the second measurement on,
            // blend as an exponential moving average so the estimate
            // settles over several files rather than swinging on any one.
            if ((copyThread.success) && (ti.elapsed()>=50))
            {
                qint64 measuredBytesPerSec=qBound(qint64(256*1024), (srcinfo.size()*1000)/ti.elapsed(), qint64(2000)*1024*1024);

                if (!hasMeasuredThroughput)
                {
                    estimatedBytesPerSec=measuredBytesPerSec;
                    hasMeasuredThroughput=true;
                }
                else
                {
                    estimatedBytesPerSec=(estimatedBytesPerSec*7 + measuredBytesPerSec*3)/10;
                }
            }
#endif

            if (copyThread.lockError)
            {
                RTI->log("Warning: Problems while creating/removing lock file for "+destName);
                RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::FileTransfer,EventInfo::Severity::Warning,
                             "Problems while creating/removing lock file for", destName);
            }

            copyError=!copyThread.success;
            fileError = copyThread.fileErrorString;
        }

        //copyError=!QFile::copy(sourceName,destName);

        if (copyError)
        {
            RTI->log("Error: Error copying the file!");
            RTI->log("Error: Source = " + sourceName);
            RTI->log("Error: Destination = " + destName);
            RTI->setSevereErrors(true);

            QString dataString=QString("<data>") + \
                 "<filename>"  + currentFilename+"</filename>" +
                 "<sourceName>"+ sourceName+"</sourceName>" +
                 "<destName>"  + destName  +"</destName>" +
                 "<fileError>" + fileError +"</fileError>" +
                 "</data>";

            RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::FileTransfer,EventInfo::Severity::Error,
                         "Error copying file", dataString);
            return false;
        }
        RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::FileTransfer,EventInfo::Severity::Success,
                     currentFilename);
    }

    return true;
}


bool rdsNetwork::verifyTransfer()
{
    if (RTI_CONFIG->isNetworkModeDrive())
    {
        // currentTimeStamp is empty unless a file with the same name already existed in the target folder
        QString destName=networkDrive.absolutePath() + "/" + currentProt + "/" + currentFilename + currentTimeStamp;
        QFileInfo fileInfo(destName);

        if (!fileInfo.exists())
        {
            RTI->log("Error: Copied file does not exist at target position: " + destName);
            RTI->log("Error: Transferring the file was not successful.");
            RTI->setSevereErrors(true);
            RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::FileTransfer,EventInfo::Severity::Error,
                         "Error verifying file: does not exist at destination", destName);
            return false;
        }

        if (fileInfo.size() != currentFilesize)
        {
            RTI->log("Error: File size of copied file does not match!");
            RTI->log("Error: Original size = " + QString::number(currentFilesize));
            RTI->log("Error: Copy size = " + QString::number(fileInfo.size()));
            RTI->setSevereErrors(true);
            RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::FileTransfer,EventInfo::Severity::Error,
                         "Error verifying file: file size does not match.", destName);
            return false;
        }
        else
        {
            RTI->log("Size of copied file matches with source " + QString::number(fileInfo.size()));
        }
    }

    RTI->log("File transfer successful.");

    return true;
}


bool rdsNetwork::removeFile()
{
    RDS_RETONERR_LOG( queueDir.exists(currentFilename), "Error: File for deletion could not be found: " + currentFilename );

    if (!queueDir.remove(currentFilename))
    {
        RTI->log("Couldn't delete file " + currentFilename + ", retrying.");
        Sleep(DWORD(100));
        if (!queueDir.remove(currentFilename))
        {
            RTI->log("Error: Deleting file was not successful. File might be locked: " + currentFilename);
            RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::FileTransfer,EventInfo::Severity::Error,
                         "Error deleting file", currentFilename);
        }
    }

    return true;
}


void rdsNetwork::releaseFile()
{
    currentFilename="";
    currentProt="";
    currentFilesize=-1;
    currentTimeStamp="";
}


bool rdsNetwork::checkExistance(QString filename)
{
    int sepPos=filename.indexOf(RTI_SEPT_CHAR);

    if (sepPos==-1)
    {
        RTI->log("Error: File name does not contain protocol name: " + filename);
        RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::FileTransfer,EventInfo::Severity::Error,
                     "File name does not contain protocol name", filename);

        return false;
    }

    QString fileProt=filename;
    fileProt.truncate(sepPos);

    if (RTI_CONFIG->isNetworkModeDrive())
    {
        QString fullFilename=RTI_CONFIG->netDriveBasepath + "/" + RTI_CONFIG->infoName + "/" + fileProt + "/" + filename;
        return QFile::exists(fullFilename);
    }

    return false;
}


QString rdsNetwork::getConfigFileData() 
{
    QFile configFile(RTI->getAppPath() + RDS_INI_NAME);
    QString  result = "";
    try {
        if(configFile.open(QIODevice::ReadOnly)) {
            QByteArray lines = configFile.readAll();
            QTextCodec *codec = QTextCodec::codecForName("UTF-8");
            result =  codec->toUnicode(lines);
            configFile.close();
        }
    } catch (...) {
    }
    return result;
}


QString rdsNetwork::getLogFileData(int lines) 
{
    QString logName="rds.log";
    QString logPath=RTI->getLogInstance()->getLocalLogPath();
    QFile logFile(logPath+logName);
    RTI->getLogInstance()->pauseLogfile();

    QString result = "";

    try {
        if(logFile.open(QIODevice::ReadOnly))
        {
            try {
                logFile.seek(logFile.size()-1);
                int count = 0;
                while ( (count < lines) && (logFile.pos() > 0) )
                {
                    QString ch = logFile.read(1);
                    logFile.seek(logFile.pos()-2);
                    if (ch == "\n")
                        count++;
                }
                QByteArray lines = logFile.readAll();
                QTextCodec *codec = QTextCodec::codecForName("UTF-8");
                result =  codec->toUnicode(lines);
                logFile.close();
            } catch(...) {
                logFile.close();
            }
        }
    } catch (...) {
        RTI->getLogInstance()->resumeLogfile();
    }
    RTI->getLogInstance()->resumeLogfile();
    return result;
}


bool rdsNetwork::copyLogFile()
{
    // Get the filename and path of the local log files from the log instance
    QString logName="rds.log";
    QString logPath=RTI->getLogInstance()->getLocalLogPath();

    // Delete the existing log file on the network drive inside the system folder
    // No error logging here because copying the log files is of low priority
    if (networkDrive.exists(logName))
    {
        if (!networkDrive.remove(logName))
        {
            RTI->log("ERROR: Can't remove existing log file on network drive.");
            RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::Diagnostics,EventInfo::Severity::Error,
                         "Can't remove existing log file on network drive");
            return false;
        }
    }

    // Stop the log engine (i.e. flush and close the log file)
    RTI->getLogInstance()->pauseLogfile();

    // Copy the log file into the network folder
    QFile logFile(logPath+logName);
    bool logCopySuccess=logFile.copy(networkDrive.absoluteFilePath(logName));

    // Open the log file again
    RTI->getLogInstance()->resumeLogfile();

    if (!logCopySuccess)
    {
        RTI->log("WARNING: Copying the log file did not succeed! "+logFile.errorString());
        RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::Diagnostics,EventInfo::Severity::Error,
                     "Copying the log file did not succeed", logFile.errorString());
    }
    return logCopySuccess;
}


void rdsNetwork::runReconnectCmd()
{
    // TODO: Change to rdsExecHelper to cover potential early return from QEventLoop

    // If a reconnect command has been defined, call it before the update
    if (RTI_CONFIG->netDriveReconnectCmd != "")
    {
        RTI->log("Calling network drive reconnect command.");

        QProcess *myProcess = new QProcess(0);
        myProcess->setReadChannel(QProcess::StandardOutput);

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QEventLoop q;
        connect(myProcess, SIGNAL(finished(int , QProcess::ExitStatus)), &q, SLOT(quit()));
        connect(&timeoutTimer, SIGNAL(timeout()), &q, SLOT(quit()));

        // If the command does not return within 1min, then kill it
        timeoutTimer.start(RDS_CONNECT_TIMEOUT);
        myProcess->start(RTI_CONFIG->netDriveReconnectCmd);
        q.exec();

        // Timer is still active, so the process seemed to termiante normally
        if (timeoutTimer.isActive())
        {
            timeoutTimer.stop();
        }

        QString output(myProcess->readAllStandardError());
        if (!output.isEmpty()) {
            RTI->log(QString("WARNING: Network drive reconnect command generated errors!"));
            RTI->log(output);
            RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::Diagnostics,EventInfo::Severity::FatalError,
                         "Network drive reconnect command generated errors", output);
        } else {
            RTI->log(QString("Network drive reconnect command succeeded"));
            RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::Diagnostics,EventInfo::Severity::Success,
                         "Network drive reconnect command executed");
        }

        if (myProcess->state()==QProcess::Running)
        {
            RTI->log("WARNING: The network drive reconnect command timed out!");
            myProcess->kill();
            RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::Diagnostics,EventInfo::Severity::FatalError,
                         "Network drive reconnect command timed out", output);
        }

        delete myProcess;
        myProcess=0;
    }
}


void rdsNetwork::runDisconnectCmd()
{
    // If a disconnect command has been defined, call it after the update has finished
    if (!RTI_CONFIG->netDriveDisconnectCmd.isEmpty())
    {
        RTI->log("Calling network drive reconnect command.");
        rdsExecHelper exec;
        if (!exec.run(RTI_CONFIG->netDriveDisconnectCmd))
        {
            RTI->log("ERROR: Execution of disconnect command failed '" + RTI_CONFIG->netDriveDisconnectCmd + "'");
            RTI_NETLOG.postEvent(EventInfo::Type::RawDataStorage,EventInfo::Detail::Diagnostics,EventInfo::Severity::Error,
                         "Execution of disconnect command failed");
        }
    }
}



rdsCopyThread::rdsCopyThread()
    : QThread()
{
    success=false;
    sourceName="";
    destName="";
    finishedCopy=false;
    fileErrorString = "";
    fileError = QFile::FileError::NoError;
}


void rdsCopyThread::run()
{
    finishedCopy=false;
    lockError=false;

    // Generate name for lockfile
    QString lockFilename=destName;
    lockFilename.truncate(lockFilename.lastIndexOf("."));
    lockFilename+=".lock";

    // Create lock file
    QFile lockFile(lockFilename);
    if (!lockFile.open(QIODevice::ReadWrite))
    {
        lockError=true;
    }
    else
    {
        lockFile.flush();
        lockFile.close();
    }

    // Copy file
    QFile sourceFile(sourceName);

    success=sourceFile.copy(destName);
    fileError=sourceFile.error();
    fileErrorString=sourceFile.errorString();

    // Remove lock file
    if (!lockFile.remove())
    {
        lockError=true;
    }

    finishedCopy=true;
    return;
}

