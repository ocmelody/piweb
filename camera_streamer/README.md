This program provides an easy way to stream camera images from a Raspberry Pi
over a network. At the moment it encodes the camera images as a MJPEG stream
and provides a simple HTTP server that allows them to be viewed in a web
browser.

Installation
------------

Run the following to install the necessary dependencies

    sudo apt-get update
    sudo apt-get install gcc build-essential cmake vlc rpi-update


Colone the code, and then compile and install using the standard cmake way

    cd camera_streamer
    mkdir build
    cd build
    cmake ../
    make
    sudo make install

Running
-------

Start up the streamer by running

    raspberry_pi_camera_streamer

View the stream in a webbrowser using the following address

    http://RASPBERRY_PI_IP_ADDRESS:8080/?action=stream

where RASPBERRY_PI_IP_ADDRESS is replaced by the IP address of your Pi
