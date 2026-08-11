#include "rds_raid.h"
#include "rds_global.h"
#include "rds_network.h"
#include "rds_processcontrol.h"
#include "rds_anonymizeVB17.h"

#ifdef YARRA_APP_ORT
    #include "ort_global.h"
#endif

#if defined(Q_OS_WIN)
    #include <QtCore/QLibrary>
    #include <QtCore/qt_windows.h>
#endif
#if defined(Q_OS_UNIX)
    #include <time.h>
#endif


void rdsRaidEntry::addToUrlQuery(QUrlQuery& query)
{
    // Use the serial number as ID. If the serial number has not been
    // defined, then fallback to the name selected in the configuration
    // dialog (which might not be unique)
    QString scannerID=RTI_CONFIG->infoSerialNumber;
    if (scannerID=="0")
    {
        scannerID=RTI_CONFIG->infoName;
    }

    query.addQueryItem("creation_time",  creationTime.toString());
    query.addQueryItem("closing_time",   closingTime.toString());
    query.addQueryItem("file_id",        QString::number(fileID));
    query.addQueryItem("meas_id",        QString::number(measID));
    query.addQueryItem("protocol_name",  protName);
    query.addQueryItem("patient_name",   patName);
    query.addQueryItem("size",           QString::number(size));
    query.addQueryItem("size_on_disk",   QString::number(sizeOnDisk));
    query.addQueryItem("scanner_serial", scannerID);
    query.addQueryItem("exam_id",        QString(""));
}


rdsRaid::rdsRaid()
{
    // Select the output directory for queuing files. Different
    // directories are used for ORT and RDS.
#ifdef YARRA_APP_ORT
    queueDir.cd(RTI->getAppPath() + "/" + ORT_DIR_QUEUE);
#else
    queueDir.cd(RTI->getAppPath() + "/" + RDS_DIR_QUEUE);
#endif

    // Set the command line and options for calling the raid tool
    if (RTI->isSimulation())
    {
        raidToolCmd=QDir::toNativeSeparators( RTI->getAppPath()+QString("/") ) + RDS_RAIDSIMULATOR_NAME;
    }
    else
    {
        // The new Syngo version uses a different path for the binaries where the RaidTool is located
        if (RTI->isSyngoXALine() || (RTI->isSyngoXBLine()))
        {
            raidToolCmd=QDir::toNativeSeparators( RTI->getSyngoXABinPath()+QString("/") ) + RDS_RAIDTOOL_NAME;
        }
        else
        {
            raidToolCmd=QDir::toNativeSeparators( RDS_RAIDTOOL_PATH+QString("/") ) + RDS_RAIDTOOL_NAME;
        }
    }

    // For VD13 use local patched versions of the RaidTool that flush stdout
    // before the program shutdown. Otherwise, it will not be possible to read the program output.
    usePatchedRaidTool=false;
    patchedRaidToolMissing=false;

    useVerboseMode=true;
    missingVerboseData=false;

    if (RTI->getSyngoMRVersion()==rdsRuntimeInformation::RDS_VD13C)
    {
        usePatchedRaidTool=true;
        raidToolCmd=QDir::toNativeSeparators( RTI->getAppPath()+QString("/") ) + "RaidTool_VD13C.dll";
    }

    if (RTI->getSyngoMRVersion()==rdsRuntimeInformation::RDS_VD13D)
    {
        usePatchedRaidTool=true;
        raidToolCmd=QDir::toNativeSeparators( RTI->getAppPath()+QString("/") ) + "RaidTool_VD13D.dll";
    }

    if (RTI->getSyngoMRVersion()==rdsRuntimeInformation::RDS_VD13A)
    {
        usePatchedRaidTool=true;
        raidToolCmd=QDir::toNativeSeparators( RTI->getAppPath()+QString("/") ) + "RaidTool_VD13A.dll";
    }

    if (RTI->getSyngoMRVersion()==rdsRuntimeInformation::RDS_VD13B)
    {
        usePatchedRaidTool=true;
        raidToolCmd=QDir::toNativeSeparators( RTI->getAppPath()+QString("/") ) + "RaidTool_VD13B.dll";
    }

    if (usePatchedRaidTool)
    {
        // Check for existence of patched RaidTool
        QDir appDir(RTI->getAppPath());
        if (!appDir.exists(raidToolCmd))
        {
            QMessageBox msgBox;
            msgBox.setWindowTitle("Patched Raid Tool not found");
            msgBox.setText("Systems running VD13 software require modified local version of the RaidTool, which was not found. <br><br>"\
                           "Please check the Yarra website to obtain these files.");
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setWindowIcon(RDS_ICON);
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.exec();

            patchedRaidToolMissing=true;
        }
    }

    // Ask RTI for the IP of the images as this differs for VD13+
    raidToolIP=QString("-a ") + RTI->getSyngoImagerIP() + " -p " + RDS_IMAGER_PORT;

    currentFilename="";
    lastProcessedFileID=-1;
    lastProcessedFileIDScaninfo=-1;
    ignoreLPFID=false;
    ortMissingDiskspace=false;
    scanActive=false;

    // Read the file ID of the last processed file from file.
    // The LPFI will increase speed for parsing the output from the RaidTool
    // because parsing can stop when the LPFI has been found.
    readLPFI();          

    // Initialize the internal variable used for holding the system name
    // when using the class within ORT (needed to remove the dependency
    // on the RTI class)
    ortSystemName="Unknown";    
    ortTaskID="";
}


rdsRaid::~rdsRaid()
{
}


bool rdsRaid::setLocalBufferPath(QString bufferPath)
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

    RTI->log("Using custom buffer path");
    return true;
}


bool rdsRaid::isLocalBufferPathValid()
{
    return queueDir.exists();
}


bool rdsRaid::callRaidTool(QStringList command, QStringList options, int timeout)
{
    // Clear the output buffer
    raidToolOutput.clear();

    QProcess *myProcess = new QProcess(0);
    myProcess->setReadChannel(QProcess::StandardOutput);

    QStringList args;
    args << command;
    args << raidToolIP;
    args << options;

    QString argLine=args.join(" ");

    RTI->debug("Calling RAID tool: " + raidToolCmd);
    RTI->debug("Arguments: " + argLine );

    args.clear();
    args << argLine.split(" ");

    bool success=false;

    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(timeout);
    QEventLoop q;
    connect(myProcess, SIGNAL(finished(int , QProcess::ExitStatus)), &q, SLOT(quit()));
    connect(&timeoutTimer, SIGNAL(timeout()), &q, SLOT(quit()));

    // Time measurement to diagnose RaidTool calling problems
    QElapsedTimer ti;
    ti.start();
    timeoutTimer.start();
    myProcess->start(raidToolCmd, args);
    q.exec();

    // Check for problems with the event loop: Sometimes it seems to return to quickly!
    // In this case, start a second while loop to check when the process is really finished.
    if ((timeoutTimer.isActive()) && (myProcess->state()==QProcess::Running))
    {
        timeoutTimer.stop();
        RTI->log("Warning: QEventLoop returned too early. Starting secondary loop.");
        while ((myProcess->state()==QProcess::Running) && (ti.elapsed()<timeout))
        {
            RTI->processEvents();
            Sleep(RDS_SLEEP_INTERVAL);
        }

        // If the process did not finish within the timeout duration
        if (myProcess->state()==QProcess::Running)
        {
            RTI->log("Warning: Process is still active. Killing process.");
            myProcess->kill();
            success=false;
        }
        else
        {
            success=true;
        }
    }
    else
    {
        // Normal timeout-handling if QEventLoop works normally
        if (timeoutTimer.isActive())
        {
            success=true;
            timeoutTimer.stop();
        }
        else
        {
            RTI->log("Warning: Process event loop timed out.");
            RTI->log("Warning: Duration since start "+QString::number(ti.elapsed())+" ms");
            success=false;
            if (myProcess->state()==QProcess::Running)
            {
                RTI->log("Warning: Process is still active. Killing process.");
                myProcess->kill();
            }
        }
    }

    if (!success)
    {
        // The process timeed out. Probably some error occurred.
        RTI->log("ERROR: Timeout during call of RaidTool!");
        RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "Process timeout during RaidTool call");
    }
    else
    {
        // Read the output lines from the process and store in string list
        // for parsing
        char buf[1024];
        qint64 lineLength = -1;

        do
        {
            lineLength=myProcess->readLine(buf, sizeof(buf));
            if (lineLength != -1)
            {
                //RTI->debug("Read line!");
                raidToolOutput << QString(buf);
            }
        } while (lineLength!=-1);
    }

    delete myProcess;
    myProcess=0;

    return success;
}


void rdsRaid::readLPFI()
{
    QSettings lpfiFile(RTI->getLpfiPath(), QSettings::IniFormat);
    lastProcessedFileID        =lpfiFile.value("LPFI/FileID",     -1).toInt();
    lastProcessedFileIDScaninfo=lpfiFile.value("Scaninfo/FileID", -1).toInt();
}


void rdsRaid::saveLPFI()
{
    QSettings lpfiFile(RTI->getLpfiPath(), QSettings::IniFormat);
    lpfiFile.setValue("LPFI/FileID",     lastProcessedFileID);
    lpfiFile.setValue("Scaninfo/FileID", lastProcessedFileIDScaninfo);
}


bool rdsRaid::saveRaidFile(int fileID, QString filename, bool saveAdjustments, bool anonymize)
{
    QString filePath=QDir::toNativeSeparators(queueDir.absolutePath()) + "\\" + filename;
    RTI->debug("File path for RAID export: " + filePath);

    // Check if the file already exists in the queue directory and remove because
    // it might be broken
    if (queueDir.exists(filename))
    {
        RTI->log("Export file name already exists in queue directory: " + filename);
        RTI->log("Overwriting file.");
        RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Warning, "File already exists in queue dir");

        if (!queueDir.remove(filename))
        {
            RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "Unable to overwrite file in queue dir");
            RTI->log("Error overwriting file! Files seems locked. Canceling.");
            return false;
        }
    }

    // Call raidtool to store scan fileID under filename
    QStringList cmd, opt;
    cmd << QString("-m ") + QString::number(fileID);
    cmd << "-o " + filePath;

    // For the VD line, add option for inclusion of adjustment scans
    if ((RTI->isSyngoVDLine()) || (RTI->isSyngoVELine()) || (RTI->isSyngoXALine()) || (RTI->isSyngoXBLine()))
    {
        if (saveAdjustments)
        {
            opt << "-D";
        }
    }

    // For the VD/VE line and VB20P, add options for anonymization
    if ((RTI->isSyngoVDLine()) || (RTI->isSyngoVELine() || (RTI->isSyngoXALine()) || (RTI->isSyngoXBLine()))
        || (RTI->getSyngoMRVersion()==rdsRuntimeInformation::RDS_VB20P)
        || (RTI->getSyngoMRVersion()==rdsRuntimeInformation::RDS_VB19A)
        || (RTI->getSyngoMRVersion()==rdsRuntimeInformation::RDS_VB19B))
    {
        if (!anonymize)
        {
            // NOTE: Option -k means "Do not anonymize", VD/VB20P RaidTool will anonymize by default
            opt << "-k";
        }
    }

    int timeout=RDS_RAIDSTORE_TIMEOUT;

#ifdef YARRA_APP_RDS
    // The RDS tool allows overwriting the default process timeout for store events
    // because writing the files can take a long time for very large scans
    timeout=RTI_CONFIG->infoRAIDTimeout;
#endif

    // Call Raidtool to safe the file
    if (!callRaidTool(cmd, opt,timeout))
    {
        // If it fails (e.g. due to timeout), check if a partially written file exist
        // This file needs to be deleted because it would be copied to the directory
        // otherwise.
        RTI->log("Saving file from RAID failed. Canceling.");
        if (queueDir.exists(filename))
        {
            RTI->log("Potentially corrupted file found in queue directory.");
            RTI->log("Deleting file.");
            if (!queueDir.remove(filename))
            {
                RTI->log("Deleting file failed. File might be locked.");
            }
        }
        return false;
    }

    // Evaluate RAID output
    if (!parseOutputFileExport())
    {
        RTI->log("ERROR: File transfer from RAID was not successful. Retrying in 2 minutes.");
        RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "Transfer from RAID not successful. Retry in 2min.");
        RTI_CONTROL->setExplicitUpdate(RDS_UPDATETIME_RAIDRETRY);        
        return false;
    }

    // Check if files exists in queue directory
    if (!queueDir.exists())
    {
        RTI->log("Error: Saved RAID file not found: " + filename);
        RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "Saved RAID file not found");
        return false;
    }

    RTI->log("Exported file " + filename);

    return true;
}


bool rdsRaid::parseOutputFileExport()
{
    bool isSuccess=false;

    RTI->debug("Received " + QString::number(raidToolOutput.count()) + " lines from the RAID tool.");

    // Parse all text output lines received from the RaidTool
    for (int i=0; i<raidToolOutput.count(); i++)
    {
        if (raidToolOutput.at(i).contains(RDS_RAID_SUCCESS))
        {
            isSuccess=true;
            break;
        }

        if (raidToolOutput.at(i).contains(RDS_RAID_ERROR_INIT))
        {
            RTI->log("Error initializing RAID access.");
            RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "Error initializing RAID access");
            isSuccess=false;
            break;
        }

        if (raidToolOutput.at(i).contains(RDS_RAID_ERROR_FILE))
        {
            RTI->log("Error finding file on RAID.");
            RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "Error finding file on RAID");
            isSuccess=false;
            break;
        }

        if (raidToolOutput.at(i).contains(RDS_RAID_ERROR_DIR))
        {
            RTI->log("Error reading RAID directory.");
            RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "Error reading RAID directory");
            isSuccess=false;
            break;
        }

        if (raidToolOutput.at(i).contains(RDS_RAID_ERROR_COPY))
        {
            RTI->log("Error copying RAID file.");
            RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "Error copying RAID file");
            isSuccess=false;
            break;
        }
    }

    return isSuccess;
}


bool rdsRaid::readRaidList()
{
    // Dermines whether the verbose mode is used for directory readout
    if ( (RTI->getSyngoMRVersion()==rdsRuntimeInformation::RDS_VB13A) ||
         (RTI->getSyngoMRVersion()==rdsRuntimeInformation::RDS_VB15A)
       )
    {
        useVerboseMode=false;
    }
    else
    {
        useVerboseMode=true;
    }

    // Call raid tool and obtain directory list
    // Parse text output and sort entries into raidlist structure
    QStringList cmd, opt;
    cmd << "-d";

    // Starting with VB20P, also direcoty entries are anonymize (VD13 not yet affected).
    if ((RTI->getSyngoMRVersion()==rdsRuntimeInformation::RDS_VB20P)
        || (RTI->getSyngoMRVersion()==rdsRuntimeInformation::RDS_VB19A)
        || (RTI->getSyngoMRVersion()==rdsRuntimeInformation::RDS_VB19B)
        || (RTI->isSyngoVELine()) || (RTI->isSyngoXALine()) || (RTI->isSyngoXBLine()))
    {
        opt << "-k";
    }

    if (useVerboseMode)
    {
        opt << "-v";
    }

    if (!callRaidTool(cmd, opt))
    {
        RTI->log("Reading RAID directory failed. Canceling.");
        RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "Reading RAID failed");
        return false;
    };

    // Update last processed file ID from the LPFI file (might be needed here in case a remote LPFI file is used for dual-console systems)
    readLPFI();

    // Parse the raid output
    if (!parseOutputDirectory())
    {
        RTI->log("Reading the RAID directory failed.  Retrying in 2 minutes.");
        RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "Parser error. Retry in 2min");
        RTI_CONTROL->setExplicitUpdate(RDS_UPDATETIME_RAIDRETRY);
        return false;
    }

    return true;
}


bool rdsRaid::parseOutputDirectory()
{
    clearRaidList();
    scanActive=false;

    bool isSuccess=true;
    bool dirHeadFound=false;
    bool lineProcessed=false;
    missingVerboseData=false;

    QString unknownVerboseAttributeFound="";

    RTI->debug("Received " + QString::number(raidToolOutput.count()) + " lines from the RAID tool.");

    QString dirHead=RDS_RAID_DIRHEAD;

    // Some of the software versions have a different header format

    if ((RTI->getRaidToolFormat()==rdsRuntimeInformation::RDS_RAIDTOOL_VD13C)
        || (RTI->getRaidToolFormat()==rdsRuntimeInformation::RDS_RAIDTOOL_VE))
    {
        dirHead=RDS_RAID_DIRHEAD_VD13C;
    }

    if (RTI->getSyngoMRVersion()==rdsRuntimeInformation::RDS_VB15A)
    {
        dirHead=RDS_RAID_DIRHEAD_VB15;
    }

    if (RTI->getSyngoMRVersion()==rdsRuntimeInformation::RDS_VB13A)
    {
        dirHead=RDS_RAID_DIRHEAD_VB13;
    }

    for (int i=0; i<raidToolOutput.count(); i++)
    {
        lineProcessed=false;

        // Check for error codes
        if (raidToolOutput.at(i).contains(RDS_RAID_ERROR_INIT))
        {
            RTI->log("Error initializing RAID access.");
            RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "Error initializing RAID access");
            isSuccess=false;
            break;
        }

        if (raidToolOutput.at(i).contains(RDS_RAID_ERROR_DIR))
        {
            RTI->log("Error reading RAID directory.");
            RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "Error reading RAID directory");
            isSuccess=false;
            break;
        }

        // Check if current line is the header of the RAID index. If so, we can
        // start parsing for raid entries from the next line.
        if (raidToolOutput.at(i).contains(dirHead))
        {
            if (dirHeadFound)
            {
                RTI->log("ERROR: RAID header found twice. Something is wrong with the RAID directory.");
                RTI->log("ERROR: Canceling.");
                RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "RAID header found twice");
                return false;
            }

            // Found the header of the RAID directory
            dirHeadFound=true;
            lineProcessed=true;
        }

        // On the VD line (except VD13C/D), the RaidTool outputs extra information on depending files,
        // which should be skipped here
        if ((RTI->getRaidToolFormat()==rdsRuntimeInformation::RDS_RAIDTOOL_VD11) && (dirHeadFound))
        {
            QString raidLine=raidToolOutput.at(i);

            if ((!lineProcessed) && (raidLine.contains(RDS_RAID_VD_IGNORE1)))
            {
                // Detected line of type "dependent file:", skip further processing
                lineProcessed=true;
            }
            if ((!lineProcessed) && (raidLine.contains(RDS_RAID_VD_IGNORE2)))
            {
                // Detected line of type "measID:", skip further processing
                lineProcessed=true;
            }
        }

        if ((!lineProcessed) && (dirHeadFound) && (raidToolOutput.at(i).length() > 2))
        {
            rdsRaidEntry raidEntry;
            raidEntry.attribute=RDS_SCANATTRIBUTE_OK;

            bool res=true;
            bool skipEntry=false;

            // Parse raid line
            QString raidLine=raidToolOutput.at(i);

            // Remove the CR+LF at the end
            raidLine.chop(2);

            QString origLine=raidLine;

            if (useVerboseMode)
            {
                chopDependingIDsVerbose(raidLine, raidEntry.attribute, unknownVerboseAttributeFound);
            }
            else
            {
                // For the VD13+ format, chop the IDs of the depending scans from the end of the line
                if ((RTI->getRaidToolFormat()==rdsRuntimeInformation::RDS_RAIDTOOL_VD13C)
                    || (RTI->getRaidToolFormat()==rdsRuntimeInformation::RDS_RAIDTOOL_VE))
                {
                    chopDependingIDs(raidLine);
                }
            }

            QString temp="";

            // NOTE: Format of the raid entries is %7u%11u%32s%32s%9s%13I64u%13I64u%22s%22s
            // NOTE: On VB13 and VB15, the format is different (no patient name is printed, protocol has 16 chars).
            //       Format is should be %7u%11u%16s%9s%13I64u%13I64u%22s%22s

            // ## FileID
            // Read the FileID from the front and remove from raidLine
            temp=raidLine.left(7);
            raidLine.remove(0,7);

            // Remove preceeding " " characters
            temp.remove(" ");
            raidEntry.fileID=temp.toInt(&res);

            if (!res)
            {
                RTI->log("ERROR: Parsing the RAID directory was not successful.");
                RTI->log("ERROR: Conversion error for fileID: " + temp);
                RTI->log("ERROR: Original line: " + origLine);
                RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "Unable to parse RAID succesfully");
                return false;
            }

            // The parsing should be stopped when the last-processed file ID is reached. However,
            // for use with the ORT client, this mode should be ignored.
            if (!ignoreLPFID)
            {
                // ## Evaluate the LPFI value
                if ((raidList.count()==0) && (raidEntry.fileID<lastProcessedFileID))
                {
                    // Latest file on the RAID, i.e. the file with the highest fileID, has
                    // a value the is smaller than the lastProcessedFileID --> This can
                    // only mean that an overlap of the fileID has occurred. Therefore,
                    // reset the lastProcessedFileID counter
                    lastProcessedFileID=-1;
                    lastProcessedFileIDScaninfo=-1;
                }

                if (raidEntry.fileID<=lastProcessedFileID)
                {
                    // Scan has already been processed during previous run. We can stop here
                    break;
                }
            }
            else
            {
#ifdef YARRA_APP_ORT
                // Limit the amount of parsed scans to speed up the update
                if (raidList.count()>ORT_RAID_MAXPARSECOUNT)
                {
                    break;
                }
#endif
            }

            // ## MeasID
            temp=raidLine.left(11);
            raidLine.remove(0,11);
            temp.remove(" ");
            raidEntry.measID=temp.toInt(&res);

            if (!res)
            {
                RTI->log("ERROR: Parsing the RAID directory was not successful.");
                RTI->log("ERROR: Conversion error for measID: " + temp);
                RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Error, "Unable to parse RAID (conv error measID)");
                return false;
            }

            if (raidEntry.measID >= 3000000)
            {
                // Measurement is a retrorecon. These data sets should not be saved.
            }
            else
            {
                // VB13 and VB15 have a different format (no patient name) and require
                // a different processing (encapsulated in helper function)
                if (RTI->getRaidToolFormat()==rdsRuntimeInformation::RDS_RAIDTOOL_VB15)
                {
                    parseVB15Line(raidLine, &raidEntry);
                }
                else
                {
                    // Parse the line for all other software versions

                    // ## protName

                    // The next 32 chars will be definitely the protocol. NOTE: The protocol name
                    // can be longer than 32 characters as well as the patient name. Therefore, it
                    // is not possible to separate both entries for lengths > 32 because both values
                    // can contain space characters.
                    temp=raidLine.left(32);
                    raidLine.remove(0,32);
                    removePrecedingSpace(temp);
                    raidEntry.protName=temp;

                    // Now start evaluating the string from the back because we do not know how long
                    // the combination of protName + patName is (these can be longer than 32 characters)

                    // ## Closing date
                    temp=raidLine.right(22);
                    raidLine.chop(22);
                    removePrecedingSpace(temp);
                    raidEntry.closingTime=QDateTime::fromString(temp,"dd.MM.yyyy HH:mm:ss");

                    // ## Creation date
                    temp=raidLine.right(22);
                    raidLine.chop(22);
                    removePrecedingSpace(temp);
                    raidEntry.creationTime=QDateTime::fromString(temp,"dd.MM.yyyy HH:mm:ss");

                    // ## Size on disk
                    temp=raidLine.right(13);
                    raidLine.chop(13);
                    removePrecedingSpace(temp);
                    raidEntry.sizeOnDisk=temp.toLongLong();

                    // ## Size
                    temp=raidLine.right(13);
                    raidLine.chop(13);
                    removePrecedingSpace(temp);
                    raidEntry.size=temp.toLongLong();

                    // TODO: On VB17, exclude these weird preceding files with small size and identical protocol name
                    //       Check if these files always have the same size, so that they can be identified based on the size.

                    // Check the status information for active scans
                    temp=raidLine.right(9);
                    if (temp.contains("wip"))
                    {
                        // Scan is not closed. This can only be the case for the first
                        // file on raid. This means, scanning is active. Raw data storate
                        // should be postponed. Scan info transfer can take place.
                        scanActive=true;
                        skipEntry=true;
                    }
                    raidLine.chop(9);                    

                    // ## PatName
                    // The remaining part should be the patient name
                    temp=raidLine;

                    // For RDS the patient name should not be trimmed as this might confuse the
                    // exam aggregation mechanism of the log server backend
                #ifndef YARRA_APP_RDS
                    removePrecedingSpace(temp);
                #endif

                    raidEntry.patName=temp;

                    //TODO: The patient name might still contain the date of birth.
                    //      In this case, the format is patname,YYYYMMDD
                }

                if (!skipEntry)
                {
                    // Add entry to raid list (will be copied internally)
                    addRaidEntry(&raidEntry);
                }
            }
        }
    }

    if (!dirHeadFound)
    {
        RTI->log("ERROR: Could not receive RAID directory.");
        isSuccess=false;

        if (RTI->isSimulation())
        {
            RTI->log("Simulation mode --> Overwriting error.");
            isSuccess=true;
        }
    }

    if (useVerboseMode && missingVerboseData)
    {
        RTI->log("WARNING: Missing verbose information!");
    }

    if (useVerboseMode && (!unknownVerboseAttributeFound.isEmpty()))
    {
        RTI->log("WARNING: Unknown attributes in verbose information found!");
        RTI->log("WARNING: Attribute = "+unknownVerboseAttributeFound);
    }

    return isSuccess;
}


bool rdsRaid::parseVB15Line(QString line, rdsRaidEntry* entry)
{
    // Format is should be %7u%11u%16s%9s%13I64u%13I64u%22s%22s

    // Start evaluating the string from the back because we do not know how long
    // the protName is (can be longer than 16 characters)

    // ## Closing date
    QString temp=line.right(22);
    line.chop(22);
    removePrecedingSpace(temp);
    entry->closingTime=QDateTime::fromString(temp,"dd.MM.yyyy HH:mm:ss");

    // ## Creation date
    temp=line.right(22);
    line.chop(22);
    removePrecedingSpace(temp);
    entry->creationTime=QDateTime::fromString(temp,"dd.MM.yyyy HH:mm:ss");

    // ## Size on disk
    temp=line.right(13);
    line.chop(13);
    removePrecedingSpace(temp);
    entry->sizeOnDisk=temp.toLongLong();

    // ## Size
    temp=line.right(13);
    line.chop(13);
    removePrecedingSpace(temp);
    entry->size=temp.toLongLong();

    // The Status information is not evaluated
    line.chop(9);

    // ## ProtName
    // The remaining part should be the protocol name
    temp=line;
    removePrecedingSpace(temp);
    entry->protName=temp;

    // Patient name is not available from the VB15/13 RaidTool
    entry->patName="Not available";

    return true;
}


bool rdsRaid::createExportList()
{   
    exportList.clear();

    int raidCount=raidList.count();
    int raidIndex=0;  

    // Evalute RaidList backwards and filter measurements that have to be saved
    // NOTE: Backward evaluation is needed to ensure that the LPFI mechanism works
    for (int rc=0; rc<raidCount; rc++)
    {
        // NOTE: RAID entries are evaluated from the back
        raidIndex=raidCount-1-rc;
        rdsRaidEntry* currentEntry=raidList.at(raidIndex);

        // Loop over all protocols
        for (int pc=0; pc<RTI_CONFIG->getProtocolCount(); pc++)
        {
            QString name="";
            QString filter="";
            bool saveAdjustData=false;
            bool anonymizeData=false;
            bool smallFiles=false;
            bool remotelyDefined=false;

            RTI_CONFIG->readProtocol(pc, name, filter, saveAdjustData, anonymizeData, smallFiles, remotelyDefined);

            // Check if protocol name from raid contains filter tag
            if (currentEntry->protName.contains(filter))
            {
                // TODO: If in verbose mode, don't export files that have the error attribute

                // Check the size. Only export measurements that are larger than 1Mb. Measurements
                // smaller than 1Mb are most likely adjustment scans (shims etc), which should
                // not be transferred, unless explicitly enables for the project.
                if ((currentEntry->size > RDS_FILESIZE_FILTER) || (smallFiles))
                {
                    addExportEntry(raidIndex, pc, name, anonymizeData, saveAdjustData);
                }
            }
        }
    }

    RTI->log(QString::number(exportList.count()) + " files to be processed.");

    return true;
}


bool rdsRaid::anonymizeCurrentFile()
{
    // NOTE: On VD11 and newer, the RaidTool does the job for us
    if ((RTI->isSyngoVDLine()) || (RTI->isSyngoVELine()) || (RTI->isSyngoXALine()) || (RTI->isSyngoXBLine()))
    {
        return true;
    }

    bool result=false;

    QString fullFilename=queueDir.absoluteFilePath(currentFilename);    
    result=rdsAnonymizeVB17::anonymize(fullFilename);

    // NOTE: When the anonymization fails, the file has to be deleted!
    if (!result)
    {
        RTI->log("ERROR: Anonymization failed! Deleting file: " + currentFilename);
        QFile::remove(fullFilename);
    }

    return result;
}


bool rdsRaid::findAdjustmentScans()
{
    // NOTE: On VD11+, RaidTool does the job for us
    if ((RTI->isSyngoVDLine()) || (RTI->isSyngoVELine()) || (RTI->isSyngoXALine()) || (RTI->isSyngoXBLine()))
    {
        return true;
    }

    // Clear old entries of adjustment scans list
    currentAdjustScans.clear();

    // Variables to keep track which of the adjustment scan have been found
    // Currently, we are only searching for AdjCoilSens
    bool foundCoilSens=false;

    // Now search the raid list for depending adjustment scans
    for (int i=currentRaidIndex+1; i<raidList.count(); i++)
    {
        // Check if the scan is a AdjCoilSens scan and add to storage list
        if ((!foundCoilSens) && (raidList.at(i)->protName.contains(RDS_ADJUSTSCANVB17_COILSENS)))
        {
            currentAdjustScans.append(i);
            foundCoilSens=true;
        }

        // Terminate the search if all needed adjustment scans have been found
        if (foundCoilSens)
        {
            break;
        }
    }

    return true;
}


bool rdsRaid::processTotalExportList()
{
    bool result=true;

    while ((result==true) && (exportList.count()>0))
    {
        result=exportScanFromList();

        RTI->processEvents();
        if (RTI->isPostponementRequested())
        {
            RTI->log("Received postponement request. Stopping update.");
            RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::Success, "Postpone requested");
            break;
        }
    }

    return result;
}


bool rdsRaid::processExportListEntry()
{
    return exportScanFromList();
}


bool rdsRaid::exportsAvailable()
{
    return (exportList.count()>0);
}


bool rdsRaid::exportScanFromList()
{
    if (exportList.count()==0)
    {
        // Export list is empty, we are done.
        return true;
    }

    // Get the RAID info for the file that is in the front of the export list
    currentRaidIndex=getFirstExportEntry()->raidIndex;
    bool saveAdjustScans=getFirstExportEntry()->adjustmentScans;
    bool anonymize=getFirstExportEntry()->anonymize;
    QString bufferPath = queueDir.absolutePath();

    // Test if enough space for exporting the file is available!
    if ( RTI->getFreeDiskSpace(bufferPath) < getRaidEntry(currentRaidIndex)->size )
    {
        RTI->log("ERROR: Available disk space too small for exporting data.");
        RTI->log("ERROR: Needed    = " + QString::number(getRaidEntry(currentRaidIndex)->size));
        RTI->log("ERROR: Available = " + QString::number(RTI->getFreeDiskSpace(bufferPath)));
        RTI->showOperationWindow();
        RTI_NETLOG.postEvent(EventInfo::Type::Update, EventInfo::Detail::Information, EventInfo::Severity::FatalError, "Not enough disk space");
        return false;
    }

    // Retrieve the RAID file ID of the scan scheduled for export
    RDS_RETONERR( setCurrentFileID() );

    // Save FileID for setting the LPFI later (and also for setting the name of the adjustment scans)
    int scanFileID=currentFileID;

    RDS_RETONERR( setCurrentFilename() );

/*
    // Change in transfer strategy: Files should always be copied, even if
    // they already exist (because the existing file might be from an incomplete
    // transfer). If the file already exists, a time stamp will be added to the name.

    #ifdef YARRA_APP_ORT
        // If we compile for ORT, the network object is not available.
        bool scanAlreadyExists=false;
    #else
        // Check on target directory if the file already exists.
        bool scanAlreadyExists=RTI_NETWORK->checkExistance(currentFilename);
    #endif

    if (scanAlreadyExists)
    {
        RTI->log("File already exists on network location: " + currentFilename);
        RTI->log("Skipping export of file.");
    }
    else
*/

#ifdef YARRA_APP_RDS
    RTI->log("Selected file name "   + currentFilename);
    RTI->log("Size of scan on RAID " + QString::number(getRaidEntry(currentRaidIndex)->size));
#endif

    RDS_RETONERR( saveRaidFile(currentFileID, currentFilename, saveAdjustScans, anonymize) );

    // Anonymize the raw data, but only on VB line. In VD, the raid tool
    // automatically anonymizes the data if desired
    if ((RTI->isSyngoVBLine()) && (anonymize))
    {
        RDS_RETONERR( anonymizeCurrentFile() );
    }

    // Fetch the depending adjustment scans, but only for VB line.
    // The VD line provides the option to store adjustment data
    // automatically.
    if ((RTI->isSyngoVBLine()) && (saveAdjustScans))
    {
        RDS_RETONERR( findAdjustmentScans() );

        for (int i=0; i<currentAdjustScans.count(); i++)
        {
            currentRaidIndex=currentAdjustScans.at(i);

            RDS_RETONERR( setCurrentFileID() );
            RDS_RETONERR( setCurrentFilename(scanFileID) );
            RDS_RETONERR( saveRaidFile(currentFileID, currentFilename, false, anonymize) );

            if (anonymize)
            {
                RDS_RETONERR( anonymizeCurrentFile() );
            }
        }
    }

    // Store the FileID of the main scan, used for
    setLPFI(scanFileID);
    saveLPFI();

    // Free the processed entry from the export list
    delete exportList.takeFirst();

    return true;
}


bool rdsRaid::setCurrentFilename(int refID)
{
    currentFilename="";

    // Get the actual scan from the export list to obtain the prot name
    rdsExportEntry* exportEntry=getFirstExportEntry();

    // Retrieve the information for the current file to export
    // NOTE: The current file is determined by currentRaidID
    rdsRaidEntry* raidEntry=0;
    if (exportEntry!=0)
    {
        raidEntry=getRaidEntry(currentRaidIndex);
    }

    if ((exportEntry==0) || (raidEntry==0))
    {
        RTI->log("WARNING: setCurrentFilename() called with invalid export or raid entry. Data export canceled.");
        return false;
    }

    currentFilename += exportEntry->protName + RTI_SEPT_CHAR;
    currentFilename += "S" + RTI_CONFIG->infoName + RTI_SEPT_CHAR;

    if (refID!=-1)
    {
        currentFilename += "R" + QString::number(refID) + RTI_SEPT_CHAR;
    }

    currentFilename += "F" + QString::number(raidEntry->fileID) + RTI_SEPT_CHAR;
    currentFilename += "M" + QString::number(raidEntry->measID) + RTI_SEPT_CHAR;
    currentFilename += "D" + raidEntry->creationTime.toString("ddMMyy") + RTI_SEPT_CHAR;
    currentFilename += "T" + raidEntry->creationTime.toString("HHmmss") + RTI_SEPT_CHAR;

    QString protocolName=raidEntry->protName;

    // Remove characters from the protocol name that are not allowed in file names
    protocolName.remove(QChar('.'),  Qt::CaseInsensitive);
    protocolName.remove(QChar('/'),  Qt::CaseInsensitive);
    protocolName.remove(QChar('\\'), Qt::CaseInsensitive);
    protocolName.remove(QChar(':'),  Qt::CaseInsensitive);
    protocolName.remove(QChar(' '),  Qt::CaseInsensitive);
    protocolName.remove(QChar('*'),  Qt::CaseInsensitive);
    protocolName.remove(QChar('>'),  Qt::CaseInsensitive);
    protocolName.remove(QChar('<'),  Qt::CaseInsensitive);
    protocolName.remove(QChar('|'),  Qt::CaseInsensitive);
    protocolName.remove(QChar('?'),  Qt::CaseInsensitive);
    protocolName.remove(QChar('"'),  Qt::CaseInsensitive);

    currentFilename += protocolName;
    currentFilename += ".dat";

    RTI->debug("Filename selected: " + currentFilename);

    return true;
}


bool rdsRaid::setCurrentFileID()
{
    // Search current RAID entry in list
    rdsRaidEntry* entry=getRaidEntry(currentRaidIndex);

    // If not found, there is a problem, abort.
    if (entry==0)
    {
        return false;
    }

    // Store the FileID -> needed for calling the RaidTool
    currentFileID=entry->fileID;

    return true;
}


void rdsRaid::dumpRaidList(QString filename)
{
    QFile dumpFile;
    dumpFile.setFileName(RTI->getAppPath()+"/"+filename);
    if (dumpFile.exists())
    {
        dumpFile.remove();
    }
    dumpFile.open(QIODevice::ReadWrite | QIODevice::Text);

    QString line="## Raid entriues = " + QString::number(raidList.count());
    dumpFile.write(line.toLatin1());
    line="\n";
    dumpFile.write(line.toLatin1());

    for (int i=0; i < raidList.count(); i++)
    {
        rdsRaidEntry* entry=raidList.at(i);
        line="FID=" + QString::number(entry->fileID) + " MID=" + QString::number(entry->measID) + " PROT='" + entry->protName + "' PAT='" + entry->patName + "' ";
        line+= (entry->attribute==RDS_SCANATTRIBUTE_ERROR ? QString("ERROR") : QString("OK"))+"\n";
        dumpFile.write(line.toLatin1());
    }

    line=QString(RDS_RAID_DEBUG_FOOTER)+"\n";
    dumpFile.write(line.toLatin1());
    dumpFile.close();
}


void rdsRaid::dumpRaidToolOutput(QString filename)
{
    QFile dumpFile;
    dumpFile.setFileName(RTI->getAppPath()+"/"+filename);
    if (dumpFile.exists())
    {
        dumpFile.remove();
    }
    dumpFile.open(QIODevice::ReadWrite | QIODevice::Text);

    QString line = QString(RDS_RAID_DEBUG_HEADER) + QString::number(raidToolOutput.count());
    dumpFile.write(line.toLatin1());
    line="\n";
    dumpFile.write(line.toLatin1());

    for (int i=0; i < raidToolOutput.count(); i++)
    {
        line=raidToolOutput.at(i);
        dumpFile.write(line.toLatin1());
    }

    line=QString(RDS_RAID_DEBUG_FOOTER)+"\n";
    dumpFile.write(line.toLatin1());
    dumpFile.close();
}


bool rdsRaid::saveSingleFile(int fileID, bool saveAdjustments, QString modeID,
                             QString &filename, QStringList& adjustFilenames, QString paramSuffix,
                             QString cloudUUID)
{
    ortTaskID="";
    int raidCount=raidList.count();
    int raidIndex=0;
    bool scanFound=false;
    rdsRaidEntry* currentEntry=0;

    // Find the raid entry corresponding to the provided fileID, so that
    // the filename can be estimated
    for (int rc=0; rc<raidCount; rc++)
    {
        // NOTE: RAID entries are evaluated from the back
        raidIndex=rc;
        currentEntry=raidList.at(raidIndex);

        if (currentEntry->fileID==fileID)
        {
            scanFound=true;
            break;
        }
    }

    if (!scanFound)
    {
        RTI->log("ERROR: Selected FID not found in Raid list.");
        return false;
    }

    // Estimate if enough disk space is available for local export
    // Multiply by 1.2 to be on the safe side for the adjustment scans
    if ( RTI->getFreeDiskSpace() < (getRaidEntry(raidIndex)->size * 1.2) )
    {
        RTI->log("ERROR: Available disk space too small for exporting data.");
        RTI->log("ERROR: Needed    = " + QString::number(getRaidEntry(raidIndex)->size*1.2));
        RTI->log("ERROR: Available = " + QString::number(RTI->getFreeDiskSpace()));
        ortMissingDiskspace=true;
        return false;
    }
    else
    {
        ortMissingDiskspace=false;
    }

    // Estimate filename for the main measurement
    filename=getORTFilename(currentEntry,modeID,paramSuffix,cloudUUID);

    // Save file to queue directory
    if (!saveRaidFile(fileID, filename, saveAdjustments, false))
    {
        RTI->log("ERROR: Error exporting file from Raid.");
        return false;
    }

    // Now save the adjustment scans (for VB17-type systems)
    if (saveAdjustments)
    {
        currentRaidIndex=raidIndex;

        // Search for the indices of the adjustment scans
        findAdjustmentScans();

        for (int i=0; i<currentAdjustScans.count(); i++)
        {
            int indexToSave=currentAdjustScans.at(i);
            currentEntry=raidList.at(indexToSave);
            QString adjFilename=getORTFilename(currentEntry,modeID,paramSuffix,cloudUUID,fileID,i);
            adjustFilenames.append(adjFilename);

            // Save file to queue directory
            if (!saveRaidFile(currentEntry->fileID, adjFilename, false, false))
            {
                RTI->log("ERROR: Error exporting adjustment file from Raid.");
                return false;
            }
        }
    }

    return true;
}


QString rdsRaid::getORTFilename(rdsRaidEntry* entry, QString modeID, QString param, QString cloudUUID, int refID, int refIndex)
{
    QString filename="";

    filename += modeID + RTI_SEPT_CHAR;
    filename += "S" + ortSystemName + RTI_SEPT_CHAR;

    if (refID!=-1)
    {
        filename += "R" + QString::number(refID) + RTI_SEPT_CHAR;
    }

    filename += "F" + QString::number(entry->fileID) + RTI_SEPT_CHAR;
    filename += "M" + QString::number(entry->measID) + RTI_SEPT_CHAR;

    filename += "D" + entry->creationTime.toString("ddMMyy") + RTI_SEPT_CHAR;
    filename += "T" + entry->creationTime.toString("HHmmss"); //+ RTI_SEPT_CHAR;

    // Add the param value as suffix if an param is used by the mode
    if (param!="")
    {
        filename += QString(RTI_SEPT_CHAR) + "P" + param;
    }

    // NOTE: The protocol name is not attached for now because otherwise the
    //       protocol length might become too long

    // In cloud mode, the files are named by the UUID. However, the normal ORT file name
    // is still needed as taskID, so store that in a variable
    if (!cloudUUID.isEmpty())
    {
        if (refID==-1)
        {
            // Store the "normal" ORT filename from the main scanfile as TaskID
            ortTaskID=filename;
        }

        QString refSuffix="";

        // For adjustment scans, append the index of the reference
        if (refIndex>-1)
        {
            refSuffix="_"+QString::number(refIndex);
        }

        return cloudUUID+refSuffix+".dat";
    }

    filename += ".dat";

    return filename;
}


bool rdsRaid::debugReadTestFile(QString filename)
{
    bool result=false;

    QFile testFile(filename);
    if (!testFile.exists())
    {
        RTI->debug("Can't find test file.");
        return false;
    }

    if (!testFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        RTI->debug("Can't open test file.");
        return false;
    }

    raidToolOutput.clear();
    QTextStream in(&testFile);

    QString input="";
    int i=0;
    while (!in.atEnd())
    {
        input=in.readLine();
        // Skip the header line for files saved from the debug dialog
        if ((input.contains(RDS_RAID_DEBUG_HEADER)) || (input.contains(RDS_RAID_DEBUG_FOOTER)))
        {
            continue;
        }
        raidToolOutput.append(input);
        i++;
    }
    RTI->debug("Lines read from test file: "+QString::number(i));
    RTI->debug("Calling parser now.");

    ignoreLPFID=true;

    QElapsedTimer t;
    t.start();

    result=parseOutputDirectory();

    RTI->debug(QString("Parsing duration: %1 ms").arg(t.elapsed()));

    // Parse the raid output
    if (!result)
    {
        RTI->debug("Parsing file was not succesful.");
    }
    else
    {
        RTI->debug("File parsed without problems.");
    }

    ignoreLPFID=false;

    return result;
}
