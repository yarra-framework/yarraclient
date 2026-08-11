#ifndef RDS_GLOBAL_H
#define RDS_GLOBAL_H

// Include definition of the singleton for runtime control
#include "rds_runtimeinformation.h"

// Definitions
#define RDS_VERSION     "0.65b3"
#define RDS_PASSWORD    "nyc2012"
#define RDS_DBGPASSWORD "pastrami"

#define RDS_PROC_TIMEOUT         600000
#define RDS_RAIDSTORE_TIMEOUT    3600000
#define RDS_COPY_TIMEOUT         3600000
#define RDS_TIMERINTERVAL        60000
#define RDS_SLEEP_INTERVAL       10
#define RDS_CONNECT_TIMEOUT      1200000
#define RDS_NETLOG_RETRYINTERVAL 1800000
#define RDS_MAXLOGSIZE           1000000
#define RDS_HEARTBEAT_INTERVAL   300000

#define RDS_DIR_LOG   "log"
#define RDS_DIR_QUEUE "queue"

#define RDS_INI_NAME  "/rds.ini"
#define RDS_LPFI_NAME "/lpfi.ini"

#define RDS_SCANINFO_EXT    ".ylb"
#define RDS_SCANINFO_HEADER "## Yarra RDS ScanInfo ##"

#define RDS_RAIDTOOL_PATH      "C:/MedCom/bin"
#define RDS_RAIDTOOL_NAME      "RaidTool.exe"
#define RDS_RAIDSIMULATOR_NAME "RaidSimulator.exe"

#define RDS_SYNGODETECT_VB13A "C:/MedCom/MriProduct/inst/build_VB13A_label.txt"
#define RDS_SYNGODETECT_VB15A "C:/MedCom/MriProduct/inst/build_VB15A_label.txt"
#define RDS_SYNGODETECT_VB17A "C:/MedCom/MriProduct/inst/build_VB17A_label.txt"
#define RDS_SYNGODETECT_VB19A "C:/MedCom/MriProduct/inst/build_VB19A_label.txt"
#define RDS_SYNGODETECT_VB19B "C:/MedCom/MriProduct/inst/build_VB19B_label.txt"
#define RDS_SYNGODETECT_VB18P "C:/MedCom/MriProduct/inst/build_VB18P_label.txt"
#define RDS_SYNGODETECT_VB20P "C:/MedCom/MriProduct/inst/build_VB20P_label.txt"
#define RDS_SYNGODETECT_VD11A "C:/MedCom/MriProduct/inst/build_VD11A_label.txt"
#define RDS_SYNGODETECT_VD11D "C:/MedCom/MriProduct/inst/build_VD11D_label.txt"
#define RDS_SYNGODETECT_VD13A "C:/MedCom/MriProduct/inst/build_VD13A_label.txt"
#define RDS_SYNGODETECT_VD13B "C:/MedCom/MriProduct/inst/build_VD13B_label.txt"
#define RDS_SYNGODETECT_VD13C "C:/MedCom/MriProduct/inst/build_VD13C_label.txt"
#define RDS_SYNGODETECT_VD13D "C:/MedCom/MriProduct/inst/build_VD13D_label.txt"
#define RDS_SYNGODETECT_VE11A "C:/MedCom/MriProduct/inst/build_VE11A_label.txt"
#define RDS_SYNGODETECT_VE11B "C:/MedCom/MriProduct/inst/build_VE11B_label.txt"
#define RDS_SYNGODETECT_VE11C "C:/MedCom/MriProduct/inst/build_VE11C_label.txt"
#define RDS_SYNGODETECT_VE11U "C:/MedCom/MriProduct/inst/build_VE11U_label.txt"
#define RDS_SYNGODETECT_VE11P "C:/MedCom/MriProduct/inst/build_VE11P_label.txt"
#define RDS_SYNGODETECT_VE12U "C:/MedCom/MriProduct/inst/build_VE12U_label.txt"
#define RDS_SYNGODETECT_VE11D "C:/MedCom/MriProduct/inst/build_VE11D_label.txt"
#define RDS_SYNGODETECT_VE11E "C:/MedCom/MriProduct/inst/build_VE11E_label.txt"
#define RDS_SYNGODETECT_VE11S "C:/MedCom/MriProduct/inst/build_VE11S_label.txt"

#define RDS_IMAGER_IP_3 "192.168.2.3"
#define RDS_IMAGER_IP_2 "192.168.2.2"

#define RDS_IMAGER_PORT "8010"

#define RDS_UPDATETIME_STARTUP   10
#define RDS_UPDATETIME_RETRY      5
#define RDS_UPDATETIME_RAIDRETRY  2

#define RDS_CONNECTIONFAILURE_COUNT   5
#define RDS_STARTUPCMDAFTERFAIL_COUNT 3

#define RDS_DISKLIMIT_WARNING     3000000000
#define RDS_DISKLIMIT_ALTERNATING 5000000000

#define RDS_LOG_CHECKSUM false
#define RDS_FILESIZE_FILTER 1000000
#define RDS_SHIMSIZE_FILTER 1400000

// Convenience macros
#define RDS_FREE(x) if (x!=0) { delete x; x=0; }
#define RDS_FREEARR(x) if (x!=0) { delete[] x; x=0; }
#define RDS_RETONERR(x) if (!x) return false;
#define RDS_RETONERR_LOG(x,y) if (!x) { RTI->log(y); return false; };

#define RDS_ICON      QIcon(":/images/yarraicon.png")
#define ORT_ICON_MENU QIcon(":/images/orticon.png")
#define YCA_ICON_MENU QIcon(":/images/ycaicon.png")
#define CMD_ICON_MENU QIcon(":/images/startupicon.png")

#define RTI rdsRuntimeInformation::getInstance()
#define RTI_CONFIG  rdsRuntimeInformation::getInstance()->getConfigInstance()
#define RTI_RAID    rdsRuntimeInformation::getInstance()->getRaidInstance()
#define RTI_NETWORK rdsRuntimeInformation::getInstance()->getNetworkInstance()
#define RTI_CONTROL rdsRuntimeInformation::getInstance()->getControlInstance()
#define RTI_NETLOG  RTI_NETWORK->netLogger


// Naming convention for queued files: filtername#filename.dat
#define RTI_SEPT_CHAR "#"


// RaidTool output codes
#define RDS_RAID_ERROR_INIT    "Could not initialize tool instance for remote access"
#define RDS_RAID_ERROR_DIR     "Could not retrieve directory information"
#define RDS_RAID_ERROR_FILE    "Could not find file ID"
#define RDS_RAID_ERROR_COPY    "Could not copy measurement data"
#define RDS_RAID_SUCCESS       "Copied measurement data to file"
#define RDS_RAID_DIRHEAD       " FileID     MeasID                        ProtName                         PatName   Status         Size   SizeOnDisk          CreationTime             CloseTime"
#define RDS_RAID_DIRHEAD_VD13C " FileID     MeasID                        ProtName                         PatName   Status         Size   SizeOnDisk          CreationTime             CloseTime"
#define RDS_RAID_DIRHEAD_VB15  " FileID     MeasID                        ProtName   Status         Size   SizeOnDisk          CreationTime             CloseTime"
#define RDS_RAID_DIRHEAD_VB13  " FileID     MeasID        ProtName   Status         Size   SizeOnDisk          CreationTime             CloseTime"


// Note: It can be either "1 dependent file:" or "n dependent files:"
#define RDS_RAID_VD_IGNORE1    "dependent file"
#define RDS_RAID_VD_IGNORE2    "measID:"

#define RDS_RAID_DEBUG_HEADER  "## Output lines from RaidTool = "
#define RDS_RAID_DEBUG_FOOTER  "## End"

// Names of the adjustment scans which should be saved together with the scan data
#define RDS_ADJUSTSCANVB17_COILSENS "AdjCoilSens"

// Allow building a Linux version for development and debugging
#if (defined (LINUX) || defined (__linux__) || defined(Q_OS_UNIX))
    #include <unistd.h>
    #define Sleep(x) usleep((x)*1000)
    typedef unsigned int DWORD;
#endif

#endif // RDS_GLOBAL_H

