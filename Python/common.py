import cfunctions
import socket

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

def set_keepalive_linux(sock, after_idle_sec=1, interval_sec=3, max_fails=5):
    try:
        """Set TCP keepalive on an open socket.
            
            It activates after 1 second (after_idle_sec) of idleness,
            then sends a keepalive ping once every 3 seconds (interval_sec),
            and closes the connection after 5 failed ping (max_fails), or 15 seconds
            """
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, after_idle_sec)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, interval_sec)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, max_fails)
    except Exception as e:
        cfunctions.Log_Error("set_keepalive_linux: " + str(e))