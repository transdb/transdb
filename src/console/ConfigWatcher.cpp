//
//  FileWatcher.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/10/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

ConfigWatcher::ConfigWatcher(const char *pConfigPath) : m_configPath(pConfigPath)
{

}

bool ConfigWatcher::run()
{
    SetThreadName("ConfigWatcher thread");
    
    ByteBuffer rBuff;
    time_t lastFileModification;
    time_t lastFileModificationNew;
    
    //
    lastFileModification = Common::GetLastFileModificationTime(m_configPath.c_str());
    
	while(m_threadRunning)
	{
        lastFileModificationNew = Common::GetLastFileModificationTime(m_configPath.c_str());
        if(lastFileModificationNew != lastFileModification)
        {
            //save last access time
            lastFileModification = lastFileModificationNew;
            //reload
            if(g_rConfig.MainConfig.SetSource(m_configPath.c_str()))
            {
                //reload config
                LoadConfig();
            }
            else
            {
                Log.Error(__FUNCTION__, "Config: %s not found.", m_configPath.c_str());
            }
        }

        Wait(1000);
	}

	return true;
}