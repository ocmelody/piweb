#! /usr/bin/env python
import logging
import math
import time
import Queue
import threading
import RPi.GPIO as GPIO

debug=0
class RobotController:
    def __init__(self):
        self.lastUpdateTime = 0.0
        self.lastMotionCommandTime = time.time()
        self.MAX_UPDATE_TIME_DIFF = 0.25
        self.LEFT_FORWARD = 27
        self.LEFT_BACKWARD= 17
        self.RIGHT_FORWARD = 2
        self.RIGHT_BACKWARD= 3
	self.setup()

    def setup(self):
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(self.LEFT_FORWARD, GPIO.OUT)
        GPIO.setup(self.LEFT_BACKWARD, GPIO.OUT)
        GPIO.setup(self.RIGHT_FORWARD, GPIO.OUT)
        GPIO.setup(self.RIGHT_BACKWARD, GPIO.OUT)

    def forward(self,x):
        print "forwarding running  motor "
        GPIO.output(self.LEFT_FORWARD, GPIO.HIGH)
        GPIO.output(self.RIGHT_FORWARD, GPIO.HIGH)
        time.sleep(x)
        GPIO.output(self.LEFT_FORWARD, GPIO.LOW)
        GPIO.output(self.RIGHT_FORWARD, GPIO.LOW)

    def backward(self,x):
        GPIO.output(self.LEFT_BACKWARD, GPIO.HIGH)
        GPIO.output(self.RIGHT_BACKWARD, GPIO.HIGH)
        print "backwarding running motor"
        time.sleep(x)
        GPIO.output(self.LEFT_BACKWARD, GPIO.LOW)
        GPIO.output(self.RIGHT_BACKWARD, GPIO.LOW)

    def left(self,x):
        print "left forwarding running  motor "
        GPIO.output(self.LEFT_FORWARD, GPIO.HIGH)
        time.sleep(x)
        GPIO.output(self.LEFT_FORWARD, GPIO.LOW)

    def l_backward(self,x):
        print "left forwarding running  motor "
        GPIO.output(self.LEFT_FORWARD, GPIO.HIGH)
        time.sleep(x)
        GPIO.output(self.LEFT_FORWARD, GPIO.LOW)

    def right(self,x):
        print "right forwarding running  motor "
        GPIO.output(self.RIGHT_FORWARD, GPIO.HIGH)
        time.sleep(x)
        GPIO.output(self.RIGHT_FORWARD, GPIO.LOW)

    def hello(self):
	print "Hello"

    def __del__( self ):
        self.disconnect()
    
    def disconnect( self ):
        print "Closing"
        GPIO.cleanup()
       
    def control(self, cmd):
        if cmd == "l":
			print "Left"
			self.left(1)
        elif cmd == "r":
			print "Right"
			self.right(1)
        elif cmd == "f":
			print "Fwd"
			self.forward(1)
        elif cmd == "b":
			print "Back"
			self.backward(1)
        else:
			print "Stop"
			#gopigo.stop()
		

    def update( self ):
        if debug:	
			print "Updating"
        curTime = time.time()
        timeDiff = min( curTime - self.lastUpdateTime, self.MAX_UPDATE_TIME_DIFF )
        self.lastUpdateTime = curTime
