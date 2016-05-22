cmake_minimum_required( VERSION 2.8 )
project( raspberry_pi_camera_streamer )

SET( COMPILE_DEFINITIONS -Werror )

include_directories( /opt/vc/include )
include_directories( /opt/vc/include/interface )
include_directories( /opt/vc/include/interface/vcos )
include_directories( /opt/vc/include/interface/vcos/pthreads )
include_directories( /opt/vc/include/interface/vmcs_host/linux )

link_directories( /opt/vc/lib )
add_executable( raspberry_pi_camera_streamer 
    src/main.cpp
    src/h264_encoder.cpp
    src/image_encoder.cpp
    src/mjpeg_httpd.cpp
    src/resizer.cpp
    src/splitter.cpp
    src/utils.cpp )

target_link_libraries( raspberry_pi_camera_streamer 
    mmal_core
    mmal_util
    mmal_vc_client
    vcos
    bcm_host
    pthread
)

install( TARGETS raspberry_pi_camera_streamer RUNTIME DESTINATION bin )
