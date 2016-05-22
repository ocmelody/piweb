#! /usr/bin/env python
import logging

LOG_FILENAME = "/tmp/robot_web_server_log.txt"
file_location = "/home/pi/projects/piweb/www/"
logging.basicConfig( filename=LOG_FILENAME, level=logging.DEBUG )

# Also log to stdout
consoleHandler = logging.StreamHandler()
consoleHandler.setLevel( logging.DEBUG )
logging.getLogger( "" ).addHandler( consoleHandler )

import os.path
import time
import signal
import tornado.httpserver
import tornado.ioloop
import tornado.web
import tornado.escape
import sockjs.tornado
import threading
import Queue
import camera_streamer
import robot_controller
import json
import subprocess
import sys
robot = None

cameraStreamer = None
scriptPath = os.path.dirname( __file__ )
webPath = os.path.abspath( file_location)
print webPath
robotConnectionResultQueue = Queue.Queue()
isClosing = False

#--------------------------------------------------------------------------------------------------- 
def createRobot(resultQueue ):
    
    r = robot_controller.RobotController( )
    resultQueue.put( r )
            
#--------------------------------------------------------------------------------------------------- 
class ConnectionHandler( sockjs.tornado.SockJSConnection ):
    
    #-----------------------------------------------------------------------------------------------
    def on_open( self, info ):
        pass
        
    #-----------------------------------------------------------------------------------------------
    def on_message( self, message ):
                
        try:
            message = str( message )
        except Exception:
            logging.warning( "Got a message that couldn't be converted to a string" )
            return

        if isinstance( message, str ):
            
            lineData = message.split( " " )
            if len( lineData ) > 0:
                
                if lineData[ 0 ] == "Centre":
                
                    if robot != None:
                        robot.centreNeck()
                
                elif lineData[ 0 ] == "StartStreaming":
                    cameraStreamer.startStreaming()
                    
                elif lineData[ 0 ] == "Shutdown":
                    cameraStreamer.stopStreaming()
                    robot.disconnect()
                    sys.exit()
                
                elif lineData[ 0 ] == "Move" and len( lineData ) >= 2:
                    
                    print lineData[1]
                    if robot != None:
                        robot.control(lineData[1])
                        
    #-----------------------------------------------------------------------------------------------
    def on_close( self ):
        logging.info( "SockJS connection closed" )

#--------------------------------------------------------------------------------------------------- 
class MainHandler( tornado.web.RequestHandler ):
    
    #------------------------------------------------------------------------------------------------
    def get( self ):
        self.render( webPath + "/index.html" )
        
#--------------------------------------------------------------------------------------------------- 
def robotUpdate():
    
    global robot
    global isClosing
    
    if isClosing:
        tornado.ioloop.IOLoop.instance().stop()
        return
        
    if robot == None:
        if not robotConnectionResultQueue.empty():
            robot = robotConnectionResultQueue.get()
    #else:
    #    robot.update()

#--------------------------------------------------------------------------------------------------- 
def signalHandler( signum, frame ):
    
    if signum in [ signal.SIGINT, signal.SIGTERM ]:
        global isClosing
        isClosing = True
        
        
#--------------------------------------------------------------------------------------------------- 
if __name__ == "__main__":
    
    signal.signal( signal.SIGINT, signalHandler )
    signal.signal( signal.SIGTERM, signalHandler )
    
    # Create the configuration for the web server
    router = sockjs.tornado.SockJSRouter( 
        ConnectionHandler, '/robot_control' )
    application = tornado.web.Application( router.urls + [ 
        ( r"/", MainHandler ), 
        ( r"/(.*)", tornado.web.StaticFileHandler, { "path": webPath } ),
        ( r"/css/(.*)", tornado.web.StaticFileHandler, { "path": webPath + "/css" } ),
        ( r"/css/images/(.*)", tornado.web.StaticFileHandler, { "path": webPath + "/css/images" } ),
        ( r"/images/(.*)", tornado.web.StaticFileHandler, { "path": webPath + "/images" } ),
        ( r"/js/(.*)", tornado.web.StaticFileHandler, { "path": webPath + "/js" } ) ] )
    
    #( r"/(.*)", tornado.web.StaticFileHandler, {"path": scriptPath + "/www" } ) ] \
    
    # Create a camera streamer
    cameraStreamer = camera_streamer.CameraStreamer()
    
    # Start connecting to the robot asyncronously
    robotConnectionThread = threading.Thread( target=createRobot, 
        args=[ robotConnectionResultQueue ] )
        #args=[ robotConfig, robotConnectionResultQueue ] )
    robotConnectionThread.start()

    # Now start the web server
    logging.info( "Starting web server..." )
    http_server = tornado.httpserver.HTTPServer( application )
	
    #The port number on which the robot control works, change in line 105 in www/index.html too
    http_server.listen( 9000 )
    
    robotPeriodicCallback = tornado.ioloop.PeriodicCallback( 
        robotUpdate, 100, io_loop=tornado.ioloop.IOLoop.instance() )
    robotPeriodicCallback.start()
    
    cameraStreamerPeriodicCallback = tornado.ioloop.PeriodicCallback( 
        cameraStreamer.update, 1000, io_loop=tornado.ioloop.IOLoop.instance() )
    cameraStreamerPeriodicCallback.start()
    
    tornado.ioloop.IOLoop.instance().start()
    
    # Shut down code
    robotConnectionThread.join()
    
    if robot != None:
        robot.disconnect()
    else:
        if not robotConnectionResultQueue.empty():
            robot = robotConnectionResultQueue.get()
            robot.disconnect()
            
    cameraStreamer.stopStreaming()
