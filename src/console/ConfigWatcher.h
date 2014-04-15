//
//  FileWatcher.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/10/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDB__ConfigWatcher__
#define __TransDB__ConfigWatcher__

class ConfigWatcher : public ThreadContext
{
public:
	ConfigWatcher(const char *pConfigPath);

	bool run();

private:
	string	m_configPath;
};

#endif /* defined(__TransDB__ConfigWatcher__) */
