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

//transdb interface
typedef struct
{
    PyObject_HEAD
    Storage     *m_pStorage;
    LRUCache    *m_pLRUCache;
    HANDLE      m_rDataFileHandle;
} TransDB;

static void TransDB_dealloc(TransDB* self)
{
    self->ob_type->tp_free((PyObject*)self);
}

static PyObject *TransDB_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    TransDB *self;
    
    self = (TransDB *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->m_pStorage = NULL;
        self->m_pLRUCache = NULL;
        self->m_rDataFileHandle = INVALID_HANDLE_VALUE;
    }
    
    return (PyObject *)self;
}

static int TransDB_init(TransDB *self, PyObject *args, PyObject *kwds)
{
    PyObject *pStorage;
    PyObject *pLRUCache;
    PyObject *pFileHandle;
 
    static const char *kwlist[] = {"storage", "LRUCache", "fileHandle", NULL};
    
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "OOO", (char**)kwlist, &pStorage, &pLRUCache, &pFileHandle))
    {
        return -1;
    }
    
    //init variables
    self->m_pStorage = (Storage*)PyCObject_AsVoidPtr(pStorage);
    self->m_pLRUCache = (LRUCache*)PyCObject_AsVoidPtr(pLRUCache);
    void *pHandle = PyCObject_AsVoidPtr(pFileHandle);
    memcpy(&self->m_rDataFileHandle, pHandle, sizeof(HANDLE));
    
    return 0;
}


static PyObject *TransDB_ReadData(TransDB *self, PyObject *args, PyObject *kwds)
{
    uint64 x;
    uint64 y;
    
    static const char *kwlist[] = {"x", "y", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "LL", (char**)kwlist, &x, &y))
    {
        return NULL;
    }
    
    //read data
    ByteBuffer rData;
    if(y != 0)
        self->m_pStorage->ReadData(self->m_rDataFileHandle, *self->m_pLRUCache, x, y, rData);
    else
        self->m_pStorage->ReadData(self->m_rDataFileHandle, *self->m_pLRUCache, x, rData);
    
    //return PyByteArray
    PyObject *pPyData = PyByteArray_FromStringAndSize((const char*)rData.contents(), rData.size());
    return pPyData;
}

static PyObject *TransDB_WriteData(TransDB *self, PyObject *args, PyObject *kwds)
{
    uint64 x;
    uint64 y;
    char *pData;
    int recordSize;
    
    static const char *kwlist[] = {"x", "y", "data", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "LLs#", (char**)kwlist, &x, &y, &pData, &recordSize))
    {
        return NULL;
    }
    
    //write
    uint32 status = self->m_pStorage->WriteData(self->m_rDataFileHandle, *self->m_pLRUCache, x, y, (const uint8*)pData, recordSize);
    return PyInt_FromLong(status);
}

static PyObject *TransDB_DeleteData(TransDB *self, PyObject *args, PyObject *kwds)
{
    uint64 x;
    uint64 y;
    
    static const char *kwlist[] = {"x", "y", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "LL", (char**)kwlist, &x, &y))
    {
        return NULL;
    }
    
    //delete
    if(y != 0)
        self->m_pStorage->DeleteData(self->m_rDataFileHandle, *self->m_pLRUCache, x, y);
    else
        self->m_pStorage->DeleteData(*self->m_pLRUCache, x);
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *TransDB_GetAllY(TransDB *self, PyObject *args, PyObject *kwds)
{
    uint64 x;
    
    static const char *kwlist[] = {"x", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "L", (char**)kwlist, &x))
    {
        return NULL;
    }
    
    //read all Y
    ByteBuffer rY;
    self->m_pStorage->GetAllY(self->m_rDataFileHandle, *self->m_pLRUCache, x, rY);
    
    //return PyByteArray
    PyObject *pPyData = PyByteArray_FromStringAndSize((const char*)rY.contents(), rY.size());
    return pPyData;
}

static PyMethodDef TransDB_methods[] =
{
    {"ReadData", (PyCFunction)TransDB_ReadData, METH_KEYWORDS, "Read data from transDB"},
    {"WriteData", (PyCFunction)TransDB_WriteData, METH_KEYWORDS, "Write data to transDB"},
    {"DeleteData", (PyCFunction)TransDB_DeleteData, METH_KEYWORDS, "Delete data from transDB"},
    {"GetAllY", (PyCFunction)TransDB_GetAllY, METH_KEYWORDS, "Get all Y keys in X"},
    {NULL}  /* Sentinel */
};

static PyTypeObject TransDBType =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "ctransdb.TransDB",        /*tp_name*/
    sizeof(TransDB),           /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)TransDB_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "TransDB objects",         /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    TransDB_methods,           /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)TransDB_init,    /* tp_init */
    0,                         /* tp_alloc */
    TransDB_new,               /* tp_new */
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
        //ctransdb
        int status = PyType_Ready(&TransDBType);
        if(status != 0)
        {
            PyErr_Print();
        }
        PyObject *m = Py_InitModule3("ctransdb", ctransdb_methods, "Module constains interface for work with transDB");
        Py_INCREF(&TransDBType);
        PyModule_AddObject(m, "TransDB", (PyObject*)&TransDBType);
        
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

std::string PythonInterface::executePythonScript(Storage *pStorage,
                                                 LRUCache *pLRUCache,
                                                 HANDLE hDataFileHandle,
                                                 const uint8 *pScriptData,
                                                 size_t scriptDataSize)
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
        PyObject *pPyStoragePtr = PyCObject_FromVoidPtr(pStorage, NULL);
        PyObject *pPypLRUCachePtr = PyCObject_FromVoidPtr(pLRUCache, NULL);
        //save file handle to tmp varibale and pass to script
        uint8 rHandle[sizeof(HANDLE)];
        memcpy(&rHandle, &hDataFileHandle, sizeof(HANDLE));
        PyObject *pPyDataFileHandlePtr = PyCObject_FromVoidPtr(&rHandle, NULL);
        
        PyObject *pPythonScript = PyString_FromStringAndSize((const char*)pScriptData, scriptDataSize);
        PyObject *pArgs = PyTuple_Pack(4, pPyStoragePtr, pPypLRUCachePtr, pPyDataFileHandlePtr, pPythonScript);
        //execute
        PyObject *pResult = PyObject_CallObject(pFunction, pArgs);

        //create copy of result and return
        sResult = std::string(PyString_AsString(pResult));
        
        //cleanup
        Py_DECREF(pFunction);
        Py_DECREF(pPyStoragePtr);
        Py_DECREF(pPypLRUCachePtr);
        Py_DECREF(pPyDataFileHandlePtr);
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





























