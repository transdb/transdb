//
//  FileWatcher.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/10/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDB__ConfigWatcher__
#define __TransDB__ConfigWatcher__

class IConfigListener
{
public:
    virtual void onConfigReload() = 0;
};

class ConfigWatcher : public ThreadContext
{
    typedef std::set<IConfigListener*> ListenerSet;
    
public:
	explicit ConfigWatcher(const std::string &sConfigPath);
    
    //
    void addListener(IConfigListener *pIConfigListener);
    void removeListener(IConfigListener *pIConfigListener);
    void executeListeners();

    //ThreadContext
	bool run();

    //
    INLINE const char *GetConfigPath()     { return m_configPath.c_str(); }
    
private:
	string          m_configPath;
    ListenerSet     m_rListeners;
    tbb::mutex      m_rListenersLock;
};

extern ConfigWatcher *g_pConfigWatcher;

#endif /* defined(__TransDB__ConfigWatcher__) */
