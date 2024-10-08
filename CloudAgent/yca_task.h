#ifndef YCATASK_H
#define YCATASK_H

#include <QWidget>
#include <QDateTime>
#include <QMutex>


class yctAPI;

class ycaTask
{    
public:

    enum TaskStatus
    {
        tsInvalid=-1,
        tsPreparing=0,     // SAC/ORT are currently preparing the job
        tsScheduled,       // Job is scheduled for upload to cloud
        tsProcessing,      // Job is currently being processed, i.e. residing in the cloud (or upoladed/downloaded)
        tsUploading,       // Job is currently being uploaded (only relevant is called outside of worker thread)
        tsRunning,         // Job is running/scheduled in the cloud
        tsReady,           // Job is finished and awaiting download
        tsDownloading,     // Job is currently being uploaded (only relevant is called outside of worker thread)
        tsStorage,         // Results are being stored
        tsArchived,        // Job resides in the archive
        tsErrorTransfer,   // Error occured on client side
        tsErrorProcessing, // Error occured on the cloud side
        tsErrorStorage     // Error occured on client side
    };

    enum TaskResult
    {
        trInProcess=-1,
        trSuccess=0,
        trAbortedTransfer,
        trAbortedProcessing,
        trAbortedStorage
    };

    enum WorkerProcess
    {
        wpIdle=0,
        wpUpload,
        wpDownload,
        wpStorage
    };

    ycaTask();

    TaskStatus  status;
    TaskResult  result;
    QString     getStatus();
    QString     getResult();

    QString     patientName;
    QString     uuid;
    QString     reconMode;
    QString     shortcode;

    QString     mrn;
    QString     dob;
    QString     acc;
    QString     taskID;

    QString     taskFilename;
    QString     phiFilename;
    QStringList twixFilenames;

    QDateTime   retryDelay;
    int         uploadRetry;
    int         downloadRetry;
    int         storageRetry;

    double      cost;
    int         datasize;

    QDateTime   timeptCreated;
    QDateTime   timeptCompleted;
    QDateTime   timeptUploadBegin;
    QDateTime   timeptUploadEnd;
    QDateTime   timeptProcessingCreated;
    QDateTime   timeptProcessingBegin;
    QDateTime   timeptProcessingEnd;
    QDateTime   timeptDownloadBegin;
    QDateTime   timeptDownloadEnd;
    QDateTime   timeptStorageBegin;
    QDateTime   timeptStorageEnd;

};

typedef QList<ycaTask*> ycaTaskList;


class ycaTaskHelper
{
public:

    ycaTaskHelper();
    void setCloudInstance(yctAPI* apiInstance);
    yctAPI* cloud;

    bool getScheduledTasks(ycaTaskList& taskList);
    bool getProcessingTasks(ycaTaskList& taskList);
    bool getAllTasks(ycaTaskList& taskList, bool includeCurrent, bool includeArchive, QString workerJobID="", ycaTask::WorkerProcess workerOperation=ycaTask::wpIdle);
    bool checkScanfiles(QString taskID, ycaTask* task);    
    bool readPHIData(QString filepath, ycaTask* task);
    bool saveResultToPHI(QString filepath, ycaTask::TaskResult result);
    bool saveCostsToPHI(ycaTaskList& taskList);
    bool saveTimepoint(ycaTask* task, QString timepointID, QMutex* mutex=0);

    void getTasksForDownloadArchive(ycaTaskList& taskList, ycaTaskList& downloadList, ycaTaskList& archiveList);
    bool archiveTasks(ycaTaskList& archiveList, QString& notificationString);
    void clearTaskList(ycaTaskList& list);

    bool storeTasks(ycaTaskList& archiveList, QMutex* mutex, QObject* notificationWidget=0);

    bool removeIncompleteDownloads();

};


#endif // YCATASK_H
