import compiler
import ctransdb

def executePythonScript(storage, LRUCache, fileHandle, script):
    """ Check and execute python script """
    try:
        #create TransDB
        g_transDB = ctransdb.TransDB(storage, LRUCache, fileHandle)
        
        #parse script for checks
        compiler.parse(script)
        #compile script
        objectCode = compiler.compile(script, "execScript", "exec")
        #execute script
        scriptResult = eval(objectCode, None, {'g_transDB':g_transDB})
        return str(scriptResult)
    except Exception as e:
        return str(e)


