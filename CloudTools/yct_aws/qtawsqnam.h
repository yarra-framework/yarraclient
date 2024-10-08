#ifndef QTAWSQNAM_H
#define QTAWSQNAM_H

#include <QtNetwork/QNetworkAccessManager>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>


class SlottetNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
public slots:
    QNetworkReply *sendCustomRequest_slot(const QNetworkRequest &request, const QByteArray &verb,
                                          QIODevice *data = 0);
};


class ThreadsafeBlockingNetworkAccesManager : public QObject
{
    Q_OBJECT
public:
    ThreadsafeBlockingNetworkAccesManager();
    ~ThreadsafeBlockingNetworkAccesManager();
    QNetworkReply *sendCustomRequest(const QNetworkRequest &request, const QByteArray &verb,
                                     QIODevice *data = 0);
    void waitForAll();
    int pendingRequests();

public slots:
    void wakeWaitingThreads();
    void cancelAll();

private:
    QNetworkAccessManager *m_networkAccessManager;
    QThread *m_networkThread;
    QMutex m_mutex;
    QWaitCondition m_waitCompleted;
    QWaitCondition m_waitAll;
    int m_requestCount;
    bool m_cancellAll;
};


#endif

