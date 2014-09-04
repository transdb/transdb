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
	explicit ConfigWatcher();
    
    //
    void addListener(IConfigListener *pIConfigListener);
    void removeListener(IConfigListener *pIConfigListener);
    void executeListeners();

    //ThreadContext
	bool run();

private:
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(ConfigWatcher);
    
    ListenerSet         m_rListeners;
    std::mutex          m_rListenersLock;
};

#endif /* defined(__TransDB__ConfigWatcher__) */
