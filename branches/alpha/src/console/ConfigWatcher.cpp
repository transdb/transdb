//
//  FileWatcher.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/10/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

ConfigWatcher::ConfigWatcher(const char *pConfigPath)
{
	m_configPath    = pConfigPath;
    m_lastCrc32     = 0;
}

bool ConfigWatcher::run()
{
    SetThreadName("ConfigWatcher thread");
    
    uint32 crc32check;
    size_t size;
    ByteBuffer rBuff;
	FILE *pFile;
	while(m_threadRunning)
	{
        //
        pFile = fopen(m_configPath.c_str(), "rb");
        if(pFile)
        {
            IO::fseek(pFile, 0, SEEK_END);
            size = IO::ftell(pFile);
            IO::fseek(pFile, 0, SEEK_SET);
            //resize buffer
            rBuff.resize(size);
            IO::fread((void*)rBuff.contents(), size, 1, pFile);
            IO::fclose(pFile);
            //
            crc32check = g_CRC32->ComputeCRC32(rBuff.contents(), rBuff.size());
            if(crc32check != m_lastCrc32)
            {
                if(g_rConfig.MainConfig.SetSource(m_configPath.c_str()))
                {
                    //reload config
                    LoadConfig();
                    //
                    m_lastCrc32 = crc32check;
                }
                else
                {
                    Log.Error(__FUNCTION__, "Config: %s not found.", m_configPath.c_str());
                }
            }
        }

        Wait(1000);
	}

	return true;
}