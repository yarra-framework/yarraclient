#ifndef NETLOGGER_H
#define NETLOGGER_H

#include <QString>
#include <QUrlQuery>
#include <QtNetwork>
#include "netlog_events.h"
#include <functional>

class NetLogger
{
public:

    NetLogger();
    ~NetLogger();

    void configure(QString path, EventInfo::SourceType sourceType, QString sourceId, QString key, bool skipDomainValidation=false);
    bool isConfigured();
    bool isConfigurationError();
    bool isServerInSameDomain(QString serverPath);
    bool retryDomainValidation();

    QNetworkReply* postDataAsync(QUrlQuery query, QString endpt);
    QUrlQuery buildEventQuery(EventInfo::Type type, EventInfo::Detail detail, EventInfo::Severity severity, QString info, QString data);

    bool postData(QUrlQuery query, QString endpt, QNetworkReply::NetworkError& error, int &http_status, QString &errorString, int timeoutMsec=NETLOG_POST_TIMEOUT);
    void postEvent    (EventInfo::Type type, EventInfo::Detail detail=EventInfo::Detail::Information, EventInfo::Severity severity=EventInfo::Severity::Success, QString info=QString(""), QString data=QString(""));
    bool postEventSync(EventInfo::Type type, EventInfo::Detail detail=EventInfo::Detail::Information, EventInfo::Severity severity=EventInfo::Severity::Success, QString info=QString(""), QString data=QString(""), int timeoutMsec=NETLOG_EVENT_TIMEOUT);
    bool postEventSync(QNetworkReply::NetworkError& error, int& status_code, EventInfo::Type type, EventInfo::Detail detail=EventInfo::Detail::Information, EventInfo::Severity severity=EventInfo::Severity::Success, QString info=QString(""), QString data=QString(""), int timeoutMsec=NETLOG_POST_TIMEOUT);

//    template <typename F>
    bool doRequest(QString endpoint, const std::function<void(QNetworkReply*)> fn, int timeout = 2000);
//    template <typename F>
    bool doRequest(QString endpoint, QUrlQuery, const std::function<void(QNetworkReply*)> fn, int timeout = 2000);

    static QString dnsLookup(QString address);
    QNetworkAccessManager* networkManager;
    bool waitForReply(QNetworkReply* reply, int timeout);
protected:

    bool configured;
    bool configurationError;

    QString serverPath;
    QString apiKey;
    QString source_id;
    EventInfo::SourceType source_type;

};


inline bool NetLogger::isConfigured()
{
    return configured;
}


inline bool NetLogger::isConfigurationError()
{
    return configurationError;
}


#endif // NETLOGGER_H
