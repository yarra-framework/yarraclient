#ifndef RDS_CONFIGURATION_H
#define RDS_CONFIGURATION_H

#include <QtGui>


class rdsConfigurationProtocol
{
public:
    QString name;
    QString filter;
    bool    saveAdjustData;
    bool    anonymizeData;
    bool    smallFiles;
    bool    remotelyDefined;
};


class rdsConfiguration
{
public:
    rdsConfiguration();
    ~rdsConfiguration();

    void loadConfiguration();
    void saveConfiguration();

    bool isConfigurationValid();

    bool    infoValidityTest;

    QString infoName;
    QString infoSerialNumber;
    bool    infoShowIcon;
    bool    infoFileExplorerItem;
    QString infoScannerType;

    int     infoUpdateMode;
    int     infoUpdatePeriod;
    int     infoUpdatePeriodUnit;

    QTime   infoUpdateTime1;
    QTime   infoUpdateTime2;
    QTime   infoUpdateTime3;

    bool    infoUpdateUseTime2;
    bool    infoUpdateUseTime3;

    bool    infoJitterTimes;
    int     infoJitterWindow;

    QTime   infoUpdateTime1Jittered;
    QTime   infoUpdateTime2Jittered;
    QTime   infoUpdateTime3Jittered;

    int     infoRAIDTimeout;
    int     infoCopyTimeout;

    int     netMode;
    QString netDriveBasepath;
    QString netDriveReconnectCmd;
    QString netDriveDisconnectCmd;
    bool    netDriveCreateBasepath;
    QString netRemoteConfigFile;
    QString netRemoteLpfiFile;
    bool    netDriveStartupCmdsAfterFail;
    QString netDriveLocalBufferPath;
    double  netAlternatingDiskLimitGb;
    double  netWarningDiskLimitGb;

    QString logServerPath;
    QString logApiKey;
    bool    logSendHeartbeat;
    bool    logSendScanInfo;
    int     logUpdateFrequency;
    int     logUpdateFrequencyUnit;

    bool    mailboxEnabled;

    QStringList startCmds;

    bool    cloudSupportEnabled;

    QList<rdsConfigurationProtocol*> protocols;
    int getProtocolCount();
    void addProtocol(QString name, QString filter, bool saveAdjustData, bool anonymizeData, bool smallFiles, bool remotelyDefined);
    void updateProtocol(int index, QString name, QString filter, bool saveAdjustData, bool anonymizeData, bool smallFiles, bool remotelyDefined);
    void readProtocol(int index, QString& name, QString& filter, bool& saveAdjustData, bool& anonymizeData, bool &smallFiles, bool &remotelyDefined);
    void deleteProtocol(int index);

    bool loadRemotelyDefinedName();
    bool loadRemotelyDefinedProtocols();
    void removeRemotelyDefinedProtocols(); 

    bool protocolNeedsAnonymization(int index);

    enum
    {
        UPDATEMODE_STARTUP          =0,
        UPDATEMODE_FIXEDTIME        =1,
        UPDATEMODE_PERIODIC         =2,
        UPDATEMODE_MANUAL           =3,
        UPDATEMODE_STARTUP_FIXEDTIME=4
    };

    enum
    {
        PERIODICUNIT_MIN  =0,
        PERIODICUNIT_HOUR =1
    };

    enum
    {
        NETWORKMODE_DRIVE=0
    };

    bool isNetworkModeDrive();
    bool isLogServerConfigured();

protected:

    void loadCloudSettings();

};



inline int rdsConfiguration::getProtocolCount()
{
    return protocols.count();
}


inline bool rdsConfiguration::isNetworkModeDrive()
{
    return netMode==NETWORKMODE_DRIVE;
}


inline bool rdsConfiguration::protocolNeedsAnonymization(int index)
{
    rdsConfigurationProtocol* item=protocols.at(index);

    if (item!=0)
    {
        return (item->anonymizeData);
    }
    else
    {
        return false;
    }
}


inline bool rdsConfiguration::isLogServerConfigured()
{
    return (!logServerPath.isEmpty());
}



#endif // RDS_CONFIGURATION_H


