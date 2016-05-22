echo "Check for internet connectivity..."
echo "=================================="
wget -q --tries=2 --timeout=20 http://google.com
if [ $? -eq 0 ];then
	echo "Connected"
else
	echo "Unable to Connect, try again !!!"
	exit 0
fi

#Installing Mjpeg streamer http://blog.miguelgrinberg.com/post/how-to-build-and-run-mjpg-streamer-on-the-raspberry-pi
sudo apt-get update
sudo apt-get install libjpeg8-dev imagemagick libv4l-dev
sudo ln -s /usr/include/linux/videodev2.h /usr/include/linux/videodev.h
wget http://sourceforge.net/code-snapshots/svn/m/mj/mjpg-streamer/code/mjpg-streamer-code-182.zip
unzip mjpg-streamer-code-182.zip
cd mjpg-streamer-code-182/mjpg-streamer
make mjpg_streamer input_file.so output_http.so
sudo cp mjpg_streamer /usr/local/bin
sudo cp output_http.so input_file.so /usr/local/lib/
sudo cp -R www /usr/local/www
mkdir /tmp/stream
cd ../../

rm -rf mjpg-streamer-182
rm -rf mjpg-streamer-code-182
rm index.html
rm mjpg-streamer-code-182.zip

git clone https://bitbucket.org/DexterIndustries/raspberry_pi_camera_streamer.git
cd raspberry_pi_camera_streamer
mkdir build
cd build
cmake ../
make
sudo make install
cd ../../

rm -R raspberry_pi_camera_streamer

sudo pip install tornado
git clone https://github.com/DexterInd/sockjs-tornado
cd sockjs-tornado
sudo python setup.py install
cd ..
rm -R sockjs-tornado

echo " "
echo "Restarting"
echo "3"
sleep 1
echo "2"
sleep 1
echo "1"
sleep 1
shutdown -r now
