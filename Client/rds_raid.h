#ifndef RDS_RAID_H
#define RDS_RAID_H

#include <QtWidgets>

#include "rds_global.h"


#define RDS_SCANATTRIBUTE_UNKNOWN      -1
#define RDS_SCANATTRIBUTE_ERROR         0
#define RDS_SCANATTRIBUTE_OK            1
#define RDS_SCANATTRIBUTE_USERCANCEL    3


#define RDS_VERBOSEATTRIBUTE_ERROR      "0000"
#define RDS_VERBOSEATTRIBUTE_OK         "0001"
#define RDS_VERBOSEATTRIBUTE_USERCANCEL "0003"


class rdsRaidEntry
{
public:
    int       fileID;
    int       measID;
    QString   protName;
    QString   patName;
    qint64    size;
    qint64    sizeOnDisk;
    QDateTime creationTime;
    QDateTime closingTime;
    int       attribute;

    void addToUrlQuery(QUrlQuery& query);
};


class rdsExportEntry
{
public:
    int     raidIndex;
    int     protIndex;
    QString protName;
    bool    anonymize;
    bool    adjustmentScans;
};


class rdsRaid : public QObject
{
    Q_OBJECT

public:
    rdsRaid();
    ~rdsRaid();

    bool createExportList();
    bool processTotalExportList();
    bool processExportListEntry();
    bool processExportListBatch(qint64 maxBatchBytes);
    bool exportsAvailable();
    qint64 getExportListTotalSize();
    int getExportListCount();

    void dumpRaidList(QString filename);
    void dumpRaidToolOutput(QString filename);

    void chopDependingIDs(QString& text);
    void chopDependingIDsVerbose(QString& text, int& scanAttribute, QString& unknownAttribute);

    bool isPatchedRaidToolMissing();

    bool readRaidList();
    bool saveRaidFile(int fileID, QString filename, bool saveAdjustments=false, bool anonymize=false);

    bool findAdjustmentScans();

    QDir queueDir;
    QList<rdsRaidEntry*> raidList;
    QList<int> currentAdjustScans;

    int     currentRaidIndex;
    int     currentFileID;
    QString currentFilename;

    void setIgnoreLPFI();

    // For ORT-use only
    bool saveSingleFile(int fileID,  bool saveAdjustments, QString modeID,
                        QString &filename, QStringList& adjustFilenames, QString paramSuffix, QString cloudUUID="");

    bool    ortMissingDiskspace;
    QString ortTaskID;
    QString ortSystemName;

    void setORTSystemName(QString name);

    bool isScanActive();

    void readLPFI();
    void saveLPFI();

    int  getLPFIScaninfo();
    void setLPFIScaninfo(int value);

    bool debugReadTestFile(QString filename);

    bool setLocalBufferPath(QString bufferPath);
    bool isLocalBufferPathValid();


protected:

    bool parseVB15Line(QString line, rdsRaidEntry* entry);
    bool parseOutputDirectory();
    bool parseOutputFileExport();
    bool exportScanFromList();

    bool setCurrentFileID();
    bool setCurrentFilename(int refID=-1);

    bool anonymizeCurrentFile();

    int  getLPFI();
    void setLPFI(int value);

    bool callRaidTool(QStringList command, QStringList options, int timeout=RDS_PROC_TIMEOUT);

    QStringList raidToolOutput;

    QString raidToolCmd;
    QString raidToolIP;

    QList<rdsExportEntry*> exportList;

    int lastProcessedFileID;
    int lastProcessedFileIDScaninfo;

    void clearRaidList();
    void clearExportList();

    rdsExportEntry* getFirstExportEntry();
    rdsRaidEntry*   getFirstRaidEntry();
    rdsRaidEntry*   getRaidEntry(int index);

    void addExportEntry(int raidIndex, int protIndex, QString protName, bool anonymize, bool adjustmentScans);
    void addRaidEntry(int fileID, int measID, QString protName, QString patName, qint64 size, qint64 sizeOnDisk, QDateTime creationTime, int attribute=RDS_SCANATTRIBUTE_OK);
    void addRaidEntry(rdsRaidEntry* source);

    void removePrecedingSpace(QString& text);

    bool usePatchedRaidTool;
    bool patchedRaidToolMissing;

    bool ignoreLPFID;
    bool scanActive;

    bool useVerboseMode;
    bool missingVerboseData;

    QString getORTFilename(rdsRaidEntry* entry, QString modeID, QString param, QString cloudUUID, int refID=-1, int refIndex=-1);

};


inline int rdsRaid::getLPFI()
{
    return lastProcessedFileID;
}


inline void rdsRaid::setLPFI(int value)
{
    RTI->debug("Setting LPFI to " + QString::number(value));
    lastProcessedFileID=value;
}


inline int rdsRaid::getLPFIScaninfo()
{
    return lastProcessedFileIDScaninfo;
}


inline void rdsRaid::setLPFIScaninfo(int value)
{
    lastProcessedFileIDScaninfo=value;
}


inline void rdsRaid::clearRaidList()
{
    // Dispose all entries in the raid list
    while (!raidList.isEmpty())
    {
        delete raidList.takeFirst();
    }
}


inline void rdsRaid::clearExportList()
{
    // Dispose all entries in the export work list
    while (!exportList.isEmpty())
    {
        delete exportList.takeFirst();
    }
}


inline rdsRaidEntry* rdsRaid::getRaidEntry(int index)
{
    if ((index<0) || (index>=raidList.count()))
    {
        RTI->log("WARNING: Invalid raid index requested (Index = "+QString::number(index)+")");
        return 0;
    }

    return raidList.at(index);
}


inline rdsExportEntry* rdsRaid::getFirstExportEntry()
{
    if (exportList.count()==0)
    {
        return 0;
    }

    return exportList.at(0);
}


inline rdsRaidEntry* rdsRaid::getFirstRaidEntry()
{
    // Get the scan from the front of the export list
    rdsExportEntry* exportEntry=getFirstExportEntry();

    if (exportEntry==0)
    {
        // Something is wrong, probably list is empty
        return 0;
    }

    return getRaidEntry(exportEntry->raidIndex);
}


inline void rdsRaid::addExportEntry(int raidIndex, int protIndex, QString protName, bool anonymize, bool adjustmentScans)
{
    rdsExportEntry* entry=new rdsExportEntry;

    entry->raidIndex=raidIndex;
    entry->protIndex=protIndex;
    entry->protName=protName;
    entry->anonymize=anonymize;
    entry->adjustmentScans=adjustmentScans;

    exportList.append(entry);
}


inline void rdsRaid::addRaidEntry(int fileID, int measID, QString protName, QString patName, qint64 size, qint64 sizeOnDisk, QDateTime creationTime, int attribute)
{
    rdsRaidEntry* entry=new rdsRaidEntry;

    entry->fileID=fileID;
    entry->measID=measID;
    entry->protName=protName;
    entry->patName=patName;
    entry->size=size;
    entry->sizeOnDisk=sizeOnDisk;
    entry->creationTime=creationTime;
    entry->attribute=attribute;

    raidList.append(entry);
}


inline void rdsRaid::addRaidEntry(rdsRaidEntry* source)
{
    rdsRaidEntry* entry=new rdsRaidEntry;

    entry->fileID=source->fileID;
    entry->measID=source->measID;
    entry->protName=source->protName;
    entry->patName=source->patName;
    entry->size=source->size;
    entry->sizeOnDisk=source->sizeOnDisk;
    entry->creationTime=source->creationTime;
    entry->closingTime=source->closingTime;
    entry->attribute=source->attribute;

    raidList.append(entry);
}


inline void rdsRaid::removePrecedingSpace(QString& text)
{
    while ((text.length()!=0) && (text.at(0)==' '))
    {
        text.remove(0,1);
    }
}


inline void rdsRaid::chopDependingIDs(QString& text)
{
    int length=text.length();

    if (length==0)
    {
        return;
    }    

    int colon=text.lastIndexOf(":");

    if (colon != length-3)
    {
        text.chop(length-(colon+3));
    }
}


inline void rdsRaid::chopDependingIDsVerbose(QString& text, int& scanAttribute, QString& unknownAttribute)
{
    scanAttribute=RDS_SCANATTRIBUTE_OK;

    int length=text.length();
    int colonPos =text.lastIndexOf(":");
    int attribPos=colonPos+20;

    if ((length==0) || (colonPos==-1))
    {
        return;
    }

    // Check if the line has the verbose format
    if (text.length()>attribPos)
    {
        QString attrSubString=text.mid(colonPos+5,16);

        // Check for OK first. This will apply to most entry and avoids
        // running two compares.
        if (attrSubString.endsWith(RDS_VERBOSEATTRIBUTE_OK))
        {
            scanAttribute=RDS_SCANATTRIBUTE_OK;
        }
        else
        {
            if (attrSubString.endsWith(RDS_VERBOSEATTRIBUTE_ERROR))
            {
                scanAttribute=RDS_SCANATTRIBUTE_ERROR;
            }
            else
            {
                if (attrSubString.endsWith(RDS_VERBOSEATTRIBUTE_USERCANCEL))
                {
                    scanAttribute=RDS_SCANATTRIBUTE_USERCANCEL;
                }
                else
                {
                    scanAttribute=RDS_SCANATTRIBUTE_UNKNOWN;
                    unknownAttribute=attrSubString;
                }
            }
        }
    }
    else
    {
        missingVerboseData=true;
    }

    // Chop of the tail of the entry 3 chars after the last ":" of the closing date
    if (colonPos != length-3)
    {
        text.chop(length-(colonPos+3));
    }
}


inline bool rdsRaid::isPatchedRaidToolMissing()
{
    return patchedRaidToolMissing;
}


inline void rdsRaid::setIgnoreLPFI()
{
    ignoreLPFID=true;
}


inline void rdsRaid::setORTSystemName(QString name)
{
    ortSystemName=name;
}


inline bool rdsRaid::isScanActive()
{
    return scanActive;
}


#endif // RDS_RAID_H
