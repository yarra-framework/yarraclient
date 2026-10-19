#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <stdio.h>

#include "yct_twix_anonymizer.h"
#include "yct_twix_header.h"
#include "../yct_common.h"


#define MAX_LINE_LENGTH 65536

#define LOG(x) {std::cout << x << std::endl;}
#define DBG(x) if (debug) {std::cout << x << std::endl;}


yctTWIXAnonymizer::yctTWIXAnonymizer()
{
    errorReason="";
    debug=false;
    testing=false;
    dumpProtocol=false;
    showOnlyInfo=false;

    fileVersion=UNKNOWN;
    strictVersionChecking=true;
    versionStringSeen=false;
    readAdditionalPatientInformation=false;
}


bool yctTWIXAnonymizer::processFile(QString twixFilename, QString phiPath,
                                    QString acc, QString taskid, QString uuid, QString mode,
                                    bool storePHI)
{
    bool result=false;
    versionStringSeen=false;

    // Clean the patient data - to be sure
    patientInformation.name       ="";
    patientInformation.mrn        ="";
    patientInformation.dateOfBirth="";

    // Populate the externally provided information
    patientInformation.uuid       =uuid;
    patientInformation.acc        =acc;
    patientInformation.taskid     =taskid;
    patientInformation.mode       =mode;

    QFile file(twixFilename);

    if (testing)
    {
        if (!file.open(QIODevice::ReadOnly))
        {
            errorReason="Unable to open raw-data file for reading";
            return false;
        }
    }
    else
    {
        if (!file.open(QIODevice::ReadWrite))
        {
            errorReason="Unable to open raw-data file for read/write";
            return false;
        }
    }

    // Determine TWIX file type: VA/VB or VD/VE?
    uint32_t x[2];
    file.read((char*)x, 2*sizeof(uint32_t));

    if ((x[0]==0) && (x[1]<=64))
    {
        fileVersion=VDVE;
        DBG("File type: VD/VE");
    }
    else
    {
        fileVersion=VAVB;
        DBG("File type: VA/VB");
    }
    file.seek(0);

    if (dumpProtocol)
    {
        QString dumpFilename=twixFilename+".prot";

        dumpFile.setFileName(dumpFilename);
        if (!dumpFile.open(QIODevice::ReadWrite | QIODevice::Text))
        {
            LOG("ERROR: Unable to create dump file " << dumpFilename.toStdString());
            return false;
        }
    }

    // If VD/VE format, there can be multiple measurements stored in the same file
    if (fileVersion==VDVE)
    {
        uint32_t id=0, ndset=0;
        std::vector<VD::EntryHeader> veh;

        file.read((char*)&id,   sizeof(uint32_t));  // ID
        file.read((char*)&ndset,sizeof(uint32_t));  // # data sets

        DBG("");
        DBG("Contained datasets: " << ndset);
        DBG("");

        if ((ndset>30) || (ndset<1))
        {
            // If there are more than 30 measurements, it's unlikely that the
            // file is a valid TWIX file
            LOG("WARNING: Number of measurements in file " << ndset);

            errorReason="File is invalid (invalid number of measurements)";
            file.close();
            return false;
        }

        veh.resize(ndset);

        if (showOnlyInfo)
        {
            LOG("Format: VD/VE line");
            LOG("Protocols: " << ndset);
        }

        // Remove patient name from index block
        for (size_t i=0; i<ndset; ++i)
        {
            qint64 startPos=file.pos();
            file.read((char*)&veh[i], VD::ENTRY_HEADER_LEN);            
            qint64 endPos=file.pos();

            if (showOnlyInfo)
            {
                LOG("- " << veh.at(i).ProtocolName);
            }

            DBG(veh.at(i).PatientName);
            DBG(veh.at(i).ProtocolName);

            // Clean the patient name
            if (!testing)
            {
                // Wipe the whole array to be sure
                memset(veh.at(i).PatientName,0,64);
                strcpy(veh.at(i).PatientName,"YARRAYARRA");
                file.seek(startPos);
                file.write((char*)&veh[i], VD::ENTRY_HEADER_LEN);

                // Check whether writing the file was successful
                if (file.pos() != endPos)
                {
                    LOG("WARNING: File inconsistency detected while cleaning measurement table!");
                }
            }
        }

        if (showOnlyInfo)
        {
            return true;
        }
        DBG("");

        // Process the individual measurements in the file
        for (size_t i=0; i<ndset; ++i)
        {
            // Jump to ith measurement
            file.seek(veh.at(i).MeasOffset);
            result=processMeasurement(&file);

            if (!result)
            {
                LOG("ERROR: Error while processing section " << i);
                break;
            }
        }

        // TODO: Reiterate over all lines and double check for the PHI
    }
    else
    {
        if (showOnlyInfo)
        {
            LOG("Format: VB line");
            LOG("Protocols: 1");
            LOG("- Only main scan data");
            return true;
        }

        result=processMeasurement(&file);
    }    

    file.close();    

    if (dumpProtocol)
    {
        dumpFile.close();
    }

    if (strictVersionChecking && !versionStringSeen)
    {
        LOG("ERROR: Version string not found. File invalid.");
        return false;
    }

    if (storePHI)
    {
        if (!checkAndStorePatientData(twixFilename, phiPath))
        {
            return false;
        }
    }

    return result;
}


bool yctTWIXAnonymizer::processMeasurement(QFile* file)
{
    uint32_t headerLength=0;
    qint64   headerEnd   =0;
    qint64   headerStart=file->pos();

    // Find header length
    file->read((char*)&headerLength, sizeof(uint32_t));

    if ((headerLength<=0) || (headerLength>5000000))
    {
        // File header is invalid
        std::string fileType="VB";
        if (fileVersion==VDVE)
        {
            fileType="VD/VE";
        }
        LOG("WARNING: Unusual header size " << headerLength << " (file type " << fileType << ")");
        errorReason="File is invalid (unusual header size)";
        return false;
    }

    // Parse header
    DBG("Header size is " << headerLength << " bytes.");
    DBG("");

    headerEnd=headerStart + (qint64) headerLength;

    QByteArray line="";
    qint64     lineStart=0;
    qint64     lineEnd  =0;
    bool       isLineValid=false;
    bool       waitingForSensitiveData=false;
    bool       terminateParsing=false;
    int        charsToRead=0;
    //int      maxRead=0;

    while ((!file->atEnd()) && (file->pos() < headerEnd) && (!terminateParsing))
    {        
        isLineValid=false;
        lineStart=file->pos();
        charsToRead=MAX_LINE_LENGTH;
        if (lineStart+charsToRead>=headerEnd)
        {
            charsToRead=headerEnd-lineStart;
        }

        if (charsToRead>0)
        {
            line=file->readLine(charsToRead+1);

            if (dumpProtocol)
            {
                dumpFile.write(line);
            }

            //LOG("READ: " << line.size());

            if (line.size()<MAX_LINE_LENGTH)
            {
                isLineValid=true;

                /*
                if (line.size()>maxRead)
                {
                    maxRead=line.size();
                }
                */
            }
            else
            {
                LOG("ERROR: Incomplete line read (exceeds buffer size)");
                LOG("ERROR: Anonymization not possible. Report error to Yarra team.");
                //LOG("Chopped line: charsToRead=" << charsToRead << " actual: " << line.size());
                //LOG(line.data())
                return false;
            }
        }
        else
        {
            terminateParsing=true;
            //DBG("Stopped at position " + QString::number(lineEnd));
        }
        lineEnd=file->pos();

        int result=NO_SENSITIVE_INFORMATION;

        if (isLineValid)
        {
            if (waitingForSensitiveData)
            {
                // It is expected that this line contains sensitive information following a line break
                result=analyzeFollowLine(&line);

                if (result==PROCESSING_ERROR)
                {
                    LOG("Error while processing follow line:");
                    LOG(line.data());
                    return false;
                }

                if ((result==SENSITIVE_INFORMATION_CLEARED) || (result==NO_SENSITIVE_INFORMATION))
                {
                    // Sensitive information cleared, continue normally for next line
                    waitingForSensitiveData=false;
                }
            }
            else
            {
                // Process line
                result=analyzeLine(&line);

                if (result==PROCESSING_ERROR)
                {
                    LOG("Error while processing line:");
                    LOG(line.data());
                    return false;
                }

                if ((result==SENSITIVE_INFORMATION_FOLLOWS) || (result==SENSITIVE_INFORMATION_CLEARED_FOLLOWS))
                {
                    // Sensitive information follows in the next lines
                    waitingForSensitiveData=true;
                }
            }

            if ((result==SENSITIVE_INFORMATION_CLEARED) || (result==SENSITIVE_INFORMATION_CLEARED_FOLLOWS))
            {
                // Line contains information that has been anonymized.
                // Write anonymized line back to file.
                if (!testing)
                {
                    file->seek(lineStart);
                    file->write(line);
                }

                // Check whether writing the file was successful
                if (file->pos() != lineEnd)
                {
                    LOG("WARNING: File inconsistency detected during anonymization!");
                    LOG("WARNING: Anonymization might not be complete. Discard file.");
                }
            }
        }

        if ((lineEnd>=headerEnd) || (file->atEnd()))
        {
            terminateParsing=true;
            //DBG("Stopped at position " + QString::number(lineEnd));
            //DBG("Line start is " + QString::number(lineStart));
        }
    }

    //LOG("INFO: Max line size = " << maxRead);
    return true;
}


bool yctTWIXAnonymizer::checkAndStorePatientData(QString twixFilename, QString phiPath)
{
    DBG("");
    DBG("Patient name:  " + patientInformation.name.toStdString());
    DBG("Date of birth: " + patientInformation.dateOfBirth.toStdString());
    DBG("MRN:           " + patientInformation.mrn.toStdString());
    DBG("ACC:           " + patientInformation.acc.toStdString());
    DBG("UUID:          " + patientInformation.uuid.toStdString());
    DBG("TaskID:        " + patientInformation.taskid.toStdString());
    DBG("Mode:          " + patientInformation.mode.toStdString());
    DBG("");    

    QDir phiDir(phiPath);
    if (!phiDir.exists())
    {
        LOG("ERROR: Unable to access phi path " << phiPath.toStdString());
        return false;
    }

    QFileInfo twixFile(twixFilename);
    QString phiFilename=phiPath+"/"+twixFile.completeBaseName()+".phi";

    if (QFile::exists(phiFilename))
    {
        LOG("WARNING: PHI file already exists. Keeping existing file.");
        return false;
    }

    // If patient information has not been found in the file, set it to a recognizable value
    if (patientInformation.name.isEmpty())
    {
        patientInformation.name="MISSING";
    }
    if (patientInformation.mrn.isEmpty())
    {
        patientInformation.mrn="MISSING";
    }
    if (patientInformation.dateOfBirth.isEmpty())
    {
        patientInformation.dateOfBirth="MISSING";
    }

    QSettings phiFile(phiFilename, QSettings::IniFormat);
    phiFile.setValue("PHI/NAME",   patientInformation.name);
    phiFile.setValue("PHI/DOB",    patientInformation.dateOfBirth);
    phiFile.setValue("PHI/MRN",    patientInformation.mrn);
    phiFile.setValue("PHI/ACC",    patientInformation.acc);
    phiFile.setValue("PHI/UUID",   patientInformation.uuid);
    phiFile.setValue("PHI/TASKID", patientInformation.taskid);
    phiFile.setValue("PHI/MODE",   patientInformation.mode);

    phiFile.setValue(YCT_TIMEPT_CREATED,QDateTime::currentDateTime().toString(Qt::ISODate));

    return true;
}


int yctTWIXAnonymizer::analyzeLine(QByteArray* line)
{
    if (line->indexOf("<ParamString.\"tPatientName\">", 0) >= 0)
    {
        expectedContent=CONTENT_NAME;
        return clearLine(line);
    }

    if (line->indexOf("<ParamString.\"PatientName\">", 0) >= 0)
    {
        expectedContent=CONTENT_NAME;
        return clearLine(line);
    }

    if (line->indexOf("<ParamString.\"PatientsName\">", 0) >= 0)
    {
        expectedContent=CONTENT_NAME;
        return clearLine(line);
    }

    if (line->indexOf("<ParamString.\"PatientBirthDay\">", 0) >= 0)
    {
        expectedContent=CONTENT_BIRTHDAY;
        return clearLine(line);
    }

    if (line->indexOf("<ParamString.\"PatientID\">", 0) >= 0)
    {
        expectedContent=CONTENT_ID;
        return clearLine(line);
    }

    if (line->indexOf("<ParamString.\"Patient\">", 0) >= 0)
    {
        expectedContent=CONTENT_ID;
        return clearLine(line);
    }

    if (line->indexOf("<ParamString.\"tPerfPhysiciansName\">", 0) >= 0)
    {
        expectedContent=CONTENT_REFPHYSICIAN;
        return clearLine(line);
    }

    if (line->indexOf("<ParamString.\"SoftwareVersions\">", 0) >= 0)
    {
        expectedContent=CONTENT_VERSIONSTRING;
        return clearLine(line);
    }

    if (line->indexOf("<ParamArray.\"PatientRegistrationData\">", 0) >= 0)
    {
        expectedContent=CONTENT_PATIENTREGISTRATION;
        return clearLine(line);
    }

    if (   (line->indexOf("HEADER.tPatientName")            >= 0)
        || (line->indexOf("IRIS.RECOMPOSE.tPatientName")    >= 0)
        || (line->indexOf("IRIS.RECOMPOSE.PatientBirthDay") >= 0)
        || (line->indexOf("IRIS.RECOMPOSE.PatientID")       >= 0))
    {
        // These two entries don't contain the PHI
        return NO_SENSITIVE_INFORMATION;
    }

    if (   (line->indexOf("PatientName")             >= 0)
        || (line->indexOf("PatientsName")            >= 0)
        || (line->indexOf("BirthDay")                >= 0)
        || (line->indexOf("PatientID")               >= 0)
        || (line->indexOf("PatientRegistrationData") >= 0))
    {
        // If unexpected fields with the patient name occur, raise an error
        return PROCESSING_ERROR;
    }

    // Anonymize patient age by replacing decimal digits with zeros
    if (line->indexOf("<ParamDouble.\"flPatientAge\">", 0) >= 0)
    {
        return anonymizePatientAge(line);
    }

    if (readAdditionalPatientInformation)
    {
        int startPos=line->indexOf("{", 0);
        int endPos=line->indexOf("}", 0);
        QString temp = "";

        if ((patientInformation.serialNumber.isEmpty()) &&
            (line->indexOf("<ParamString.\"DeviceSerialNumber\">", 0) >= 0))
        {
            if ((startPos >= 0) && (endPos >= 0))
            {
                temp=QString(line->mid(startPos+1,endPos-startPos-1)).trimmed();
                patientInformation.serialNumber=temp.trimmed().mid(1,temp.length()-2);
            }
            return NO_SENSITIVE_INFORMATION;
        }

        if ((patientInformation.patientWeight.isEmpty()) &&
            (line->indexOf("<ParamDouble.\"flUsedPatientWeight\">", 0) >= 0))
        {
            if ((startPos >= 0) && (endPos >= 0))
            {
                temp=QString(line->mid(startPos+1,endPos-startPos-1)).trimmed();
                patientInformation.patientWeight=temp;
            }
            return NO_SENSITIVE_INFORMATION;
        }

        if ((patientInformation.patientSex.isEmpty()) &&
            (line->indexOf("<ParamLong.\"lPatientSex\">", 0) >= 0))
        {
            if ((startPos >= 0) && (endPos >= 0))
            {
                temp=QString(line->mid(startPos+1,endPos-startPos-1)).trimmed();
                patientInformation.patientSex=temp;
            }
            return NO_SENSITIVE_INFORMATION;
        }
    }

    return NO_SENSITIVE_INFORMATION;
}


int yctTWIXAnonymizer::clearLine(QByteArray* line)
{
    int startPos=line->indexOf("{ \"", 0);
    int endPos=line->indexOf("\"  }", 0);

    if (endPos<0)
    {
        // For XA software, they removed one space char at the end
        endPos=line->indexOf("\" }", 0);
    }

    if ((startPos < 0) && (endPos < 0) && (line->indexOf("{ }", 0) >= 0))
    {
        // Entry is empty with no value, i.e. { }
        return SENSITIVE_INFORMATION_CLEARED;
    }

    if (startPos < 0)
    {
        // Information seems to follow after line break
        return SENSITIVE_INFORMATION_FOLLOWS;
    }

    switch (expectedContent)
    {
    case CONTENT_NAME:
    default:
        DBG("Found patient name: " + line->toStdString());

        if (patientInformation.name.isEmpty())
        {
            patientInformation.name=QString(line->mid(startPos+3,endPos-(startPos+3)));
        }

        // If a fill string has been provided, overwrite the patient name with that;
        // otherwise, insert "Y" characters.
        if (patientInformation.fillStr.isEmpty())
        {
            for (int i=startPos+3; i<endPos; i++)
            {
                line->replace(i,1,"Y");
            }
        }
        else
        {
            int j=0;
            for (int i=startPos+3; i<endPos; i++)
            {
                if (j<patientInformation.fillStr.length())
                {
                    const char chr=patientInformation.fillStr.at(j).toLatin1();
                    line->replace(i,1,&chr,1);
                }
                else
                {
                    line->replace(i,1," ");
                }
                j++;
            }
            //std::cout << "Out:" << line->toStdString() << ":" << std::endl;
        }
        break;

    case CONTENT_REFPHYSICIAN:
        DBG("Found ref physician: " + line->toStdString());

        for (int i=startPos+3; i<endPos; i++)
        {
            line->replace(i,1,"Y");
        }
        break;

    case CONTENT_BIRTHDAY:
        DBG("Found birthday: " + line->toStdString());

        if (patientInformation.dateOfBirth.isEmpty())
        {
            patientInformation.dateOfBirth=QString(line->mid(startPos+3,endPos-(startPos+3)));
        }

        line->replace(startPos+3,8,"20120101");
        break;

    case CONTENT_ID:
        DBG("Found MRN: " + line->toStdString());

        if (patientInformation.mrn.isEmpty())
        {
            patientInformation.mrn=QString(line->mid(startPos+3,endPos-(startPos+3)));
        }

        for (int i=startPos+3; i<endPos; i++)
        {
            line->replace(i,1,"0");
        }
        break;

    case CONTENT_VERSIONSTRING:
        DBG("Found VersionString: " + line->toStdString());
        versionStringSeen=true;

        if (strictVersionChecking)
        {
            if (!checkVersionString(QString(line->mid(startPos+3,endPos-(startPos+3)))))
            {
                return PROCESSING_ERROR;
            }
        }
        break;

    case CONTENT_PATIENTREGISTRATION:
        // The patient registration block should always start in the next line, so that the above
        // check for the missing "{" should trigger. If we reach this point, something must be wrong,
        // so create an error
        DBG("Invalid formating of patient registration found: " + line->toStdString());
        return PROCESSING_ERROR;
        break;
    }

    return SENSITIVE_INFORMATION_CLEARED;
}


bool yctTWIXAnonymizer::clearPatientRegistrationEntry(QByteArray* line)
{
    int currentPosition = 0;

    while ((currentPosition < line->length()) && (line->indexOf("{ \"", currentPosition) >= 0))
    {
        // Find the quotation marks
        int startPos=line->indexOf("\"", currentPosition);
        int endPos=line->indexOf("\"", startPos+1);

        // Check if the line contains two quotation marks
        if ((endPos < 0) || (startPos < 0))
        {
            DBG("Incorrect patient registration data: " + line->toStdString());
            return false;
        }

        // Fill the patient registration string with space characters
        for (int i=startPos+1; i<endPos; i++)
        {
            line->replace(i,1," ");
        }

        currentPosition = endPos+1;
    }

    return true;
}


int yctTWIXAnonymizer::analyzeFollowLine(QByteArray* line)
{
    // The block containing the patient registration information requires special treatment
    if (expectedContent==CONTENT_PATIENTREGISTRATION)
    {
        // Ignore the preceeding items
        if (line->indexOf("<DefaultSize>", 0) >= 0)
        {
            return SENSITIVE_INFORMATION_FOLLOWS;
        }
        if (line->indexOf("<MaxSize>", 0) >= 0)
        {
            return SENSITIVE_INFORMATION_FOLLOWS;
        }
        if (line->indexOf("<Default>", 0) >= 0)
        {
            return SENSITIVE_INFORMATION_FOLLOWS;
        }

        // Check and clear the content line
        if (line->indexOf("{ \"", 0) >= 0)
        {
            if (!clearPatientRegistrationEntry(line))
            {
                return PROCESSING_ERROR;
            }
            else
            {
                // Make sure that the modified string gets written into the file
                return SENSITIVE_INFORMATION_CLEARED_FOLLOWS;
            }
        }
        else
        {
            // Ignore the line that opens the block
            if (line->indexOf("{", 0) >= 0)
            {
                return SENSITIVE_INFORMATION_FOLLOWS;
            }
            // Continue normally when the line that closes the block has been fonud
            if (line->indexOf("}", 0) >= 0)
            {
                return NO_SENSITIVE_INFORMATION;
            }
        }
    }

    // Check if line contains closing of brackets
    if (line->indexOf("}", 0) >= 0)
    {
        return NO_SENSITIVE_INFORMATION;
    }

    if (line->indexOf("<Visible> \"true\"",0) >= 0)
    {
        return SENSITIVE_INFORMATION_FOLLOWS;
    }

    // Check if line contains new XML tag
    if (line->indexOf("<", 0) >= 0)
    {
        LOG("ERROR: File inconsistency while expecting sensitive information!");
        LOG("ERROR: Affected line: " << line->toStdString());
        errorReason="File inconsistency while expecting sensitive information!";
        return PROCESSING_ERROR;
    }

    int startPos=line->indexOf("\"", 0);
    int endPos=line->indexOf("\"", startPos+1);

    if ((startPos >= 0) && (endPos <= startPos))
    {
        LOG("ERROR: Inconsistent information found!");
        errorReason="Inconsistent information found!";
        return PROCESSING_ERROR;
    }

    if ((startPos >= 0) && (endPos > startPos))
    {
        // Information found to be cleared

        switch (expectedContent)
        {
        case CONTENT_NAME:
        default:
            // If a fill string has been provided, overwrite the patient name with that;
            // otherwise, insert "Y" characters.
            if (patientInformation.fillStr.isEmpty())
            {
                for (int i=startPos+1; i<endPos; i++)
                {
                    line->replace(i,1,"Y");
                }
            }
            else
            {
                int j=0;
                for (int i=startPos+1; i<endPos; i++)
                {
                    if (j<patientInformation.fillStr.length())
                    {
                        const char chr=patientInformation.fillStr.at(j).toLatin1();
                        line->replace(i,1,&chr,1);
                    }
                    else
                    {
                        line->replace(i,1," ");
                    }
                    j++;
                }
            }
            break;

        case CONTENT_REFPHYSICIAN:
            for (int i=startPos+1; i<endPos; i++)
            {
                line->replace(i,1,"Y");
            }
            break;

        case CONTENT_ID:
            for (int i=startPos+1; i<endPos; i++)
            {
                line->replace(i,1,"0");
            }
            break;

        case CONTENT_BIRTHDAY:
            line->replace(startPos+1,8,"20120101");
            break;

        case CONTENT_VERSIONSTRING:
            DBG("Found VersionString: " + line->toStdString());
            versionStringSeen=true;

            if (strictVersionChecking)
            {
                if (!checkVersionString(QString(line->mid(startPos+1,endPos-(startPos+1)))))
                {
                    return PROCESSING_ERROR;
                }
            }
            break;
        }

        return SENSITIVE_INFORMATION_CLEARED;
    }

    // Senstive information still not found in this line
    return SENSITIVE_INFORMATION_FOLLOWS;
}


int yctTWIXAnonymizer::anonymizePatientAge(QByteArray* line)
{
    // Anonymize patient age by replacing digits after the decimal point with zeros
    // Example: 43.12321 becomes 43.00000

    int startPos = line->indexOf("{", 0);
    int endPos = line->indexOf("}", 0);

    if ((startPos < 0) || (endPos < 0) || (endPos <= startPos))
    {
        // Empty or malformed entry
        return NO_SENSITIVE_INFORMATION;
    }

    // Find the decimal point within the braces
    int decimalPos = line->indexOf(".", startPos);
    if ((decimalPos < 0) || (decimalPos >= endPos))
    {
        // No decimal point found, nothing to anonymize
        return NO_SENSITIVE_INFORMATION;
    }

    DBG("Found patient age: " + line->toStdString());

    // Replace all digits after the decimal point with '0'
    for (int i = decimalPos + 1; i < endPos; i++)
    {
        char c = line->at(i);
        if (c >= '0' && c <= '9')
        {
            line->replace(i, 1, "0");
        }
    }

    return SENSITIVE_INFORMATION_CLEARED;
}


bool yctTWIXAnonymizer::checkVersionString(QString versionString)
{
    // Prevent processing of raw-data files from software versions that have
    // not been validated (to ensure that Siemens didn't change anything in
    // the header format that might be relevant for the anonymization)
    // Unfortunately, there is no consistent way to test for the subversion
    // as the baseline string is not included in the XA versions anymore.

    static const QStringList supportedVersions = {
        "syngo MR XA60",
        "syngo MR XA51",
        "syngo MR XA50",
        "syngo MR XA40",
        "syngo MR XA31",
        "syngo MR XA30",
        "syngo MR XA20",
        "syngo MR XA11",
        "syngo MR XA10",
        "syngo MR E11",
        "syngo MR D13",
        "syngo MR D11",
        "syngo MR B19",
        "syngo MR B17",
        // Special software versions of the Biograph mMR (for E11, the P has been dropped)
        "syngo MR B20P",
        "syngo MR B18P",

    };

    for (const QString& version : supportedVersions)
    {
        if (versionString.contains(version))
        {
            return true;
        }
    }

    LOG("ERROR: Unknown software version '" << versionString.toStdString() << "'");
    LOG("Supported versions:");
    for (const QString& version : supportedVersions)
    {
        LOG("  - " << version.toStdString());
    }
    return false;
}
