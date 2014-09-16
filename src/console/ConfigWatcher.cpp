//
//  FileWatcher.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/10/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

ConfigWatcher::ConfigWatcher()
{

}

bool ConfigWatcher::run()
{
    CommonFunctions::SetThreadName("ConfigWatcher thread");
    
    //
    time_t lastFileModification;
    time_t lastFileModificationNew;
    std::string sConfigPath = g_rConfig.MainConfig.GetConfigFilePath();
    
    //
    lastFileModification = CommonFunctions::GetLastFileModificationTime(sConfigPath.c_str());
    
	while(m_threadRunning)
	{
        lastFileModificationNew = CommonFunctions::GetLastFileModificationTime(sConfigPath.c_str());
        if(lastFileModificationNew != lastFileModification)
        {
            //save last access time
            lastFileModification = lastFileModificationNew;
            //reload
            if(g_rConfig.MainConfig.SetSource(sConfigPath.c_str()))
            {
                //reload config
                load_and_parse_config_values();
                //notify
                executeListeners();
            }
            else
            {
                Log.Error(__FUNCTION__, "Config: %s not found.", sConfigPath.c_str());
            }
        }

        Wait(1000);
	}

	return true;
}

void ConfigWatcher::addListener(IConfigListener *pIConfigListener)
{
    std::lock_guard<std::mutex> rGuard(m_rListenersLock);
    m_rListeners.insert(pIConfigListener);
}

void ConfigWatcher::removeListener(IConfigListener *pIConfigListener)
{
    std::lock_guard<std::mutex> rGuard(m_rListenersLock);
    m_rListeners.erase(pIConfigListener);
}

void ConfigWatcher::executeListeners()
{
    std::lock_guard<std::mutex> rGuard(m_rListenersLock);
    for(ListenerSet::iterator itr = m_rListeners.begin();itr != m_rListeners.end();++itr)
    {
        (*itr)->onConfigReload();
    }
}

