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
    
    //IConfigListener
    void onConfigReload();
    
    //ThreadContext
    bool run();
    void OnShutdown();
    
private:
    PyObject            *m_pInstance;
    std::string         m_lastVersion;
    tbb::atomic<bool>   m_pythonScriptRunning;
};

#endif
