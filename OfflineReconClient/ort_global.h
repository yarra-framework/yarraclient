#ifndef ORT_GLOBAL_H
#define ORT_GLOBAL_H

// Include global definitions from the RDS client
#include "../Client/rds_global.h"

#define ORT_VERSION     "0.38b6"

#define ORT_ICON QIcon(":/images/orticon_256.png")

#define ORT_SERVERFILE           "YarraServer.cfg"
#define ORT_MODEFILE             "YarraModes.cfg"
#define ORT_SERVERLISTFILE       "YarraServerList.cfg"

#define ORT_LBI_NAME             "lbi.ini"
#define ORT_INI_NAME             "ort.ini"

#define ORT_TASK_DIR             "queue"
#define ORT_TASK_EXTENSION       ".task"
#define ORT_LOCK_EXTENSION       ".lock"
#define ORT_TWIX_EXTENSION       ".dat"
#define ORT_PHI_EXTENSION        ".phi"
#define ORT_TASK_EXTENSION_PRIO  "_prio"
#define ORT_TASK_EXTENSION_NIGHT "_night"
#define ORT_INVALID              "X-INVALID-X"

#define ORT_MINSIZEMB            2.0

#define ORT_SCANSHOW_DEF         50
#define ORT_SCANSHOW_MANMULT     10

#define ORT_RAID_MAXPARSECOUNT   3000

#define ORT_CONNECT_TIMEOUT      10000

#define ORT_DIR_QUEUE            "recoqueue"

#define ORT_CONNECTION_SMB       "SMB"
#define ORT_CONNECTION_SFTP      "SFTP / SCP"

#define ORT_WINSCP_BINARY        "winscp/WinSCP.com"
#define ORT_WINSCP_SUPPORTBINARY "winscp/WinSCP.exe"

#endif // ORT_GLOBAL_H
