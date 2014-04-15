//
//  FileWatcher.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/10/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

ConfigWatcher *g_pConfigWatcher = NULL;

ConfigWatcher::ConfigWatcher(const std::string &sConfigPath) : m_configPath(sConfigPath)
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
                //notify
                executeListeners();
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

void ConfigWatcher::addListener(IConfigListener *pIConfigListener)
{
    std::lock_guard<tbb::mutex> rGuard(m_rListenersLock);
    m_rListeners.insert(pIConfigListener);
}

void ConfigWatcher::removeListener(IConfigListener *pIConfigListener)
{
    std::lock_guard<tbb::mutex> rGuard(m_rListenersLock);
    m_rListeners.erase(pIConfigListener);
}

void ConfigWatcher::executeListeners()
{
    std::lock_guard<tbb::mutex> rGuard(m_rListenersLock);
    for(ListenerSet::iterator itr = m_rListeners.begin();itr != m_rListeners.end();++itr)
    {
        (*itr)->onConfigReload();
    }
}

