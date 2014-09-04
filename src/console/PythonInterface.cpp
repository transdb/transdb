//
//  PythonInterface.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 14.01.14.
//  Copyright (c) 2014 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

static PyObject *Log_Notice(PyObject *self, PyObject *args)
{
    char *pMessage;
   
    if(!PyArg_ParseTuple(args, "s", &pMessage))
        return NULL;
    
    Log.Notice("Python", pMessage);
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Log_Error(PyObject *self, PyObject *args)
{
    char *pMessage;
    
    if(!PyArg_ParseTuple(args, "s", &pMessage))
        return NULL;
    
    Log.Error("Python", pMessage);
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Log_Warning(PyObject *self, PyObject *args)
{
    char *pMessage;
    
    if(!PyArg_ParseTuple(args, "s", &pMessage))
        return NULL;
    
    Log.Warning("Python", pMessage);
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Log_Debug(PyObject *self, PyObject *args)
{
    char *pMessage;
    
    if(!PyArg_ParseTuple(args, "s", &pMessage))
        return NULL;
    
    Log.Debug("Python", pMessage);
    
    Py_INCREF(Py_None);
    return Py_None;
}

//create module cfunctions with custom functions
static PyMethodDef cfunctions_methods[] =
{
    {"Log_Notice", (PyCFunction)Log_Notice, METH_VARARGS, NULL},
    {"Log_Warning", (PyCFunction)Log_Warning, METH_VARARGS, NULL},
    {"Log_Error", (PyCFunction)Log_Error, METH_VARARGS, NULL},
    {"Log_Debug", (PyCFunction)Log_Debug, METH_VARARGS, NULL},
    { NULL, NULL, 0, NULL }
};

static PyMethodDef ctransdb_methods[] =
{
    { NULL, NULL, 0, NULL }
};

PythonInterface::PythonInterface(ConfigWatcher &rConfigWatcher) : m_pInstance(NULL), m_pScriptSkeletonModule(NULL), m_lastVersion(g_PythonScriptVersion), m_pythonScriptRunning(false)
{
    rConfigWatcher.addListener(this);
}

bool PythonInterface::run()
{
    CommonFunctions::SetThreadName("PythonInterface thread");
    
    PyObject *pModule;
    PyObject *pName;
    PyObject *pDict;
    PyObject *pClass;
    PyObject *pCallMethodResult;
    PyThreadState *mainThreadState;
    PyThreadState *myThreadState;
    PyThreadState *tempState;
    PyInterpreterState *mainInterpreterState;
    char cBuff[1024];
    bool reloadModule = false;
    //parameters
    const char *pFunctionProto = "(sii)";
   
    while(m_threadRunning)
    {
        //Set the default “home” directory, that is, the location of the standard Python libraries
        if(g_PythonHome.length() != 0)
        {
            Py_SetPythonHome((char*)g_PythonHome.c_str());
        }
        
        // Initialize python interpreter
        Py_Initialize();
        
        //set script directiory
        sprintf(cBuff, "import sys\nsys.path[0]=\"%s\"\n", g_PythonScriptsFolderPath.c_str());
        PyRun_SimpleString(cBuff);
        
        //create module cfunctions with custom functions
        Py_InitModule("cfunctions", cfunctions_methods);
        Py_InitModule("ctransdb", ctransdb_methods);
        
        // Initialize thread support
        PyEval_InitThreads();
        
        // Save a pointer to the main PyThreadState object
        mainThreadState = PyThreadState_Get();
        
        // Get a reference to the PyInterpreterState
        mainInterpreterState = mainThreadState->interp;
        
        // Create a thread state object for this thread
        myThreadState = PyThreadState_New(mainInterpreterState);
        
        // Swap in my thread state
        tempState = PyThreadState_Swap(myThreadState);

        // Now execute some python code (call python functions)
        pName = PyString_FromString(g_PythonModuleName.c_str());
        pModule = PyImport_Import(pName);
        PyErr_Print();
        
        //import skeleton module
        PyObject *pTmp = PyString_FromString("execScriptSkeleton");
        m_pScriptSkeletonModule = PyImport_Import(pTmp);
        Py_DECREF(pTmp);
        PyErr_Print();
        
        //reload
        if(reloadModule)
        {
            //reload module
            pModule = PyImport_ReloadModule(pModule);
            PyErr_Print();
            //reload skeleton module
            m_pScriptSkeletonModule = PyImport_ReloadModule(m_pScriptSkeletonModule);
            PyErr_Print();
        }
        
        //try to reload
        if(pModule == NULL)
        {
            // Swap out the current thread
            PyThreadState_Swap(tempState);
            //
            Log.Error(__FUNCTION__, "Module: %s parse error trying to reload in 10 seconds.", g_PythonModuleName.c_str());
            Wait(10*1000);
        }
        else
        {
            //Log
            Log.Notice(__FUNCTION__,
                       "Loaded python module: %s class: %s, method: %s, version: %s.",
                       g_PythonModuleName.c_str(),
                       g_PythonClassName.c_str(),
                       g_PythonRunableMethod.c_str(),
                       g_PythonScriptVersion.c_str());
            
            // pDict and pFunc are borrowed references
            pDict = PyModule_GetDict(pModule);
            
            // Build the name of a callable class
            pClass = PyDict_GetItemString(pDict, g_PythonClassName.c_str());
            
            // Create an instance of the class
            if (PyCallable_Check(pClass))
            {
                m_pInstance = PyObject_CallObject(pClass, NULL);
            }
            
            //set init
            m_pythonScriptRunning = true;
            
            // Call a method of the class with parameters
            pCallMethodResult = PyObject_CallMethod(m_pInstance,
                                                    (char*)g_PythonRunableMethod.c_str(),
                                                    (char*)pFunctionProto,
                                                    g_ListenHost.c_str(),
                                                    g_ListenPort,
                                                    g_WebSocketPort);
            
            //set init
            m_pythonScriptRunning = false;
            
            // Swap out the current thread
            PyThreadState_Swap(tempState);
            
            // Clean up
            if(pCallMethodResult)
            {
                Py_DECREF(pCallMethodResult);
                pCallMethodResult = NULL;
            }
            if(m_pInstance)
            {
                Py_DECREF(m_pInstance);
                m_pInstance = NULL;
            }
            if(pModule)
            {
                Py_DECREF(pModule);
                pModule = NULL;
            }
            if(m_pScriptSkeletonModule)
            {
                Py_DECREF(m_pScriptSkeletonModule);
                m_pScriptSkeletonModule = NULL;
            }
        }
        
        // Clean up
        if(pName)
        {
            Py_DECREF(pName);
            pName = NULL;
        }
     
        // Clean up thread state
        PyThreadState_Clear(myThreadState);
        PyThreadState_Delete(myThreadState);
        
        //clean up python stuff
        Py_Finalize();
        
        //reload module
        reloadModule = true;
        
        //
        Wait(1000);
    }
    
    return true;
}

static int Python_callOnShutdownForReload(void *pPythonInterfaceArg)
{
    PythonInterface *pPythonInterface = static_cast<PythonInterface*>(pPythonInterfaceArg);
    pPythonInterface->reloadScript();
    return 0;
}

static int Python_callOnShutdown(void *pPythonInterfaceArg)
{
    PythonInterface *pPythonInterface = static_cast<PythonInterface*>(pPythonInterfaceArg);
    pPythonInterface->callOnShutdownPythonMethod();
    return 0;
}

void PythonInterface::callOnShutdownPythonMethod()
{
    PyObject *pCallMethodResult;
    pCallMethodResult = PyObject_CallMethod(m_pInstance, (char*)g_PythonShutdownMethod.c_str(), NULL);
    if(pCallMethodResult)
    {
        Py_DECREF(pCallMethodResult);
        pCallMethodResult = NULL;
    }
}

void PythonInterface::reloadScript()
{
    if(m_pInstance && m_lastVersion != g_PythonScriptVersion)
    {
        callOnShutdownPythonMethod();
        m_lastVersion = g_PythonScriptVersion;
    }
}

void PythonInterface::onConfigReload()
{
    if(m_pythonScriptRunning)
    {
        //call onShutdown to python
        PyGILState_STATE state = PyGILState_Ensure();
        Py_AddPendingCall(&Python_callOnShutdownForReload, this);
        PyGILState_Release(state);
    }
}

void PythonInterface::OnShutdown()
{
    if(m_pythonScriptRunning)
    {
        //call shutdown to python
        PyGILState_STATE state = PyGILState_Ensure();
        Py_AddPendingCall(&Python_callOnShutdown, this);
        PyGILState_Release(state);
    }
    
    //
    ThreadContext::OnShutdown();
}

std::string PythonInterface::executePythonScript(const uint8 *pScriptData, size_t scriptDataSize)
{
    std::string sResult;
    //check if python is running
    if(m_pythonScriptRunning)
    {
        //ensure Python GIL state for this thread
        PyGILState_STATE gstate = PyGILState_Ensure();

        //function to execute
        PyObject *pFunction = PyObject_GetAttrString(m_pScriptSkeletonModule, "executePythonScript");
        //make script arguments - socket ID and python script string
        PyObject *pPythonScript = PyString_FromStringAndSize((const char*)pScriptData, scriptDataSize);
        PyObject *pArgs = PyTuple_Pack(1, pPythonScript);
        //execute
        PyObject *pResult = PyObject_CallObject(pFunction, pArgs);

        //create copy of result and return
        sResult = std::string(PyString_AsString(pResult));
        
        //cleanup
        Py_DECREF(pFunction);
        Py_DECREF(pPythonScript);
        Py_DECREF(pArgs);
        Py_DECREF(pResult);

        PyGILState_Release(gstate);
    }
    else
    {
        sResult = "Python is not running enable python interface in config.";
    }
    return std::move(sResult);
}





























