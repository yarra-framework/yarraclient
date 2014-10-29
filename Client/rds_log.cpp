#include "rds_log.h"
#include "rds_global.h"


rdsLog::rdsLog()
{
    logWidget=0;
}



void rdsLog::start()
{
    QString logFilename=RTI->getAppPath()+"/"+RDS_DIR_LOG+"/"+getLogFilename();

    // NOTE: For the SAC app, there are no subfolders. So create a log in the app dir
#ifdef YARRA_APP_SAC
    logFilename=qApp->applicationDirPath()+"/sac.log";
#endif

    logfile.setFileName(logFilename);
    logfile.open(QIODevice::Append | QIODevice::Text);

    if (logfile.size() > RDS_MAXLOGSIZE)
    {
        log("Size of logfile getting large.");
        log("Renaming file and creating empty file.");
        logfile.flush();

        if (!logfile.rename(logFilename + "_" + QDate::currentDate().toString("ddMMyy")))
        {
            // If the filename already exists (very unlikely), add the time to make the filename unique
            logfile.rename(logFilename + "_" + QDate::currentDate().toString("ddMMyy")+QTime::currentTime().toString("HHmmss"));
        }

        logfile.close();
        logfile.setFileName(logFilename);
        logfile.open(QIODevice::Append | QIODevice::Text);
    }

    #ifdef YARRA_APP_RDS
        log("Service started  (V "+QString(RDS_VERSION)+")");
    #endif

    if (RTI->isSimulation())
    {
        log("Service is running in simulation mode.");
    }
}


void rdsLog::finish()
{
    QString line="\n";
    logfile.write(line.toLatin1());

    logfile.close();
}


void rdsLog::pauseLogfile()
{
    logfile.flush();
    logfile.close();
}


void rdsLog::resumeLogfile()
{
    logfile.open(QIODevice::Append | QIODevice::Text);
}


void rdsLog::clearLogWidget()
{
    if (logWidget!=0)
    {
        logWidget->clear();
    }
}


QString rdsLog::getLocalLogPath()
{
    return RTI->getAppPath()+"/"+RDS_DIR_LOG+"/";
}

