import compiler

def executePythonScript(script):
    """ Check and execute python script """
    try:
        #parse script for checks
        compiler.parse(script)
        #compile script
        objectCode = compiler.compile(script, "execScript", "exec")
        #execute script
        scriptResult = eval(objectCode)
        return str(scriptResult)
    except Exception as e:
        return str(e)


