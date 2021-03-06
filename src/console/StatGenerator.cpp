//
//  WebService.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 03.12.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

#ifdef WIN32
	#include <Psapi.h>
	#pragma comment(lib, "Psapi")
#else
    #include <sys/resource.h>
#endif

static float GetRAMUsage()
{
    float ram;
#ifdef WIN32    
    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    ram = (float)pmc.PagefileUsage;
#else
    int who = RUSAGE_SELF;
    struct rusage usage;
    getrusage(who, &usage);
    ram = (float)usage.ru_maxrss;
#endif
    ram /= 1024.0f;
#ifdef MAC
    ram /= 1024.0f;
#endif
    return ram;
}

static int GetNumOFCPU()
{
#ifdef WIN32
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwNumberOfProcessors;
#else
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

struct StatsDescription
{
    const char *m_pKey;
    const char *m_pDescription;
};

static const StatsDescription m_StatsDescriptionArray[] =
{
    {"svn_version",                         "Source code SVN revision"},
    {"tbb_version",                         "Intel thread building blocks version"},
    {"compile_mode",                        "if binary is compiled in RELEASE or DEBUG"},
    {"start_time",                          "TransDB startup time"},
    {"num_of_cpus",                         "Amount of server CPU(s)"},
    {"current_cpu_usage",                   "TransDB current CPU usage"},
    {"process_memory",                      "TransDB current memory usage"},
    {"activity_id",                         "Storage activity ID"},
    {"records_count",                       "Amount of records stored in TransDB"},
    {"records_in_memory",                   "Amount of records stored in memory"},
    {"records_cache_mem_usage",             "Amount of memory used by records stored in memory"},
    {"freespace_block_count",               "Amount of freespace groups/blocks"},
    {"freespace_chunk_count",               "Amount of items in freespace groups/blocks"},
    {"data_file_size",                      "Size of storage data file"},
    {"lru_cache_mempool_size",              "Amount of memory used by LRU cache"},
    {"block_mempool_size",                  "Amount of memory used by blocks"},
    {"recordindex_mempool_size",            "Amount of memory used by RecordIndexes"},
    {"block_manager_mempool_size",          "Amount of memory used by BlockManagers"},
    {"socket_send_packet_queue",            "Amount of packets wating for send"},
    {"socket_tasks_queue",                  "Size of task queue used for write operations"},
    {"socket_read_tasks_queue",             "Size of task queue used for read operations"},
    {"socket_data_received",                "Amount of bytes reveived through socket"},
    {"socket_data_sended",                  "Amount of bytes sended through socket"},
    {"avg_disk_read_time",                  "Average disk read time"},
    {"avg_disk_write_time",                 "Average disk write time"},
    {"disk_reads",                          "Amount of reads from disk to fill BlockManager"},
    {"disk_cache_reads",                    "Amount of reads from cache to fill BlockManager"},
    {"disk_writes",                         "Amount of writes to disk to save BlockManager"},
    {"disk_writer_queue_size",              "Amount of items waiting to be writtent to disk"},
    {"disk_writer_lastNumOfItemsInProcess", "Amount of items written to disk last time"},
    {"disk_writer_itemsToProcessSize",      "Actual amount of items waiting for write to disk"},
    {"record_compressions",                 "Amount of compressed records"},
    {"record_decompressions",               "Amount of decompressed records"},
    {"record_defragmentations",             "Amount of defragmented records"},
};

INLINE static const char *GetDecription(const char *pKey)
{
    size_t araySize = sizeof(m_StatsDescriptionArray) / sizeof(StatsDescription);
    for(size_t i = 0;i < araySize;++i)
    {
        if(strcmp(pKey, m_StatsDescriptionArray[i].m_pKey) == 0)
        {
            return m_StatsDescriptionArray[i].m_pDescription;
        }
    }
    return "<no decription>";
}

template<class T>
INLINE static void AddItemToJSONArray(std::stringstream &ss,
                                      const char *pKey,
                                      T value,
                                      bool oAddDescription,
                                      bool oAddComma = true)
{
    if(oAddDescription)
    {
        const char *pDescription = GetDecription(pKey);
        ss << "{\"key\":\"" << pKey << "\",\"value\":\""<< value << "\",\"desc\":\""<< pDescription << "\"}";
    }
    else
    {
        ss << "{\"key\":\"" << pKey << "\",\"value\":\""<< value << "\"}";
    }
    
    if(oAddComma)
    {
        ss << ",";
    }
}

#ifdef WIN32
    static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
    static HANDLE self;
#else
    static clock_t lastCPU, lastSysCPU, lastUserCPU;
#endif

static double GetCurrentCPUUsage()
{
    double percent;
    int numProcessors = GetNumOFCPU();
    
#ifdef WIN32
    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;

    GetSystemTimeAsFileTime(&ftime);
    memcpy(&now, &ftime, sizeof(FILETIME));

    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&sys, &fsys, sizeof(FILETIME));
    memcpy(&user, &fuser, sizeof(FILETIME));
    percent = (double)((sys.QuadPart - lastSysCPU.QuadPart) + (user.QuadPart - lastUserCPU.QuadPart));
    percent /= (now.QuadPart - lastCPU.QuadPart);
    percent /= numProcessors;
    lastCPU = now;
    lastUserCPU = user;
    lastSysCPU = sys;
#else
    struct tms timeSample;
    clock_t now;
    
    now = times(&timeSample);
    if (now <= lastCPU || timeSample.tms_stime < lastSysCPU || timeSample.tms_utime < lastUserCPU)
    {
        //Overflow detection. Just skip this value.
        percent = -1.0;
    }
    else
    {
        percent = (timeSample.tms_stime - lastSysCPU) + (timeSample.tms_utime - lastUserCPU);
        percent /= (now - lastCPU);
        percent /= numProcessors;
    }
    
    lastCPU = now;
    lastSysCPU = timeSample.tms_stime;
    lastUserCPU = timeSample.tms_utime;
#endif

    return percent * 100;
}

void StatGenerator::Init()
{
#ifdef WIN32
    FILETIME ftime, fsys, fuser;
    
    GetSystemTimeAsFileTime(&ftime);
    memcpy(&lastCPU, &ftime, sizeof(FILETIME));
    
    self = GetCurrentProcess();
    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
    memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
#else
    struct tms timeSample;
    lastCPU = times(&timeSample);
    lastSysCPU = timeSample.tms_stime;
    lastUserCPU = timeSample.tms_utime;
#endif
}

StatGenerator::StatGenerator(Storage &rStorage) : m_rStorage(rStorage)
{

}

void StatGenerator::GetDiskWriterStats(uint64 &queueSize, uint64 &lastNumOfItemsInProcess, uint64 &itemsToProcessSize)
{
    queueSize = m_rStorage.m_pDiskWriter->GetQueueSize();
    lastNumOfItemsInProcess = m_rStorage.m_pDiskWriter->GetLastNumOfItemsInProcess();
    itemsToProcessSize = m_rStorage.m_pDiskWriter->GetItemsToProcessSize();
}

void StatGenerator::GenerateStats(bbuff *pBuff, bool oAddDescription)
{
    char rStartTime[512];    
    tm localTime;
    std::stringstream ss;
    uint64 diskWriterQueueSize = 0;
    uint64 diskWriterLastNumOfItemsInProcess = 0;
    uint64 diskWriterItemsToProcessSize = 0;
    
    //create starttime string
    localtime(&g_stats.StartTime, &localTime);
    sprintf(rStartTime,
            "%04u-%02u-%02u %02u:%02u:%02u",
            localTime.tm_year+1900,
            localTime.tm_mon+1,
            localTime.tm_mday,
            localTime.tm_hour,
            localTime.tm_min,
            localTime.tm_sec);

    //DiskWriter
    GetDiskWriterStats(diskWriterQueueSize, diskWriterLastNumOfItemsInProcess, diskWriterItemsToProcessSize);
    
    //Generate JSON
    ss << "[";
    
    //common
    AddItemToJSONArray(ss, "svn_version", SVNVERSION, oAddDescription);
    AddItemToJSONArray(ss, "tbb_version", TBB_INTERFACE_VERSION, oAddDescription);
    AddItemToJSONArray(ss, "compile_mode", g_pCompiledVersion, oAddDescription);
    AddItemToJSONArray(ss, "start_time", rStartTime, oAddDescription);
    AddItemToJSONArray(ss, "num_of_cpus", GetNumOFCPU(), oAddDescription);
    AddItemToJSONArray(ss, "current_cpu_usage", GetCurrentCPUUsage(), oAddDescription);
    AddItemToJSONArray(ss, "process_memory", GetRAMUsage(), oAddDescription);
    AddItemToJSONArray(ss, "activity_id", g_cfg.ActivityID, oAddDescription);
    AddItemToJSONArray(ss, "records_count", m_rStorage.m_dataIndexes.size(), oAddDescription);
    AddItemToJSONArray(ss, "records_cache_mem_usage", m_rStorage.m_memoryUsed.load(), oAddDescription);
    AddItemToJSONArray(ss, "data_file_size", m_rStorage.m_dataFileSize.load(), oAddDescription);
    //socket
    AddItemToJSONArray(ss, "socket_send_packet_queue", g_rClientSocketHolder.GetAllSocketPacketQueueSize(), oAddDescription);
    AddItemToJSONArray<int64>(ss, "socket_tasks_queue", g_pClientSocketWorker->GetQueueSize(), oAddDescription);
    AddItemToJSONArray<int64>(ss, "socket_read_tasks_queue", g_pClientSocketWorker->GetReadQueueSize(), oAddDescription);
    AddItemToJSONArray(ss, "socket_data_received", g_stats.ReceivedBytes, oAddDescription);
    AddItemToJSONArray(ss, "socket_data_sended", g_stats.SendedBytes, oAddDescription);
    //disk
    AddItemToJSONArray(ss, "avg_disk_read_time", g_stats.AvgDiskReadTime, oAddDescription);
    AddItemToJSONArray(ss, "avg_disk_write_time", g_stats.AvgDiskWriteTime, oAddDescription);
    AddItemToJSONArray(ss, "disk_reads", g_stats.NumOfReadsFromDisk, oAddDescription);
    AddItemToJSONArray(ss, "disk_cache_reads", g_stats.NumOfReadsFromCache, oAddDescription);
    AddItemToJSONArray(ss, "disk_writes", g_stats.NumOfWritesToDisk, oAddDescription);
    //diskwriter
    AddItemToJSONArray(ss, "disk_writer_queue_size", diskWriterQueueSize, oAddDescription);
    AddItemToJSONArray(ss, "disk_writer_lastNumOfItemsInProcess", diskWriterLastNumOfItemsInProcess, oAddDescription);
    AddItemToJSONArray(ss, "disk_writer_itemsToProcessSize", diskWriterItemsToProcessSize, oAddDescription);
    //compression
    AddItemToJSONArray(ss, "record_compressions", g_stats.NumOfRecordCompressions, oAddDescription);
    AddItemToJSONArray(ss, "record_decompressions", g_stats.NumOfRecordDecompressions, oAddDescription);
    //other
    AddItemToJSONArray(ss, "record_defragmentations", g_stats.NumOfRecordDeframentations, oAddDescription, false);
    
    ss << "]";
    
    //fill packet response
    bbuff_append(pBuff, ss.str().c_str(), ss.str().length());
}











