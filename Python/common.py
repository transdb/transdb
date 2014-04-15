import cfunctions

def CStringToPython(cString):
    try:
        i = 0
        retString = ""
        while cString[i] != '\0':
            retString += cString[i]
            i += 1
        return retString
    except Exception as e:
        cfunctions.Log_Error("CStringToPython: " + str(e))
