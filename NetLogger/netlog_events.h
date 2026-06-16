#ifndef NETLOG_EVENTS_H
#define NETLOG_EVENTS_H

// Defnition of backend endpoints
#define NETLOG_ENDPT_TEST    "test"
#define NETLOG_ENDPT_EVENT   "events"
#define NETLOG_ENDPT_RAIDLOG "scans"

#define NETLOG_POST_TIMEOUT     30000
#define NETLOG_EVENT_TIMEOUT    5000
#define NETLOG_NSLOOKUP_TIMEOUT 10000
#include <iostream>

// Event definitions
namespace EventInfo
{
    enum class Type : int
    {
        Generic=0,
        Boot,               // for YarraServer, RDS, WebGUI, ArchiveSearch
        Shutdown,
        Update,             // for RDS
        RawDataStorage,
        ScanInfo,
        Transfer,           // for ORT, RDS, SAC
        Processing,         // for YarraServer
        ExceptionCaught,    // for WebGUI debugging
        Heartbeat           // for peripherals
    };

    inline std::ostream& operator<< (std::ostream& o, const Type& c)
    {
        switch (c)
        {
        case Type::Generic:          return o << "Generic";
        case Type::Boot:             return o << "Boot";
        case Type::Shutdown:         return o << "Shutdown";
        case Type::Update:           return o << "Update";
        case Type::RawDataStorage:   return o << "RawDataStorage";
        case Type::ScanInfo:         return o << "ScanInfo";
        case Type::Transfer:         return o << "Transfer";
        case Type::Processing:       return o << "Processing";
        case Type::ExceptionCaught:  return o << "ExceptionCaught";
        case Type::Heartbeat:        return o << "Heartbeat";
        }
        return o << static_cast<std::uint16_t>(c);
    }


    enum class Detail : int
    {
        Information=0,
        Start,
        End,
        LowDiskSpace,
        Diagnostics,
        Inventory,
        Push,
        FileTransfer
    };

    inline std::ostream& operator<< (std::ostream& o, const Detail& c)
    {
        switch (c)
        {
        case Detail::Information:   return o << "Information";
        case Detail::Start:         return o << "Start";
        case Detail::End:           return o << "End";
        case Detail::LowDiskSpace:  return o << "LowDiskSpace";
        case Detail::Diagnostics:   return o << "Diagnostics";
        case Detail::Inventory:     return o << "Inventory";
        case Detail::Push:          return o << "Push";
        case Detail::FileTransfer:  return o << "FileTransfer";
        }
        return o << static_cast<std::uint16_t>(c);
    }


    enum class SourceType : int
    {
        Generic=0,
        RDS,
        SAC,
        ORT,
        Server,
        WebGUI,
        ArchiveSearchIndexer,
        ArchiveSearchGUI,
        LogServer,
        Peripheral
    };

    inline std::ostream& operator<< (std::ostream& o, const SourceType& c)
    {
        switch (c)
        {
        case SourceType::Generic:               return o << "Generic";
        case SourceType::RDS:                   return o << "RDS";
        case SourceType::SAC:                   return o << "SAC";
        case SourceType::ORT:                   return o << "ORT";
        case SourceType::Server:                return o << "Server";
        case SourceType::WebGUI:                return o << "WebGUI";
        case SourceType::ArchiveSearchIndexer:  return o << "ArchiveSearchIndexer";
        case SourceType::ArchiveSearchGUI:      return o << "ArchiveSearchGUI";
        case SourceType::LogServer:             return o << "LogServer";
        case SourceType::Peripheral:            return o << "Peripheral";
        }
        return o << static_cast<std::uint16_t>(c);
    }


    enum class Severity : int
    {
        Success=0,
        Warning,
        Error,
        FatalError
    };

    inline std::ostream& operator<< (std::ostream& o, const Severity& c)
    {
        switch (c)
        {
        case Severity::Success:      return o << "Success";
        case Severity::Warning:      return o << "Warning";
        case Severity::Error:        return o << "Error";
        case Severity::FatalError:   return o << "FatalError";
        }
        return o << static_cast<std::uint16_t>(c);
    }
}


#endif // NETLOG_EVENTS_H
