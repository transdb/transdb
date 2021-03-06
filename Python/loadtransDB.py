import BaseHTTPServer
import SocketServer
import time
import socket
import threading
import urlparse
import base64
import zlib
import sys
from string import Template

import transDB
import cfunctions
from templates import *


HTML = "text/html"
CSS = "text/css"
g_authKey = base64.b64encode("transdb:A1b2C3d4")

urls = (("/", "index"),
        ("/fragment", "fragment"),
        ("/config", "config"),
        ("/log", "log"),
        ("/style.css", "css"),
        ("/editor", "editor"),
        ("/pythonScript", "pythonScript"))

def render(template, args):
    """ Render template with provided arguments. """
    return Template(templates[template]).safe_substitute(args)


class TransDBHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    """
    Main request Handler, uses url variable as a mapping between methods and url.
    """
    def css(self, body=None):
        """ Return css style """
        return (templates['style'], None, CSS)
    
    def index(self, body=None):
        """Returns main statistics."""
        buff = ''
        try:
            json_object = transDB.stats()
            
            table = '<table class="table table-hover">'
            table += '<thead><th>Description</th><th>Value</th></thead>'
            for item in json_object:
                table += '<tr><td>%s</td><td>%s</td></li></tr>'%(item["desc"], item["value"])

            table += '</table>'
            buff = render("index", {'title':"TransDB Overview", 'statString':table})
        except (socket.error, RuntimeError, KeyError) as e :
            return (str(e), 500, HTML)

        return (buff, None, HTML)

    def notImpl(self, body=None):
        """ Default method. """
        return ("File or data not found.", 404, HTML)

    def fragment(self, body=None):
        """ Free space fragmentation Info. """
        try:
            data_sz, items, blocks = transDB.fragment()
            
            table = '<table class="table table-hover">'
            table += '<thead><th>Block size</th><th>Block count</th></thead>'
            for k,v in blocks.iteritems():
                table += '<tr><td>%d</td><td>%d</td></li></tr>'%(k, v)

            table += '</table>'
            buff = render("fragment", {'title':"TransDB Fragmentation", 'datasize':data_sz, 'block_count': items, 'blocks':table})
        except (RuntimeError, KeyError) as e:
            return (str(e), 500, HTML)

        return (buff, None, HTML)

    def config(self, body=None):
        """ TransDB configuration. """
        try:
            if not body == None:
                if 'config' in body:
                    raise RuntimeError("Not implemented!")
                else:
                    raise RuntimeError("Invalid request, config parameter not found!")
                        
            #get config
            configString = transDB.readConfig()
                
            textArea = '<form role="form" action="config" method="POST"> \
                            <div class="form-group"> \
                                <textarea name="config" class="form-control" rows="40">' + configString + '</textarea> \
                            </div> \
                            <button type="submit" class="btn btn-default">Save</button> \
                        </form>'
        
            buff = render("config", {'title':"TransDB Configuration", 'config':textArea})
        except (KeyError, RuntimeError, IOError) as e :
            return (str(e), 500, HTML)
        
        return (buff, None, HTML)

    def log(self, body=None):
        """ TransDB logs. """
        try:
            logString = transDB.readLog()
            logString = "<pre>" + logString + "</pre>"
            buff = render("log", {'title':"TransDB Log", 'log':logString})
        except (KeyError, RuntimeError, IOError) as e :
            return(str(e), 500, HTML)
    
        return (buff, None, HTML)

    def editor(self, body=None):
        """ TransDB editor. """
        try:
            x = ''
            y = ''
            textAreaString = ''
            
            if not body == None:
                if 'x' in body and 'y' in body:
                    x = body['x']
                    y = body['y']
                    if 'action' in body and 'value' in body and body['action'] == 'save':
                        value = body['value']
                        #send write
                        transDB.writeData(x, y, value)
                else:
                    raise RuntimeError("Missing X, Y")
        
            #send read request
            if not x == '' and not y == '':
                textAreaString = transDB.readData(x, y)
            
            editorForm = '<form role="form" action="editor" method="POST">  \
                            <div class="form-group">    \
                                <label for="x">X Key - uint64</label> \
                                <input class="form-control"type="text" name="x" placeholder="Enter X key" value="' + x + '" /> \
                            </div>  \
                            <div class="form-group">    \
                                <label for="y">Y Key - uint64</label> \
                                <input class="form-control"type="text" name="y" placeholder="Enter Y key" value="' + y + '" /> \
                            </div>  \
                            <div class="form-group">    \
                                <label for="y">Action</label> \
                                <select name="action" class="form-control"> \
                                    <option value="load">Load</option> \
                                    <option value="save">Save</option> \
                                </select>\
                            </div>  \
                            <div class="form-group">    \
                                <label for="value">Value</label> \
                                <textarea class="form-control" name="value" rows="15" placeholder="Enter value">' + textAreaString + '</textarea>     \
                            </div>  \
                            <button type="submit" class="btn btn-default">Submit</button>          \
                         </form>'
            
            buff = render("editor", {'title':"TransDB Log", 'editor':editorForm})
        except (KeyError, RuntimeError, IOError, ValueError) as e :
            return(str(e), 500, HTML)
        
        return (buff, None, HTML)

    def pythonScript(self, body=None):
        """ Execute SQL """
        try:
            #fill with script skeleton
            with open(sys.path[0] + "/scriptSkeleton.py", "r") as src :
                pythonScript = src.read()
            
            scriptResult = "No result"
            if not body == None and 'pythonScript' in body:
                pythonScript = body['pythonScript']
                scriptResult = transDB.executePythonScript(pythonScript)

            pythonForm = '<form role="form" action="pythonScript" method="POST">  \
                        <div class="form-group">    \
                            <label for="value">Python script</label> \
                            <textarea class="form-control" name="pythonScript" rows="15" placeholder="Enter Python Script">' + pythonScript + '</textarea>     \
                        </div>  \
                        <div class="form-group">    \
                            <label for="result">Script result</label> \
                            <pre name="result">' + scriptResult + '</pre> \
                        </div>  \
                        <button type="submit" class="btn btn-default">Submit</button>          \
                       </form>'
    
            buff = render("pythonScript", {'title':"TransDB Python Script", 'pythonForm':pythonForm})
        except (KeyError, RuntimeError, IOError, ValueError) as e :
            return(str(e), 500, HTML)

        return (buff, None, HTML)

    def do_AUTHHEAD(self):
        self.send_response(401)
        self.send_header('WWW-Authenticate', 'Basic realm=\"TransDB - Web Access\"')
        self.send_header('Content-type', 'text/html')
        self.end_headers()

    def do_REQ(self, body=None):
        """
        Process a request.

        Find a valid url which is in the global variable URLS and call appropriate method.
        URLS is a tuple of tuples in form (URL, method name), if none is find return 404.
        if the BODY is not empty it will pass it to the method, that's the case for POST calls.
        """
        buff = ''
        method = TransDBHandler.notImpl;
        for url, mtd in urls:
            if(self.path == url):
                method = getattr(TransDBHandler, mtd)
    
        buff, e, typ = method(self, body)
        if not e == None :
            try:
                err = render("error", {'title': "Error", 'code': e, 'error': buff})

                self.send_response(e)
                self.send_header("Content-type", typ)
                self.send_header("Content-Length", str(len(err)))
                self.send_header("Last-Modified", self.date_time_string(time.time()))
                self.end_headers()
                self.wfile.write(err)
            
            except (KeyError, RuntimeError) as e :
                self.send_error(500, str(e))
        else:
            #gzip
            encoder = zlib.compressobj(8, zlib.DEFLATED, 16+zlib.MAX_WBITS, zlib.DEF_MEM_LEVEL, 0)
            buffGZIP = encoder.compress(buff)
            buffGZIP += encoder.flush()
            buff = buffGZIP
            #headers
            self.send_response(200)
            self.send_header("Content-type", typ)
            self.send_header("Content-Length", str(len(buff)))
            self.send_header("Content-Encoding", "gzip")
            self.send_header("Last-Modified", self.date_time_string(time.time()))
            self.end_headers()
            self.wfile.write(buff)
        

    def do_GET(self):
        """ Handle GET requests """
        #Authorization
        if self.headers.getheader('Authorization') == None:
            self.do_AUTHHEAD()
            self.wfile.write('no auth header received')
        elif self.headers.getheader('Authorization') == 'Basic ' + g_authKey:
            self.do_REQ()
        else:
            self.do_AUTHHEAD()
            self.wfile.write(self.headers.getheader('Authorization'))
            self.wfile.write('not authenticated')
    
    def do_POST(self):
        """ 
        Handle POST request. 

        BODY is a dictionary where keys are the form variables as received in the request.
        """
        content_len = int(self.headers.getheader("Content-Length"))
        body = dict((k, v if len(v)>1 else v[0]) for k, v in urlparse.parse_qs(self.rfile.read(content_len)).iteritems())
        self.do_REQ(body)

    #do not delete
    def log_message(self, format, *args):
        return

def http_socket_run(stop_event, listenHost, listenPort):
    try:
        #start HTTP interface
        SocketServer.TCPServer.allow_reuse_address = True
        httpd = SocketServer.TCPServer((listenHost, listenPort), TransDBHandler)
        httpd.timeout = 10
        while not stop_event.is_set():
            httpd.handle_request()
        httpd.server_close()
    except Exception as e:
        cfunctions.Log_Error("http_socket_run: " + str(e))

class LoadTransDB:

    def __init__(self):
        self.transdbThread = None
        self.httpdThread = None
        self.stopEvent = threading.Event()

    def run(self, transDBListenHost, transDBListenPort, webServiceListenHost, webServiceListenPort):
        try:
            #start client socket thread - windows ip connect fix
            trandbSocketConnectIP = "127.0.0.1"
            if transDBListenHost != "0.0.0.0":
                trandbSocketConnectIP = transDBListenHost
        
            #start TrandDB com thread
            self.transdbThread = threading.Thread(target=transDB.socket_run, args=[transDB.recvQueue, transDB.sendQueue, self.stopEvent,
                                                                                   trandbSocketConnectIP, transDBListenPort])
            self.transdbThread.start()
            
            #start HTTP interface thread
            self.httpdThread = threading.Thread(target=http_socket_run, args=[self.stopEvent, webServiceListenHost, webServiceListenPort])
            self.httpdThread.start()
    
            #loop until shutdown
            while not self.stopEvent.is_set():
                self.stopEvent.wait(1)
    
        except Exception as e:
            cfunctions.Log_Error("LoadTransDB.run: " + str(e))

    def shutdown(self):
        try:
            self.stopEvent.set()
            self.httpdThread.join()
            self.transdbThread.join()
        except Exception as e:
            cfunctions.Log_Error("LoadTransDB.shutdown: " + str(e))

if __name__ == '__main__':
    try:
        ld = LoadTransDB()
        ld.run("localhost", 5555, 8888)
    except KeyboardInterrupt as e:
        print("\nSIGINT caught ... shutdown")
        ld.shutdown()
