#include "rds_runtimeinformation.h"

#include <QtWidgets>
#include "rds_global.h"
#include "rds_operationwindow.h"

#if (defined (WINDOWS) || defined (__windows__) || defined(Q_OS_WIN))
    #include <windows.h>
#endif

rdsRuntimeInformation* rdsRuntimeInformation::pSingleton=0;


rdsRuntimeInformation::rdsRuntimeInformation()
{
    mode=RDS_OPERATION;
    returnValue=0;
    simulation=false;
    invalidEnvironment=false;
    preventStart=false;

    simulatorExists=false;
    syngoMRExists=false;
    appPath=qApp->applicationDirPath();
    lpfiPath="";
    logInstance=0;
    configInstance=0;
    raidInstance=0;
    networkInstance=0;
    windowInstance=0;
    controlInstance=0;

    debugMode=false;

    syngoMRVersion=RDS_INVALID;

    postponementRequest=false;
    severeErrors=false;

    // Read environment variables for detection of NumarisX versions
    detectedNumarisX=false;

    if (QProcessEnvironment::systemEnvironment().value("PRODUCT_NAME","")=="Numaris/X")
    {
        detectedNumarisX=true;
        numarisXBinPath=QProcessEnvironment::systemEnvironment().value("MED_BIN","");
    }
}


rdsRuntimeInformation* rdsRuntimeInformation::getInstance()
{
    if (pSingleton==0)
    {
        pSingleton=new rdsRuntimeInformation();
    }
    return pSingleton;
}


void rdsRuntimeInformation::prepare()
{
    // Check for existance of RaidSimulator
    simulatorExists=false;
    QString myPath=qApp->applicationDirPath();
    QDir myDir(myPath);
    if (myDir.exists(RDS_RAIDSIMULATOR_NAME))
    {
        simulatorExists=true;
    }

    // Check for existance of syngoMR installation and binary path for RaidTool
    QString raidToolPath=RDS_RAIDTOOL_PATH;
    if (detectedNumarisX)
    {
        raidToolPath=numarisXBinPath;
    }

    QDir syngoDir(raidToolPath);
    syngoMRExists=false;

    if (syngoDir.exists())
    {
        if (syngoDir.exists(raidToolPath))
        {
            syngoMRExists=true;
        }
    }

    // Check syngo Version
    if (syngoMRExists)
    {
        determineSyngoVersion();
    }

    // Check if we have a valid environment
    invalidEnvironment=true;
    if (simulatorExists || syngoMRExists)
    {
        invalidEnvironment=false;
    }

    simulation=true;
    if (syngoMRExists)
    {
        simulation=false;
    }

    if (simulation)
    {
        syngoMRVersion=RDS_VB17A;
        syngoMRLine=RDS_VB;
    }

    // Initialize random generator for jittering start times
    QTime time = QTime::currentTime();
    qsrand((uint)time.msec());
}


bool rdsRuntimeInformation::prepareEnvironment()
{
    // If any prior constructors request that start of the client is prevented
    if (preventStart)
    {
        return false;
    }

    bool error=false;

    // Prepare all directories etc
    appPath=qApp->applicationDirPath();
    lpfiPath = appPath + RDS_LPFI_NAME;

    // Change directory to application path
    QDir dir;
    dir.setCurrent(appPath);

    if (appPath.contains(" "))
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Invalid Installation Path");
        msgBox.setText("The RDS tool has been installed in an invalid path. The path must not contain any space characters. Please move the program to a different location.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setWindowIcon(RDS_ICON);
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

    // Check if directory for queued files
    if (!dir.exists(RDS_DIR_QUEUE))
    {
        dir.mkdir(RDS_DIR_QUEUE);
    }

    if (!dir.exists(RDS_DIR_QUEUE))
    {
        error=true;
    }

    // Show error message because the log is not running yet
    if (error)
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Error");
        msgBox.setText("Unable to create required directories.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setWindowIcon(RDS_ICON);
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
    }

    return (!error);
}


#define RDS_SYNGODETECT(ID,IDFILE) if ((syngoMRVersion==RDS_INVALID) && (myDir.exists(IDFILE))) { syngoMRVersion=ID; }

void rdsRuntimeInformation::determineSyngoVersion()
{
    // Detect the syngo MR version by searching for existance of version files
    syngoMRVersion=RDS_INVALID;
    QDir myDir;  

    if (detectedNumarisX)
    {
        // XA line
        syngoMRVersion=determineNumarisXVersion();
    }
    else
    {
        // VB line
        RDS_SYNGODETECT(RDS_VB13A,RDS_SYNGODETECT_VB13A);
        RDS_SYNGODETECT(RDS_VB15A,RDS_SYNGODETECT_VB15A);
        RDS_SYNGODETECT(RDS_VB17A,RDS_SYNGODETECT_VB17A);
        RDS_SYNGODETECT(RDS_VB19A,RDS_SYNGODETECT_VB19A);
        RDS_SYNGODETECT(RDS_VB19B,RDS_SYNGODETECT_VB19B);

        // VB-P line
        RDS_SYNGODETECT(RDS_VB18P,RDS_SYNGODETECT_VB18P);
        RDS_SYNGODETECT(RDS_VB20P,RDS_SYNGODETECT_VB20P);

        // VD line
        RDS_SYNGODETECT(RDS_VD11A,RDS_SYNGODETECT_VD11A);
        RDS_SYNGODETECT(RDS_VD11D,RDS_SYNGODETECT_VD11D);
        RDS_SYNGODETECT(RDS_VD13A,RDS_SYNGODETECT_VD13A);
        RDS_SYNGODETECT(RDS_VD13B,RDS_SYNGODETECT_VD13B);
        RDS_SYNGODETECT(RDS_VD13C,RDS_SYNGODETECT_VD13C);
        RDS_SYNGODETECT(RDS_VD13D,RDS_SYNGODETECT_VD13D);

        // VE line
        RDS_SYNGODETECT(RDS_VE11A,RDS_SYNGODETECT_VE11A);
        RDS_SYNGODETECT(RDS_VE11B,RDS_SYNGODETECT_VE11B);
        RDS_SYNGODETECT(RDS_VE11C,RDS_SYNGODETECT_VE11C);
        RDS_SYNGODETECT(RDS_VE11U,RDS_SYNGODETECT_VE11U);
        RDS_SYNGODETECT(RDS_VE11P,RDS_SYNGODETECT_VE11P);
        RDS_SYNGODETECT(RDS_VE12U,RDS_SYNGODETECT_VE12U);
        RDS_SYNGODETECT(RDS_VE11D,RDS_SYNGODETECT_VE11D);
        RDS_SYNGODETECT(RDS_VE11E,RDS_SYNGODETECT_VE11E);
        RDS_SYNGODETECT(RDS_VE11S,RDS_SYNGODETECT_VE11S);
    }

    syngoMRLine=getSyngoMRLine();
}


int rdsRuntimeInformation::determineNumarisXVersion()
{
    if (!QFile::exists("C:\\Program Files\\Siemens\\Numaris\\MriProduct\\SoftwareState\\CurrentState.txt"))
    {
        return RDS_INVALID;
    }

    QFile stateFile("C:\\Program Files\\Siemens\\Numaris\\MriProduct\\SoftwareState\\CurrentState.txt");

    if (!stateFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return RDS_INVALID;
    }

    int detectedVersion=RDS_INVALID;

    QString buffer="";
    QTextStream stream(&stateFile);

    while (stream.readLineInto(&buffer))
    {
        if (buffer.contains("Numaris = "))
        {
            buffer.remove(0,buffer.indexOf("(")+1);
            buffer.truncate(buffer.indexOf("."));


            if (buffer=="VA10A")
            {
                detectedVersion=RDS_XA10A;
                break;
            }

            if (buffer=="VA11A")
            {
                detectedVersion=RDS_XA11A;
                break;
            }

            if (buffer=="VA11B")
            {
                detectedVersion=RDS_XA11B;
                break;
            }

            // TODO: Validate that this is still true for newer versions

            if (buffer=="VA20A")
            {
                detectedVersion=RDS_XA20A;
                break;
            }

            if (buffer=="VA20B")
            {
                detectedVersion=RDS_XA20B;
                break;
            }

            if (buffer=="VA30A")
            {
                detectedVersion=RDS_XA30A;
                break;
            }

            if (buffer=="VA31A")
            {
                detectedVersion=RDS_XA31A;
                break;
            }

            if (buffer=="VA40A")
            {
                detectedVersion=RDS_XA40A;
                break;
            }

            if (buffer=="VA50A")
            {
                detectedVersion=RDS_XA50A;
                break;
            }

            if (buffer=="VA51A")
            {
                detectedVersion=RDS_XA51A;
                break;
            }

            if (buffer=="VA60A")
            {
                detectedVersion=RDS_XA60A;
                break;
            }

            if (buffer=="VA61A")
            {
                detectedVersion=RDS_XA61A;
                break;
            }

            if (buffer=="NX2501_20251231")
            {
                detectedVersion=RDS_XB10A;
                break;
            }
        }
    }

    stateFile.close();

    return detectedVersion;
}


QString rdsRuntimeInformation::getSyngoImagerIP()
{
    QString result=RDS_IMAGER_IP_3;

    if ((syngoMRVersion==RDS_VD13A) || (syngoMRVersion==RDS_VD13B) ||
        (syngoMRVersion==RDS_VD13C) || (syngoMRVersion==RDS_VD13D) ||
        (syngoMRLine==RDS_VE) || (syngoMRLine==RDS_XA) || (syngoMRLine==RDS_XB))
    {
        result=RDS_IMAGER_IP_2;
    }

    return result;
}


qint64 rdsRuntimeInformation::getFreeDiskSpace(QString path)
{
    // If no parameter is given, then test the disk space of the
    // application path, i.e. the queueing directory
    if (path=="")
    {
        path=appPath;
    }

    QString oldDir = QDir::current().absolutePath();
    QDir::setCurrent(path);

#if (defined (WINDOWS) || defined (__windows__) || defined(Q_OS_WIN))
    ULARGE_INTEGER freeSpace,totalSpace;
    bool bRes = ::GetDiskFreeSpaceExA( 0 , &freeSpace , &totalSpace , NULL );
    if ( !bRes )
    {
        return 0;
    }
#endif
    QDir::setCurrent( oldDir );
#if (defined (WINDOWS) || defined (__windows__) || defined(Q_OS_WIN))
    return static_cast<__int64>(freeSpace.QuadPart);
#else
    #warning "Warning: Free disk space check omitted!"
    RTI->log("Running on Linux. Skipping check for free disk space.");
    return(RDS_DISKLIMIT_ALTERNATING);
#endif
}


void rdsRuntimeInformation::showOperationWindow()
{
    // Exclude UI related stuff when compiling for the ORT app.
    #ifdef YARRA_APP_RDS

    if (windowInstance==0)
    {
        return;
    }

    windowInstance->showNormal();

    #endif
}


void rdsRuntimeInformation::clearLogWidget()
{
    #ifdef YARRA_APP_RDS

    if (logInstance!=0)
    {
        logInstance->clearLogWidget();
    }

    #endif
}


void rdsRuntimeInformation::updateInfoUI()
{
    #ifdef YARRA_APP_RDS

    if (windowInstance!=0)
    {
        windowInstance->updateInfoUI();
    }

    #endif
}


void rdsRuntimeInformation::setIconWindowAnim(bool status)
{
    #ifdef YARRA_APP_RDS

    if (configInstance->infoShowIcon)
    {
        windowInstance->iconWindow.setAnim(status);
    }

    #endif
}


void rdsRuntimeInformation::debugPatchSyngoVersion(int newVersion)
{
    // Overwrite the detected SyngoMR version. This is intended only for
    // testing the parser with different dump files
    syngoMRVersion=newVersion;
    syngoMRLine=getSyngoMRLine();
}
