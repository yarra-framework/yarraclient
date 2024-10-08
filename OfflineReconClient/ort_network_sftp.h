#ifndef ORTNETWORKSFTP_H
#define ORTNETWORKSFTP_H

#include "ort_network.h"
#include <QObject>
#include "ort_remotefilehelper.h"

class ortNetworkSftp : public ortNetwork
{
    Q_OBJECT
    ortRemoteFileHelper helper;

public:
    ortNetworkSftp();
    bool openConnection(bool fallback);
    bool copyFile();
    bool verifyTransfer();
    bool prepare();
    bool doReconnectServerEntry(ortServerEntry *selectedEntry);
    bool syncServerList();
    QSettings* readModelist(QString &error);
};

#endif // ORTNETWORKSFTP_H
