#ifndef RDS_NETWORK_H
#define RDS_NETWORK_H

#include <QtCore>
#include <QtNetwork>

#include <../NetLogger/netlogger.h>

#ifdef YARRA_APP_RDS
class rdsCopyDialog;
#endif


class rdsNetwork : public QObject
{
    Q_OBJECT

public:

    rdsNetwork();
    ~rdsNetwork();

    // External interface
    bool openConnection();
    void closeConnection();
    bool isConnectionActive();

    bool checkExistance(QString filename);
    bool transferFiles();


    // Internal methods
    bool isQueueEmpty();
    int getQueueCount();

    bool getFileToProcess(int index);
    bool copyFile();
    bool verifyTransfer();
    bool removeFile();
    void releaseFile();

    QString getConfigFileData();
    QString getLogFileData(int lines);
    bool copyLogFile();
    void runReconnectCmd();
    void runDisconnectCmd();

    bool setLocalBufferPath(QString bufferPath);
    bool isLocalBufferPathValid();

    NetLogger netLogger;

#ifdef YARRA_APP_RDS
    // Used by the alternating/batched update loop (rds_processcontrol.cpp)
    // to have the copy dialog reflect progress across the whole update
    // instead of resetting for each cycle's queue directory contents.
    // beginOverallTransfer()/endOverallTransfer() bracket the whole loop.
    //
    // There's no reliable total byte count to measure against - the RAID's
    // export list only knows about primary scans, not the adjustment scans
    // bundled in alongside them - so instead of tracking bytes overall,
    // each scan is simply assumed to take an equal 1/totalScans share of
    // the bar. setScanPhase() tells transferFiles() which slice the current
    // cycle occupies before it runs, and the existing per-cycle byte
    // estimate (accurate for the current cycle, since bundled adjustment
    // files are physically present in the queue directory by then) is used
    // only to animate smoothly within that slice.
    //
    // setScanProgress() is called once per cycle, after its files have
    // actually finished transferring, to advance the discrete X/Y count -
    // scan-based rather than file-based since a scan can produce more than
    // one file (adjustment scan bundling), which doesn't map cleanly onto a
    // file count known upfront.
    void beginOverallTransfer();
    void endOverallTransfer();
    void setScanPhase(int scansBeforeThisCycle, int scansThisCycle, int totalScans);
    void setScanProgress(int scansDone, int totalScans);

    // Lets a caller update the persistent dialog's scanning-allowed text
    // mid-transfer, for callers (e.g. normal mode) whose safety state
    // changes partway through an overall-tracked sequence rather than
    // staying fixed for its whole duration the way alternating mode does.
    void setScanningAllowed(bool allowed);
#endif

private:

    QDir queueDir;
    bool connectionActive;

    QStringList fileList;

    QString currentFilename;
    QString currentProt;
    qint64  currentFilesize;
    QString currentTimeStamp;

    QDir networkDrive;

#ifdef YARRA_APP_RDS
    rdsCopyDialog* copyDialog;
    qint64 estimatedBytesPerSec;
    bool hasMeasuredThroughput;
    qint64 transferTotalBytes;
    qint64 transferBytesDone;
    int transferFilesDone;

    bool overallTransferActive;
    int overallScansDone;
    int overallScansTotal;

    int phaseScansBefore;
    int phaseScansThisCycle;
    int phaseScansTotal;

    int computeDisplayPercent(qint64 bytesDone, qint64 totalBytes);
#endif
};


inline bool rdsNetwork::isConnectionActive()
{
    return connectionActive;
}



class rdsCopyThread : public QThread
{
public:
    rdsCopyThread();

    void run();

    QString sourceName;
    QString destName;
    bool success;
    bool lockError;

    bool finishedCopy;
    QFile::FileError fileError;
    QString fileErrorString;
};


#endif // RDS_NETWORK_H

