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

static const char *pHTTPHeaderProto = "HTTP/1.1 200 OK\r\nCache-Control: no-cache\r\nContent-Type: text/xml\r\nContent-Length: %u\r\nConnection: close\r\n\r\n";

WebService::WebService(SOCKET fd) : Socket(fd, g_SocketReadBufferSize, g_SocketWriteBufferSize)
{
    m_rGETHandlerMap.insert(GETHandlerMap::value_type("GET / HTTP/1.1", &WebService::HandleRoot));
    m_rGETHandlerMap.insert(GETHandlerMap::value_type("GET /log.xml HTTP/1.1", &WebService::HandleLog));
}

WebService::~WebService()
{
    
}

static float GetRAMUsageWin32()
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
    ram /= 1024.0f;
    return ram;
}

void WebService::OnRead()
{
    char rBuff[512];
    ByteBuffer rResponse;
    ByteBuffer rPacket;
    ByteBuffer rDummy;
    GeewaPacket rGEPacket;
    size_t dataSize;
    
    //read data from socket buffer
    dataSize = m_readBuffer.GetSize();
    if(dataSize == 0)
        return;
    
	try
	{
#ifdef WIN32
		_set_se_translator(trans_func);
#endif

		rDummy.resize(dataSize);
		m_readBuffer.Read((void*)rDummy.contents(), rDummy.size());
        
		//process request
		rGEPacket.AddHTTPData((char*)rDummy.contents(), rDummy.size());
        
		//process GET
		GETHandlerMap::iterator itr = m_rGETHandlerMap.find(rGEPacket.headers()["0"]);
		if(itr != m_rGETHandlerMap.end())
		{
			(void)(this->*itr->second)(rResponse);
		}
		else
		{
			HandleOther(rResponse);
		}
        
		//create header + append data
		sprintf(rBuff, pHTTPHeaderProto, rResponse.size());
		rPacket.append(rBuff, strlen(rBuff));
		rPacket.append(rResponse);
	}
	catch(...)
	{
		Log.Error(__FUNCTION__, "Fatal error stopping server.");
		Sync_Add(&g_stopEvent);
		return;
	}
    
    //
    OutPacket(rPacket.size(), rPacket.contents());
}

void WebService::HandleRoot(ByteBuffer &rResponse)
{
    char rStartTime[512];    
    tm localTime;
    
    //create starttime string
    localtime(&g_StartTime, &localTime);
    sprintf(rStartTime, "%04u-%02u-%02u %02u:%02u:%02u", localTime.tm_year+1900, localTime.tm_mon+1, localTime.tm_mday, localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    
    //lock
    std::lock_guard<tbb::mutex> rFreeSpaceLock(g_pStorage->m_freeSpaceLock);
    std::lock_guard<tbb::spin_mutex> rFreeSpaceQueueLock(g_pStorage->m_freeSpaceQueueLock);
    std::lock_guard<tbb::mutex> rLRUGuard(g_pStorage->m_LRULock);
    std::lock_guard<tbb::mutex> rBlockMemPoolGuard(g_pStorage->m_rBlockMemPoolLock);
    std::lock_guard<tbb::mutex> rRIMemPoolGuard(g_pStorage->m_rRecordIndexMemPoolLock);
    
    rResponse.appendNonNullTermStr("<?xml version=\"1.0\"?>");
    rResponse.appendNonNullTermStr("<stats>");
    rResponse.appendNonNullTermStr("<node id=\"start_time\" value=\"%s\" />", rStartTime);
    rResponse.appendNonNullTermStr("<node id=\"process_memory\" value=\"%.3f\" units=\"MB\" />", GetRAMUsageWin32());
    rResponse.appendNonNullTermStr("<node id=\"activity_id\" value=\"%s\" />", g_ActivityID.c_str());
    rResponse.appendNonNullTermStr("<node id=\"user_count\" value=\""I64FMTD"\" />", g_pStorage->m_dataIndexes.size());
    rResponse.appendNonNullTermStr("<node id=\"cache_mem_usage\" value=\""I64FMTD"\" units=\"B\" />", g_pStorage->m_memoryUsed.load());
    rResponse.appendNonNullTermStr("<node id=\"lru_cache_count\" value=\""I64FMTD"\" />", g_pStorage->m_pLRUCache->size());
    rResponse.appendNonNullTermStr("<node id=\"lru_cache_mempool_size\" value=\""I64FMTD"\" units=\"B\" />", g_pStorage->m_pLRUCache->cacheSize());
    rResponse.appendNonNullTermStr("<node id=\"disk_reads\" value=\""I64FMTD"\" />", g_NumOfReadsFromDisk);
    rResponse.appendNonNullTermStr("<node id=\"disk_cache_reads\" value=\""I64FMTD"\" />", g_NumOfReadsFromCache);
    rResponse.appendNonNullTermStr("<node id=\"disk_writes\" value=\""I64FMTD"\" />", g_NumOfWritesToDisk);
    rResponse.appendNonNullTermStr("<node id=\"freespace_mempool_size\" value=\""I64FMTD"\" units=\"B\" />", g_pStorage->m_pFreeSpaceMemoryPool->GetSize());
    rResponse.appendNonNullTermStr("<node id=\"freespace_count\" value=\""I64FMTD"\" />", g_pStorage->m_freeSpace.size());
    rResponse.appendNonNullTermStr("<node id=\"freespace_queue_count\" value=\""I64FMTD"\" />", g_pStorage->m_freeSpaceQueue.size());
    rResponse.appendNonNullTermStr("<node id=\"data_file_size\" value=\""I64FMTD"\" units=\"B\" />", g_pStorage->m_dataFileSize);
    rResponse.appendNonNullTermStr("<node id=\"socket_packet_queue\" value=\""I64FMTD"\" />", g_rClientSocketHolder.GetAllSocketPacketQueueSize());
    rResponse.appendNonNullTermStr("<node id=\"socket_tasks_queue\" value=\""SI64FMTD"\" />", g_pClientSocketWorker->GetQueueSize());
    rResponse.appendNonNullTermStr("<node id=\"socket_data_received\" value=\""I64FMTD"\" units=\"B\" />", g_ReceivedBytes.load());
    rResponse.appendNonNullTermStr("<node id=\"socket_data_sended\" value=\""I64FMTD"\" units=\"B\" />", g_SendedBytes.load());
    rResponse.appendNonNullTermStr("<node id=\"global_disk_reads\" value=\""I64FMTD"\" />", g_NumOfFreads.load());
    rResponse.appendNonNullTermStr("<node id=\"global_disk_writes\" value=\""I64FMTD"\" />", g_NumOfFwrites.load());
    rResponse.appendNonNullTermStr("<node id=\"global_disk_seeks\" value=\""I64FMTD"\" />", g_NumOfFseeks.load());
    rResponse.appendNonNullTermStr("<node id=\"record_compressions\" value=\""I64FMTD"\" />", g_NumOfRecordCompressions.load());
    rResponse.appendNonNullTermStr("<node id=\"record_decompressions\" value=\""I64FMTD"\" />", g_NumOfRecordDecompressions.load());
    rResponse.appendNonNullTermStr("<node id=\"block_mempool_size\" value=\""I64FMTD"\" units=\"B\" />", g_pStorage->m_pBlockMemPool->GetSize());
    rResponse.appendNonNullTermStr("<node id=\"recordindex_mempool_size\" value=\""I64FMTD"\" units=\"B\" />", g_pStorage->m_pRecordIndexMemPool->GetSize());
    
    //dump config
    rResponse.appendNonNullTermStr("<config>");
    ConfigSettings &rSettings = g_rConfig.MainConfig.GetConfigMap();
    for(ConfigSettings::iterator itr = rSettings.begin();itr != rSettings.end();++itr)
    {
        for(unordered_map<string, ConfigSetting>::iterator it2 = itr->second.ConfigBlock.begin();it2 != itr->second.ConfigBlock.end();++it2)
        {
            rResponse.appendNonNullTermStr("<node block=\"%s\" key=\"%s\" value=\"%s\" />", itr->first.c_str(), it2->first.c_str(), it2->second.AsString.c_str());
        }
    }
    rResponse.appendNonNullTermStr("</config>");
    rResponse.appendNonNullTermStr("</stats>");
}

void WebService::HandleLog(ByteBuffer &rResponse)
{
    FileLog *pFileLog;
    ifstream rLog;
    string sLine;
    
    rResponse.appendNonNullTermStr("<?xml version=\"1.0\"?>");
    
    pFileLog = Log.GetFileLog();
    if(pFileLog)
    {
        rLog.open(pFileLog->GetFileName().c_str());
        if(rLog.is_open())
        {
            rResponse.appendNonNullTermStr("<log>");
            
            for(;;)
            {
                if(rLog.eof())
                    break;
                
                getline(rLog, sLine);
                if(!sLine.size())
                    continue;
                
                rResponse.appendNonNullTermStr("<log_line value=\"%s\" />", sLine.c_str());
            }
            
            rResponse.appendNonNullTermStr("</log>");
        }
    }
    else
    {
        rResponse.appendNonNullTermStr("<response>File log does not exist</response>");
    }
}

void WebService::HandleOther(ByteBuffer &rResponse)
{
    rResponse.appendNonNullTermStr("<?xml version=\"1.0\"?>");
    rResponse.appendNonNullTermStr("<response>Recource not found</response>");
}

void WebService::OnConnect()
{
    
}

void WebService::OnDisconnect()
{
    
}

OUTPACKET_RESULT WebService::OutPacket(const size_t &len, const void* data)
{
	bool rv;
	if(!IsConnected())
		return OUTPACKET_RESULT_NOT_CONNECTED;
    
	BurstBegin();
	if(m_writeBuffer.GetSpace() < len)
	{
		BurstEnd();
		return OUTPACKET_RESULT_NO_ROOM_IN_BUFFER;
	}
    
	// Pass the rest of the packet to our send buffer
    rv = BurstSend((const uint8*)data, len);
    
	if(rv)
	{
		BurstPush();
	}
    
	BurstEnd();
    
	return rv ? OUTPACKET_RESULT_SUCCESS : OUTPACKET_RESULT_SOCKET_ERROR;
}