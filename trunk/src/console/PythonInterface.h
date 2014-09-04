//
//  PythonInterface.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 14.01.14.
//  Copyright (c) 2014 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_PythonInterface_h
#define TransDB_PythonInterface_h

class PythonInterface : public ThreadContext, public IConfigListener
{
public:
    explicit PythonInterface();
    
    void reloadScript();
    void callOnShutdownPythonMethod();
    std::string executePythonScript(const uint8 *pScriptData, size_t scriptDataSize);
    
    //IConfigListener
    void onConfigReload();
    
    //ThreadContext
    bool run();
    void OnShutdown();
    
private:
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(PythonInterface);
    
    PyObject            *m_pInstance;
    PyObject            *m_pScriptSkeletonModule;
    std::string         m_lastVersion;
    std::atomic<bool>   m_pythonScriptRunning;
};

extern PythonInterface *g_pPythonInterface;

#endif
