import time
import RPi.GPIO as GPIO

class RobotCar:
    def __init__(self):
        self.LEFT_FORWARD = 27
        self.LEFT_BACKWARD= 17
        self.RIGHT_FORWARD = 2
        self.RIGHT_BACKWARD= 3

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

    def close(self):
        print "clean up"
        GPIO.cleanup()

    def hello(self):
	print "Hello"

'''
#print "reverse motor"

car = RobotCar()
car.setup()

print "Press:\n\tw: Move PiCar Robot forward\n\ta: Turn PiCar Robot left\n\td: Turn PiCar Robot right\n\ts: Move PiCar Robot backward\n\ts: Stop PiCar Robot\n\tz: Exit\n"
while True:
        print "Enter the Command:",
        a=raw_input()   # Fetch the input from the terminal
        if a=='w':
                car.forward(1)   # Move forward
        elif a=='a':
                car.left(1)  # Turn left
        elif a=='d':
                car.right(1) # Turn Right
        elif a=='s':
                car.backward(1)   # Move back
        elif a=='z':
                print "Exiting"    # Exit
                car.close()
		break
        else:
                print "Wrong Command, Please Enter Again"
        time.sleep(.1)
'''
