import socket

import cfunctions


def set_keepalive_linux(sock, after_idle_sec=1, interval_sec=3, max_fails=5):
    """Set TCP keepalive on an open socket.

        It activates after 1 second (after_idle_sec) of idleness,
        then sends a keepalive ping once every 3 seconds (interval_sec),
        and closes the connection after 5 failed ping (max_fails), or 15 seconds
        """
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, after_idle_sec)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, interval_sec)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, max_fails)
    except Exception as e:
        cfunctions.Log_Error("common.set_keepalive_linux: " + str(e))

def set_keepalive_osx(sock, after_idle_sec=1, interval_sec=3, max_fails=5):
    """Set TCP keepalive on an open socket.

        sends a keepalive ping once every 3 seconds (interval_sec)
        """
    try:
        # scraped from /usr/include, not exported by python's socket module
        TCP_KEEPALIVE = 0x10
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        sock.setsockopt(socket.IPPROTO_TCP, TCP_KEEPALIVE, interval_sec)
    except Exception as e:
        cfunctions.Log_Error("common.set_keepalive_osx: " + str(e))

