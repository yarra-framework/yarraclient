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

