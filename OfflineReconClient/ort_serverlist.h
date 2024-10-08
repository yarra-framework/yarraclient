#ifndef ORT_SERVERLIST_H
#define ORT_SERVERLIST_H

#include <QtCore>

class ortServerEntry
{
public:
    QString     name;
    QStringList type;
    QString     connectCmd;
    QString     serverPath;
    QString     hostKey;
    bool        acceptsUndefined;
};


class ortServerList
{
public:
    ortServerList();
    ~ortServerList();

    QList<ortServerEntry*> servers;
    QList<ortServerEntry*> matchingServers;

    void clearList();
    bool syncServerList(QString remotePath);
    bool removeLocalServerList();
    bool readLocalServerList();
    bool readServerList(QString filePath);
    bool isServerListAvailable();
    ortServerEntry* getServerEntry(QString name);

    int findMatchingServers(QString type);
    ortServerEntry* getNextMatchingServer();

    void getLoadBalancingIndex(QString type);

protected:

    bool    serverListAvailable;
    QString appPath;

    int selectedIndex;

};


inline bool ortServerList::isServerListAvailable()
{
    return serverListAvailable;
}



#endif // ORT_SERVERLIST_H
