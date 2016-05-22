import RPi.GPIO as GPIO
import sys, tty, termios, os
import time

class RobotController:
    def __init__(self):
        self.LEFT_FORWARD = 2
        self.LEFT_BACKWARD= 3
        self.LEFT_PWM = 4
        self.RIGHT_FORWARD = 27
        self.RIGHT_BACKWARD= 17
        self.RIGHT_PWM = 18
	self.speedleft = 0 
	self.speedright = 0 
        self.PWM_MAX = 100
        self.STEP = 0.2
        self.setup()

    def setup(self):
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(self.LEFT_FORWARD, GPIO.OUT)
        GPIO.setup(self.LEFT_BACKWARD, GPIO.OUT)
        GPIO.setup(self.LEFT_PWM, GPIO.OUT)
        GPIO.setup(self.RIGHT_FORWARD, GPIO.OUT)
        GPIO.setup(self.RIGHT_BACKWARD, GPIO.OUT)
        GPIO.setup(self.RIGHT_PWM, GPIO.OUT)
        self.leftmotorpwm = GPIO.PWM(self.LEFT_PWM,100)
        self.rightmotorpwm = GPIO.PWM(self.RIGHT_PWM,100)
        self.leftmotorpwm.start(0)
	self.leftmotorpwm.ChangeDutyCycle(0)
	self.rightmotorpwm.start(0)
	self.rightmotorpwm.ChangeDutyCycle(0)

# setMotorMode()
# Sets the mode for the L298 H-Bridge which motor is in which mode.
# This is a short explanation for a better understanding:
# motor         -> which motor is selected left motor or right motor
# mode          -> mode explains what action should be performed by the H-Bridge
# setMotorMode(leftmotor, reverse)      -> The left motor is called by a function and set into reverse mode
# setMotorMode(rightmotor, stop)       -> The right motor is called by a function and set into stop mode
    def setMotorMode(self, motor, mode):
        if motor == "leftmotor":
                if mode == "reverse":
                        GPIO.output(self.LEFT_BACKWARD, GPIO.HIGH)
                        GPIO.output(self.LEFT_FORWARD,  GPIO.LOW)
                elif  mode == "forward":
                        GPIO.output(self.LEFT_BACKWARD, GPIO.LOW)
                        GPIO.output(self.LEFT_FORWARD,  GPIO.HIGH)
                else:
                        GPIO.output(self.LEFT_BACKWARD, GPIO.LOW)
                        GPIO.output(self.LEFT_FORWARD,  GPIO.LOW)

        elif motor == "rightmotor":
                if mode == "reverse":
                        GPIO.output(self.RIGHT_BACKWARD, GPIO.HIGH)
                        GPIO.output(self.RIGHT_FORWARD,  GPIO.LOW)
                elif  mode == "forward":
                        GPIO.output(self.RIGHT_BACKWARD, GPIO.LOW)
                        GPIO.output(self.RIGHT_FORWARD,  GPIO.HIGH)
                else:
                        GPIO.output(self.RIGHT_BACKWARD, GPIO.LOW)
                        GPIO.output(self.RIGHT_FORWARD,  GPIO.LOW)
        else:
        	GPIO.output(self.LEFT_FORWARD, GPIO.LOW)
        	GPIO.output(self.RIGHT_FORWARD, GPIO.LOW)
        	GPIO.output(self.LEFT_BACKWARD, GPIO.LOW)
        	GPIO.output(self.RIGHT_BACKWARD, GPIO.LOW)

# Sets the drive level for the left motor, from +1 (max) to -1 (min).
# This is a short explanation for a better understanding:
# SetMotorLeft(0)     -> left motor is stoped
# SetMotorLeft(0.75)  -> left motor moving forward at 75% power
# SetMotorLeft(-0.5)  -> left motor moving reverse at 50% power
# SetMotorLeft(1)     -> left motor moving forward at 100% power
    def setMotorLeft(self, power):
        print "LEFT POWER=" + str(power)
        if power < 0:
                # Reverse mode for the left motor
                self.setMotorMode("leftmotor", "reverse")
                pwm = -int(self.PWM_MAX * power)
                if pwm > self.PWM_MAX:
                        pwm = self.PWM_MAX
        elif power > 0:
                # Forward mode for the left motor
                self.setMotorMode("leftmotor", "forward")
                pwm = int(self.PWM_MAX * power)
                if pwm > self.PWM_MAX:
                        pwm = self.PWM_MAX
        else:
                # Stopp mode for the left motor
                self.setMotorMode("leftmotor", "stop")
                pwm = 0
        print "SetMotorLeft", pwm
        self.leftmotorpwm.ChangeDutyCycle(pwm)

# Sets the drive level for the right motor, from +1 (max) to -1 (min).
# This is a short explanation for a better understanding:
# SetMotorRight(0)     -> right motor is stoped
# SetMotorRight(0.75)  -> right motor moving forward at 75% power
# SetMotorRight(-0.5)  -> right motor moving reverse at 50% power
# SetMotorRight(1)     -> right motor moving forward at 100% power
    def setMotorRight(self, power):
        print "RIGHT POWER=" + str(power)
        if power < 0:
                # Reverse mode for the right motor
                self.setMotorMode("rightmotor", "reverse")
                pwm = -int(self.PWM_MAX * power)
                if pwm > self.PWM_MAX:
                        pwm = self.PWM_MAX
        elif power > 0:
                # Forward mode for the right motor
                self.setMotorMode("rightmotor", "forward")
                pwm = int(self.PWM_MAX * power)
                if pwm > self.PWM_MAX:
                        pwm = self.PWM_MAX
        else:
                # Stopp mode for the right motor
                self.setMotorMode("rightmotor", "stop")
                pwm = 0
        print "SetMotorRight", pwm
        self.rightmotorpwm.ChangeDutyCycle(pwm)

    def close(self):
	self.setMotorLeft(0)
	self.setMotorRight(0)
        #print "clean up"
        #GPIO.cleanup()

    def forward(self):
	# synchronize after a turning the motor speed
	# if speedleft > speedright:
		# speedleft = speedright
	
	# if self.speedright > self.speedleft:
		# self.speedright = self.speedleft
			
	# accelerate the RaPi car
	self.speedleft = self.speedleft + self.STEP
	self.speedright = self.speedright + self.STEP

	if self.speedleft > 1:
		self.speedleft = 1
	if self.speedright > 1:
		self.speedright = 1
	
	self.setMotorLeft(self.speedleft)
	self.setMotorRight(self.speedright)

    def backward(self):
	# synchronize after a turning the motor speed
		
	# if self.speedleft > self.speedright:
		# self.speedleft = self.speedright
		
	# if self.speedright > self.speedleft:
		# self.speedright = self.speedleft
		
	# slow down the RaPi car
	self.speedleft = self.speedleft - self.STEP
	self.speedright = self.speedright - self.STEP

	if self.speedleft < -1:
		self.speedleft = -1
	if self.speedright < -1:
		self.speedright = -1
	
	self.setMotorLeft(self.speedleft)
	self.setMotorRight(self.speedright)

    def stop(self):
	self.speedleft = 0
	self.speedright = 0
	self.setMotorLeft(self.speedleft)
	self.setMotorRight(self.speedright)

    def right(self):
	#if self.speedright > self.speedleft:
	self.speedright = self.speedright - self.STEP
	self.speedleft = self.speedleft + self.STEP
	
	if self.speedright < -1:
		self.speedright = -1
	
	if self.speedleft > 1:
		self.speedleft = 1
	
	self.setMotorLeft(self.speedleft)
	self.setMotorRight(self.speedright)

    def left(self):
	#if self.speedleft > self.speedright:
	self.speedleft = self.speedleft - self.STEP
	self.speedright = self.speedright + self.STEP
		
	if self.speedleft < -1:
		self.speedleft = -1
	
	if self.speedright > 1:
		self.speedright = 1
	
	self.setMotorLeft(self.speedleft)
	self.setMotorRight(self.speedright)

    def __del__( self ):
        print "Closing"
        self.close()
       
    def control(self, cmd):
        if cmd == "l":
		print "Left"
		self.left()
        elif cmd == "r":
		print "Right"
		self.right()
        elif cmd == "f":
		print "Fwd"
		self.forward()
        elif cmd == "b":
		print "Back"
		self.backward()
        else:
		print "Stop"
		self.stop()
		
    #class method can determine which key has been pressed
    # by the user on the keyboard.
    def getch(self):
    	fd = sys.stdin.fileno()
    	old_settings = termios.tcgetattr(fd)
    	try:
        	tty.setraw(sys.stdin.fileno())
        	ch = sys.stdin.read(1)
    	finally:
        	termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    	return ch

    def printscreen(self):
        # Print the motor speed just for interest
        os.system('clear')
        print("w/s: direction")
        print("a/d: steering")
        print("q: stops the motors")
        print("x: exit")
        print("========== Speed Control ==========")
        print "left motor:  ", self.speedleft
        print "right motor: ", self.speedright

    # Infinite loop
    # The loop will not end until the user presses the
    # exit key 'X' or the program crashes...
    def test(self):
      while True:
    # Keyboard character retrieval method. This method will save
    # the pressed key into the variable char
	char = self.getch()
	
	# The car will drive forward when the "w" key is pressed
	if(char == "w"):
		self.forward()
		self.printscreen()
	
    	# The car will reverse when the "s" key is pressed
	if(char == "s"):
		self.backward()
		self.printscreen()

	# Stop the motors
	if(char == "q"):
		self.stop()
		self.printscreen()

        # The "d" key will toggle the steering right
	if(char == "d"):		
		self.right()
		self.printscreen()

        # The "a" key will toggle the steering left
	if(char == "a"):
		self.left()
		self.printscreen()
		
	# The "x" key will break the loop and exit the program
	if(char == "x"):
		self.clsoe()
		break
	
        # The keyboard character variable char has to be set blank. We need
	# to set it blank to save the next key pressed by the user
	char = ""

#testing 
#car = RobotController()
#car.test()
# End
