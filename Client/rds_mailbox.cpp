#include "rds_mailbox.h"
#include <QGuiApplication>
#include "rds_network.h"


rdsMailboxMessage::rdsMailboxMessage()
{
}


rdsMailboxMessage::rdsMailboxMessage(QString id, QString content)
{
    this->id=id;
    this->content;
}


rdsMailboxMessage::rdsMailboxMessage(QJsonObject obj)
{
    id=obj.value("message_id").toString();
    content=obj.value("content").toString();
}



rdsMailbox::rdsMailbox(QObject *parent) : QObject(parent)
{
}


void rdsMailbox::start()
{
    if (!RTI_NETLOG.isConfigured())
    {
        qDebug() << "netlog not configured";
        return;
    }
    QObject::connect(&timer, &QTimer::timeout, this, &rdsMailbox::updateMailbox);
    startChecking();
}


void rdsMailbox::startChecking()
{
    qDebug() << "start checking";
    timer.start(1000);
}


void rdsMailbox::stopChecking()
{
    qDebug() << "stop checking";
    timer.stop();
}


void rdsMailbox::updateMailbox()
{
    qDebug()<<"updateMailbox called";

    RTI_NETLOG.doRequest("get_unread_messages", [this](QNetworkReply* reply)
    {
        startChecking();

        if (reply->error() != QNetworkReply::NetworkError::NoError)
        {
            qDebug()<< "Network error:" << reply->errorString();
            return;
        }
        auto result = reply->readAll();
        QJsonParseError err;
        QJsonDocument js = QJsonDocument::fromJson(result, &err);
        if (err.error != QJsonParseError::NoError)
        {
            qDebug() << err.errorString();
            return;
        }
        QJsonArray unread_messages = js.object().value("unread_messages").toArray();
        if (unread_messages.count() == 0 )
        {
            qDebug()<<"No unread messages";
            return;
        }
        showMessage(rdsMailboxMessage(unread_messages.at(0).toObject()));
    });
}


void rdsMailbox::windowClosing(QString button)
{
    qDebug()<< "Window closing" << button;
    QUrlQuery query;
    query.addQueryItem("response", button);
    bool did = RTI_NETLOG.doRequest(QString("mark_message_as_read/") + currentMessage.id, query, [this](QNetworkReply* reply)
        {
            if (reply->error() == QNetworkReply::NetworkError::NoError)
            {
                startChecking();
                qDebug() << "Marked message response" << reply->readAll();
                mailboxWindow->close();
                return;
            }
            mailboxWindow = new rdsMailboxWindow();
            mailboxWindow->setError("Network error while responding to message");
            connect(mailboxWindow, &rdsMailboxWindow::closing, this, [this](QString _){ this->startChecking(); });
            mailboxWindow->show();
        }
    );

    if (did)
    {
        stopChecking();
    }
}


void rdsMailbox::showMessage(rdsMailboxMessage message)
{
    qDebug()<< message.id;
    stopChecking();
    mailboxWindow = new rdsMailboxWindow();
    mailboxWindow->setMessage(message.content);
    currentMessage = message;
    connect(mailboxWindow, &rdsMailboxWindow::closing, this, &rdsMailbox::windowClosing);
    mailboxWindow->show();
}

