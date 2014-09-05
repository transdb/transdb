import compiler
import ctransdb
import cfunctions
import time
import zlib
import struct
import pickle
import zlib
import cStringIO

def executePythonScript(storage, LRUCache, fileHandle, script):
    """ Check and execute python script """
    try:
        #create TransDB
        g_transDB = ctransdb.TransDB(storage, LRUCache, fileHandle)
        
        #parse script for checks
        compiler.parse(script)
        #compile script
        objectCode = compiler.compile(script, "execScript", "exec")
        
        #prepare globas and execute script
        dict = globals()
        dict['g_transDB'] = g_transDB
        scriptResult = eval(objectCode, dict, None)
        
        #remove from globals -> avoid memory leak
        del dict['g_transDB']
        
        return str(scriptResult)
    except Exception as e:
        return str(e)


