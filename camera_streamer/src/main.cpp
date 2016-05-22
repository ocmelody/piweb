
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <bcm_host.h>
#include <mmal/mmal.h>
#include <mmal/util/mmal_connection.h>
#include <mmal/util/mmal_default_components.h>
#include <mmal/util/mmal_util.h>
#include <mmal/util/mmal_util_params.h>
#include <vcos/vcos.h>

#include <arpa/inet.h>
#include <pthread.h>
#include "mjpeg_httpd.h"

#include "h264_encoder.h"
#include "image_encoder.h"
#include "splitter.h"
#include "resizer.h"

#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

const int MAX_JPEG_SIZE = 128*1024;
const int MAX_MOTION_VECTORS_SIZE = 16*1024;
const int NUM_BUFFER_SLOTS = 32;

uint8_t IMAGE_SLOTS[ NUM_BUFFER_SLOTS*MAX_JPEG_SIZE ];
uint32_t IMAGE_SIZES[ NUM_BUFFER_SLOTS ];
uint8_t SMALL_IMAGE_SLOTS[ NUM_BUFFER_SLOTS*MAX_JPEG_SIZE ];
uint32_t SMALL_IMAGE_SIZES[ NUM_BUFFER_SLOTS ];
uint8_t MOTION_VECTORS_SLOTS[ NUM_BUFFER_SLOTS*MAX_MOTION_VECTORS_SIZE ];
uint32_t MOTION_VECTORS_SIZES[ NUM_BUFFER_SLOTS ];
int8_t gCurImageSlotIdx = 0;
int8_t gCurSmallImageSlotIdx = 0;
int8_t gCurMotionVectorsSlotIdx = 0;

struct Options
{
    int mVideoWidth;
    int mVideoHeight;
    int mVideoFPS;
    int mWebServerFPS;
    int mBitrate;
    int mJpegQuality;
    int mSmallImageWidth;
    int mSmallImageHeight;
};

const bool GENERATE_MOTION_VECTORS = true;

const int MACRO_BLOCK_SIDE_LENGTH = 16;
const int VIDEO_OUTPUT_BUFFERS_NUM = 3;
const int DEFAULT_VIDEO_WIDTH = 320; //640;
const int DEFAULT_VIDEO_HEIGHT = 240; //480;
const int DEFAULT_VIDEO_FPS = 25;       // 25 for 50Hz lights, 30 for 60Hz
const int DEFAULT_WEB_SERVER_FPS = 15;
const int DEFAULT_BITRATE = 0;  // Just let the encoder work it out for itself
const int DEFAULT_JPEG_QUALITY = 8;
const int DEFAULT_SMALL_IMAGE_WIDTH = 160;
const int DEFAULT_SMALL_IMAGE_HEIGHT = 120;

MMAL_COMPONENT_T* gpCamera = NULL;

H264Encoder* gpH264Encoder = NULL;
ImageEncoder* gpMainImageEncoder = NULL;
ImageEncoder* gpSmallImageEncoder = NULL;
Splitter* gpSplitter = NULL;
Resizer* gpResizer = NULL;

HttpServerContext gHttpServerContext;

//--------------------------------------------------------------------------------------------------
static void cameraControlCallback( MMAL_PORT_T* pPort, MMAL_BUFFER_HEADER_T* pBuffer )
{
   if ( pBuffer->cmd == MMAL_EVENT_PARAMETER_CHANGED )
   {
   }
   else
   {
      fprintf( stderr, "Error: Received unexpected camera control callback event, 0x%08x\n", pBuffer->cmd );
   }

   mmal_buffer_header_release( pBuffer );
}

//--------------------------------------------------------------------------------------------------
static void h264EncoderBufferCallback( MMAL_BUFFER_HEADER_T* pBuffer, void* pUserData )
{
    const Options* pOptions = (Options*)pUserData;
    
    // Decode flags
#if 0
    printf( "H264: Got buffer with flags %i\n", pBuffer->flags );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_EOS ) printf( "\tMMAL_BUFFER_HEADER_FLAG_EOS\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_START ) printf( "\tMMAL_BUFFER_HEADER_FLAG_FRAME_START\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END ) printf( "\tMMAL_BUFFER_HEADER_FLAG_FRAME_END\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME == MMAL_BUFFER_HEADER_FLAG_FRAME ) printf( "\tMMAL_BUFFER_HEADER_FLAG_FRAME\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME ) printf( "\tMMAL_BUFFER_HEADER_FLAG_KEYFRAME\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_DISCONTINUITY ) printf( "\tMMAL_BUFFER_HEADER_FLAG_DISCONTINUITY\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG ) printf( "\tMMAL_BUFFER_HEADER_FLAG_CONFIG\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_ENCRYPTED ) printf( "\tMMAL_BUFFER_HEADER_FLAG_ENCRYPTED\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO ) printf( "\tMMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAGS_SNAPSHOT ) printf( "\tMMAL_BUFFER_HEADER_FLAGS_SNAPSHOT\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_CORRUPTED ) printf( "\tMMAL_BUFFER_HEADER_FLAG_CORRUPTED\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED ) printf( "\tMMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED\n" );
#endif

    // Check to see if this is a buffer of motion vectors
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO )
    {
        uint16_t totalBufferLength = pBuffer->length + 2*sizeof( uint16_t );
        
        if ( totalBufferLength > MAX_MOTION_VECTORS_SIZE )
        {
            fprintf( stderr, "Error: Buffer for motion vectors is too small\n" );
        }
        else
        {
            // Copy data out
            mmal_buffer_header_mem_lock( pBuffer );
        
            // We store the motion vectors as
            // uint16_t width in macro blocks
            // uint16_t height in macro blocks
            // An array of motion vectors, one for each macro block
            uint8_t* pDstBuffer = &MOTION_VECTORS_SLOTS[ gCurMotionVectorsSlotIdx*MAX_MOTION_VECTORS_SIZE ];
            
            ((uint16_t*)pDstBuffer)[ 0 ] = (pOptions->mVideoWidth / MACRO_BLOCK_SIDE_LENGTH) + 1;
            ((uint16_t*)pDstBuffer)[ 1 ] = pOptions->mVideoHeight / MACRO_BLOCK_SIDE_LENGTH;
            
            memcpy( pDstBuffer + 2*sizeof( uint16_t ), pBuffer->data, pBuffer->length );
            MOTION_VECTORS_SIZES[ gCurMotionVectorsSlotIdx ] = totalBufferLength;
            gCurMotionVectorsSlotIdx = (gCurMotionVectorsSlotIdx + 1)%NUM_BUFFER_SLOTS;

            mmal_buffer_header_mem_unlock( pBuffer );
        }
    }
}

//--------------------------------------------------------------------------------------------------
static void mainImageEncoderBufferCallback( MMAL_BUFFER_HEADER_T* pBuffer, void* pUserData )
{
    // Decode flags
#if 0
    printf( "JPEG: Got buffer with flags %i\n", pBuffer->flags );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_EOS ) printf( "\tMMAL_BUFFER_HEADER_FLAG_EOS\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_START ) printf( "\tMMAL_BUFFER_HEADER_FLAG_FRAME_START\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END ) printf( "\tMMAL_BUFFER_HEADER_FLAG_FRAME_END\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME == MMAL_BUFFER_HEADER_FLAG_FRAME ) printf( "\tMMAL_BUFFER_HEADER_FLAG_FRAME\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME ) printf( "\tMMAL_BUFFER_HEADER_FLAG_KEYFRAME\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_DISCONTINUITY ) printf( "\tMMAL_BUFFER_HEADER_FLAG_DISCONTINUITY\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG ) printf( "\tMMAL_BUFFER_HEADER_FLAG_CONFIG\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_ENCRYPTED ) printf( "\tMMAL_BUFFER_HEADER_FLAG_ENCRYPTED\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO ) printf( "\tMMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAGS_SNAPSHOT ) printf( "\tMMAL_BUFFER_HEADER_FLAGS_SNAPSHOT\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_CORRUPTED ) printf( "\tMMAL_BUFFER_HEADER_FLAG_CORRUPTED\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED ) printf( "\tMMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED\n" );
#endif

    //printf( "JPEG: Buffer size is %i\n", pBuffer->length );
    //printf( "Buffer header is %X %X\n", pBuffer->data[ 0 ], pBuffer->data[ 1 ] );

    // Check to see if we can handle the image
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END
        && 0xFF == pBuffer->data[ 0 ] && 0xD8 == pBuffer->data[ 1 ] )
    {
        if ( pBuffer->length > MAX_JPEG_SIZE )
        {
            fprintf( stderr, "Error: Buffer for JPEG is too small\n" );
        }
        else
        {
            // Copy data out
            mmal_buffer_header_mem_lock( pBuffer );
            
            //printf( "Writing to slot %i\n", gCurImageSlotIdx );
            
            memcpy( &IMAGE_SLOTS[ gCurImageSlotIdx*MAX_JPEG_SIZE ], pBuffer->data, pBuffer->length );
            IMAGE_SIZES[ gCurImageSlotIdx ] = pBuffer->length;
            gCurImageSlotIdx = (gCurImageSlotIdx + 1)%NUM_BUFFER_SLOTS;

            mmal_buffer_header_mem_unlock( pBuffer );
        }
    }
    else
    {
        if ( pBuffer->length > 0 )
        {
            fprintf( stderr, "Error: Got just part of a frame. Unable to handle this yet. Please reduce resolution and/or jpeg quality\n" );
        }
    }
}

//--------------------------------------------------------------------------------------------------
static void smallImageEncoderBufferCallback( MMAL_BUFFER_HEADER_T* pBuffer, void* pUserData )
{
    // Decode flags
#if 0
    printf( "JPEG: Got buffer with flags %i\n", pBuffer->flags );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_EOS ) printf( "\tMMAL_BUFFER_HEADER_FLAG_EOS\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_START ) printf( "\tMMAL_BUFFER_HEADER_FLAG_FRAME_START\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END ) printf( "\tMMAL_BUFFER_HEADER_FLAG_FRAME_END\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME == MMAL_BUFFER_HEADER_FLAG_FRAME ) printf( "\tMMAL_BUFFER_HEADER_FLAG_FRAME\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME ) printf( "\tMMAL_BUFFER_HEADER_FLAG_KEYFRAME\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_DISCONTINUITY ) printf( "\tMMAL_BUFFER_HEADER_FLAG_DISCONTINUITY\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG ) printf( "\tMMAL_BUFFER_HEADER_FLAG_CONFIG\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_ENCRYPTED ) printf( "\tMMAL_BUFFER_HEADER_FLAG_ENCRYPTED\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO ) printf( "\tMMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAGS_SNAPSHOT ) printf( "\tMMAL_BUFFER_HEADER_FLAGS_SNAPSHOT\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_CORRUPTED ) printf( "\tMMAL_BUFFER_HEADER_FLAG_CORRUPTED\n" );
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED ) printf( "\tMMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED\n" );
#endif

    //printf( "Small JPEG: Buffer size is %i\n", pBuffer->length );
    //printf( "Buffer header is %X %X\n", pBuffer->data[ 0 ], pBuffer->data[ 1 ] );

    // Check to see if we can handle the image
    if ( pBuffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END
        && 0xFF == pBuffer->data[ 0 ] && 0xD8 == pBuffer->data[ 1 ] )
    {
        if ( pBuffer->length > MAX_JPEG_SIZE )
        {
            fprintf( stderr, "Error: Buffer for JPEG is too small\n" );
        }
        else
        {
            // Copy data out
            mmal_buffer_header_mem_lock( pBuffer );
            
            memcpy( &SMALL_IMAGE_SLOTS[ gCurSmallImageSlotIdx*MAX_JPEG_SIZE ], pBuffer->data, pBuffer->length );
            SMALL_IMAGE_SIZES[ gCurSmallImageSlotIdx ] = pBuffer->length;
            gCurSmallImageSlotIdx = (gCurSmallImageSlotIdx + 1)%NUM_BUFFER_SLOTS;

            mmal_buffer_header_mem_unlock( pBuffer );
        }
    }
    else
    {
        if ( pBuffer->length > 0 )
        {
            fprintf( stderr, "Error: Got just part of a frame. Unable to handle this yet. Please reduce resolution and/or jpeg quality\n" );
        }
    }
}

//--------------------------------------------------------------------------------------------------
static MMAL_STATUS_T connectPorts( MMAL_PORT_T* pOutputPort, MMAL_PORT_T* pInputPort, 
                                   MMAL_CONNECTION_T** ppConnectionOut)
{
    MMAL_STATUS_T status;

    status = mmal_connection_create( ppConnectionOut, pOutputPort, pInputPort, 
        MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);

    if ( MMAL_SUCCESS == status )
    {
        status = mmal_connection_enable( *ppConnectionOut );
        if ( MMAL_SUCCESS != status )
        {
            mmal_connection_destroy( *ppConnectionOut );
            *ppConnectionOut = NULL;
        }
    }

    return status;
}

//--------------------------------------------------------------------------------------------------
static MMAL_STATUS_T createCameraComponent( 
    int imageWidth, int imageHeight, int videoFPS, 
    MMAL_PORT_BH_CB_T cameraControlCallbackFn, MMAL_COMPONENT_T** ppCameraOut )
{
    MMAL_COMPONENT_T* pCamera = NULL;
    MMAL_ES_FORMAT_T* pFormat;
    MMAL_PORT_T* pPreviewPort = NULL;
    MMAL_PORT_T* pVideoPort = NULL;
    MMAL_PORT_T* pStillPort = NULL;
    MMAL_STATUS_T status;
    MMAL_RATIONAL_T videoFrameRate;
    videoFrameRate.num = videoFPS;
    videoFrameRate.den = 1;

    // Create the component
    status = mmal_component_create( MMAL_COMPONENT_DEFAULT_CAMERA, &pCamera );

    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Failed to create camera component\n" );
        goto error;
    }

    if ( 0 == pCamera->output_num )
    {
        status = MMAL_ENOSYS;
        fprintf( stderr, "Error: Camera doesn't have output ports\n" );
        goto error;
    }

    pPreviewPort = pCamera->output[ MMAL_CAMERA_PREVIEW_PORT ];
    pVideoPort = pCamera->output[ MMAL_CAMERA_VIDEO_PORT ];
    pStillPort = pCamera->output[ MMAL_CAMERA_CAPTURE_PORT ];

    // Enable the camera, and tell it its control callback function
    status = mmal_port_enable( pCamera->control, cameraControlCallbackFn );

    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Unable to enable control port : error %d\n", status );
        goto error;
    }

    // Set up the camera configuration
    MMAL_PARAMETER_CAMERA_CONFIG_T camConfig;
    camConfig.hdr.id = MMAL_PARAMETER_CAMERA_CONFIG;
    camConfig.hdr.size = sizeof( camConfig );
    camConfig.max_stills_w = imageWidth;
    camConfig.max_stills_h = imageHeight;
    camConfig.stills_yuv422 = 0;
    camConfig.one_shot_stills = 1;
    camConfig.max_preview_video_w = imageWidth;
    camConfig.max_preview_video_h = imageHeight;
    camConfig.num_preview_video_frames = 3;
    camConfig.stills_capture_circular_buffer_height = 0;
    camConfig.fast_preview_resume = 0;
    camConfig.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC;

    mmal_port_parameter_set( pCamera->control, &camConfig.hdr );

    // Now set up the port formats

    // Set the encode format on the Preview port
    // HW limitations mean we need the preview to be the same size as the required recorded output

    pFormat = pPreviewPort->format;

    pFormat->encoding = MMAL_ENCODING_I420;
    pFormat->encoding_variant = MMAL_ENCODING_I420;
    
    pFormat->es->video.width = VCOS_ALIGN_UP( imageWidth, 32 );
    pFormat->es->video.height = VCOS_ALIGN_UP( imageHeight, 16 );
    pFormat->es->video.crop.x = 0;
    pFormat->es->video.crop.y = 0;
    pFormat->es->video.crop.width = imageWidth;
    pFormat->es->video.crop.height = imageHeight;
    pFormat->es->video.frame_rate = videoFrameRate;

    status = mmal_port_format_commit( pPreviewPort );

    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Preview port format couldn't be set\n" );
        goto error;
    }

    // Set the encode format on the video  port

    pFormat = pVideoPort->format;
    
    pFormat->encoding = MMAL_ENCODING_I420;
    pFormat->encoding_variant = MMAL_ENCODING_I420;
    
    pFormat->es->video.width = VCOS_ALIGN_UP( imageWidth, 32 );
    pFormat->es->video.height = VCOS_ALIGN_UP( imageHeight, 16 );
    pFormat->es->video.crop.x = 0;
    pFormat->es->video.crop.y = 0;
    pFormat->es->video.crop.width = imageWidth;
    pFormat->es->video.crop.height = imageHeight;
    pFormat->es->video.frame_rate = videoFrameRate;

    status = mmal_port_format_commit( pVideoPort );

    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Camera video format couldn't be set\n" );
        goto error;
    }

    // Ensure there are enough buffers to avoid dropping frames
    if ( pVideoPort->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM )
    {
        pVideoPort->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
    }

    // Set the encode format on the still  port

    pFormat = pStillPort->format;

    pFormat->encoding = MMAL_ENCODING_OPAQUE;
    pFormat->encoding_variant = MMAL_ENCODING_I420;

    pFormat->es->video.width = VCOS_ALIGN_UP( imageWidth, 32 );
    pFormat->es->video.height = VCOS_ALIGN_UP( imageHeight, 16 );
    pFormat->es->video.crop.x = 0;
    pFormat->es->video.crop.y = 0;
    pFormat->es->video.crop.width = imageWidth;
    pFormat->es->video.crop.height = imageHeight;
    pFormat->es->video.frame_rate.num = 0;
    pFormat->es->video.frame_rate.den = 1;

    status = mmal_port_format_commit( pStillPort );

    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Camera still format couldn't be set\n" );
        goto error;
    }

    // Ensure there are enough buffers to avoid dropping frames
    if ( pStillPort->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM )
    {
        pStillPort->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
    }

    // Enable component
    status = mmal_component_enable( pCamera );

    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Camera component couldn't be enabled\n" );
        goto error;
    }

    //raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);

    *ppCameraOut = pCamera;

    return status;

error:

    if ( NULL != pCamera )
    {
        mmal_component_destroy( pCamera );
    }

    return status;
}

//--------------------------------------------------------------------------------------------------
static MMAL_STATUS_T createSplitterComponent( MMAL_PORT_T* pSourcePort, 
                                              MMAL_COMPONENT_T** ppSplitterComponentOut )
{
    MMAL_STATUS_T status;
    MMAL_COMPONENT_T* pSplitterComponent = NULL;
    MMAL_PORT_T* pInputPort = NULL;

    status = mmal_component_create( MMAL_COMPONENT_DEFAULT_VIDEO_SPLITTER, &pSplitterComponent );

    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Failed to create splitter component\n" );
        goto error;
    }

    pInputPort = pSplitterComponent->input[ 0 ];
    mmal_format_copy( pInputPort->format, pSourcePort->format );
    pInputPort->buffer_num = 3;
    status = mmal_port_format_commit( pInputPort );
    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Couldn't set splitter input port format : error %d", status );
        goto error;
    }

    for( int i = 0; i < pSplitterComponent->output_num; i++ )
    {
        MMAL_PORT_T* pOutputPort = pSplitterComponent->output[ i ];
        pOutputPort->buffer_num = 3;
        mmal_format_copy( pOutputPort->format, pInputPort->format );
        status = mmal_port_format_commit( pOutputPort );
        if ( MMAL_SUCCESS != status )
        {
            fprintf( stderr, "Error: Couldn't set splitter output port format : error %d", status );
            goto error;
        }
    }

    *ppSplitterComponentOut = pSplitterComponent;
    return status;

error:
    if ( NULL != pSplitterComponent ) 
    {
        mmal_component_destroy( pSplitterComponent );
        pSplitterComponent = NULL;
    }
    
    return status;
}

//--------------------------------------------------------------------------------------------------
void shutdown()
{
    // Stop HTTP server thread
    pthread_cancel( gHttpServerContext.threadID );
    
    // Disable all our ports that are not handled by connections
    if ( NULL != gpCamera )
    {
        MMAL_PORT_T* pStillPort = gpCamera->output[ MMAL_CAMERA_CAPTURE_PORT ];

        if ( NULL != pStillPort
            && pStillPort->is_enabled )
        {
            mmal_port_disable( pStillPort );
        }
    }
    
//     if ( NULL != gpEncoder )
//     {
//         MMAL_PORT_T* pEncoderOutputPort = gpEncoder->output[ 0 ];
// 
//         if ( NULL != pEncoderOutputPort
//             && pEncoderOutputPort->is_enabled )
//         {
//             mmal_port_disable( pEncoderOutputPort );
//         }
//     }
    
    // Shut down connections
//     if ( NULL != gpCameraToEncoderConnection )
//     {
//         mmal_connection_destroy( gpCameraToEncoderConnection );
//         gpCameraToEncoderConnection = NULL;
//     }

    // Disable components
//     if ( NULL != gpEncoder )
//     {
//         mmal_component_disable( gpEncoder );
//     }

    if ( NULL != gpSmallImageEncoder )
    {
        delete gpSmallImageEncoder;
        gpSmallImageEncoder = NULL;
    }

    if ( NULL != gpMainImageEncoder )
    {
        delete gpMainImageEncoder;
        gpMainImageEncoder = NULL;
    }

    if ( NULL != gpH264Encoder )
    {
        delete gpH264Encoder;
        gpH264Encoder = NULL;
    }

    if ( NULL != gpCamera )
    {
        mmal_component_disable( gpCamera );
    }

    // Destroy components
//     if ( NULL != gpEncoder )
//     {
//         if ( NULL != gpEncoderPool )
//         {
//             mmal_port_pool_destroy( gpEncoder->output[ 0 ], gpEncoderPool );
//         }
//         
//         mmal_component_destroy( gpEncoder );
//         gpEncoder = NULL;
//     }
    
    if ( NULL != gpCamera )
    {
        mmal_component_destroy( gpCamera );
        gpCamera = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
static void sigintHandler( int signalNumber )
{
    // Going to abort on all other signals
    printf( "Aborting program...\n" );
    exit( 0 );
}

//--------------------------------------------------------------------------------------------------
void printUsage( const char* programName )
{
    printf( "Usage %s [OPTIONS]\n", programName );
    printf( "Streams JPEG images and motion vectors from the Pi camera over HTTP\n" );
    printf( "\n" );
    printf( "    -w video_width        Sets the width of the camera image (%i)\n", DEFAULT_VIDEO_WIDTH );
    printf( "    -h video_height       Sets the height of the camera image (%i)\n", DEFAULT_VIDEO_HEIGHT );
    printf( "    -f video_fps          Sets video capture frame rate (%i)\n", DEFAULT_VIDEO_FPS );
    printf( "    -s web_server_fps     Sets the frame rate of images sent over http (%i)\n", DEFAULT_WEB_SERVER_FPS );
    printf( "    -b bitrate            Sets bitrate for encoding video (%i)\n", DEFAULT_BITRATE );
    printf( "    -q jpeg_quality       Sets the quality for JPEG compression (%i)\n", DEFAULT_JPEG_QUALITY );
    printf( "    -u small_image_width  Sets the width of the small image (%i)\n", DEFAULT_SMALL_IMAGE_WIDTH );
    printf( "    -v small_image_height Sets the height of the small image (%i)\n", DEFAULT_SMALL_IMAGE_HEIGHT );
    printf( "\n" );
}

//--------------------------------------------------------------------------------------------------
bool parseCommandLineOptions( int argc, char** argv, Options* pOptionsInOut )
{    
    // Set the options to default values
    pOptionsInOut->mVideoWidth = DEFAULT_VIDEO_WIDTH;
    pOptionsInOut->mVideoHeight = DEFAULT_VIDEO_HEIGHT;
    pOptionsInOut->mVideoFPS = DEFAULT_VIDEO_FPS;
    pOptionsInOut->mWebServerFPS = DEFAULT_WEB_SERVER_FPS;
    pOptionsInOut->mBitrate = DEFAULT_BITRATE;
    pOptionsInOut->mJpegQuality = DEFAULT_JPEG_QUALITY;
    pOptionsInOut->mSmallImageWidth = DEFAULT_SMALL_IMAGE_WIDTH;
    pOptionsInOut->mSmallImageHeight = DEFAULT_SMALL_IMAGE_HEIGHT;
    
    int opt;
    bool bInvalidArgumentFound = false;
    while ( (opt = getopt( argc, argv, "w:h:f:s:b:q:u:v:") ) != -1 
        && !bInvalidArgumentFound ) 
    {
        switch ( opt ) 
        {
        case 'w':
            {
                pOptionsInOut->mVideoWidth = atoi( optarg );
                if ( pOptionsInOut->mVideoWidth <= 0 )
                {
                    fprintf( stderr, "Error: Invalid video width specified\n" );
                    bInvalidArgumentFound = true;
                }
                break;
            }
        case 'h':
            {
                pOptionsInOut->mVideoHeight = atoi( optarg );
                if ( pOptionsInOut->mVideoHeight <= 0 )
                {
                    fprintf( stderr, "Error: Invalid video height specified\n" );
                    bInvalidArgumentFound = true;
                }
                break;
            }
        case 'f':
            {
                pOptionsInOut->mVideoFPS = atoi( optarg );
                if ( pOptionsInOut->mVideoFPS <= 0 )
                {
                    fprintf( stderr, "Error: Invalid frame rate specified\n" );
                    bInvalidArgumentFound = true;
                }
                break;
            }
        case 's':
            {
                pOptionsInOut->mWebServerFPS = atoi( optarg );
                if ( pOptionsInOut->mWebServerFPS <= 0 )
                {
                    fprintf( stderr, "Error: Invalid web server frame rate specified\n" );
                    bInvalidArgumentFound = true;
                }
                break;
            }
        case 'b':
            {
                pOptionsInOut->mBitrate = atoi( optarg );
                if ( pOptionsInOut->mBitrate < 0 )
                {
                    fprintf( stderr, "Error: Invalid bitrate specified\n" );
                    bInvalidArgumentFound = true;
                }
                break;
            }
        case 'q':
            {
                pOptionsInOut->mJpegQuality = atoi( optarg );
                if ( pOptionsInOut->mJpegQuality < 0 || pOptionsInOut->mJpegQuality > 100 )
                {
                    fprintf( stderr, "Error: Invalid jpeg quality specified\n" );
                    bInvalidArgumentFound = true;
                }
                break;
            }
        case 'u':
            {
                pOptionsInOut->mSmallImageWidth = atoi( optarg );
                if ( pOptionsInOut->mSmallImageWidth <= 0 )
                {
                    fprintf( stderr, "Error: Invalid small image width specified\n" );
                    bInvalidArgumentFound = true;
                }
                break;
            }
        case 'v':
            {
                pOptionsInOut->mSmallImageHeight = atoi( optarg );
                if ( pOptionsInOut->mSmallImageHeight <= 0 )
                {
                    fprintf( stderr, "Error: Invalid small image height specified\n" );
                    bInvalidArgumentFound = true;
                }
                break;
            }
        default: // '?'
            {
                bInvalidArgumentFound = true;
            }
        }
    }
    
    return !bInvalidArgumentFound;
}

//--------------------------------------------------------------------------------------------------
int main( int argc, char** argv )
{
    atexit( shutdown );
    signal( SIGINT, sigintHandler );
    
    bcm_host_init();
    
    // Parse the command line
    Options options;
    if ( !parseCommandLineOptions( argc, argv, &options ) )
    {
        fprintf( stderr, "Error: Unable to parse command line\n" );
        printUsage( argv[ 0 ] );
        exit( -1 );
    }
    
    // Create a camera component
    if ( createCameraComponent( options.mVideoWidth, options.mVideoHeight, options.mVideoFPS, 
        cameraControlCallback, &gpCamera ) != MMAL_SUCCESS )
    {
        fprintf( stderr, "Error: Unable to create camera component\n" );
        exit( -1 );
    }
    
    // Create a splitter components
    gpSplitter = new Splitter();
    
    // Create a resizer component
    gpResizer = new Resizer( options.mSmallImageWidth, options.mSmallImageHeight );

    // Create encoder components
    gpH264Encoder = new H264Encoder( options.mBitrate, GENERATE_MOTION_VECTORS );
    gpMainImageEncoder = new ImageEncoder( MMAL_ENCODING_JPEG, options.mJpegQuality );
    gpSmallImageEncoder = new ImageEncoder( MMAL_ENCODING_JPEG, options.mJpegQuality );
    
    // Connect the camera to the splitter
    if ( gpSplitter->connectPortToSplitter( 
        gpCamera->output[ MMAL_CAMERA_VIDEO_PORT ] ) != MMAL_SUCCESS )
    {
        fprintf( stderr, "Error: Failed to connect camera port to splitter input\n" );
        exit( -1 );
    }
    
    // Connect the splitter to the h264 encoder
    if ( gpH264Encoder->connectPortToEncoder( 
        gpSplitter->getOutputPort( 0 ) ) != MMAL_SUCCESS )
    {
        fprintf( stderr, "Error: Failed to connect splitter port to h264 encoder input\n" );
        exit( -1 );
    }
    
    // Connect the video splitter to the main image encoder
    if ( gpMainImageEncoder->connectPortToEncoder( 
        gpSplitter->getOutputPort( 1 ) ) != MMAL_SUCCESS )
    {
        fprintf( stderr, "Error: Failed to connect splitter port to image encoder input\n" );
        exit( -1 );
    }
    
    // Connect the camera to the resizer
    if ( gpResizer->connectPortToResizer( 
        gpCamera->output[ MMAL_CAMERA_PREVIEW_PORT ] ) != MMAL_SUCCESS )
    {
        fprintf( stderr, "Error: Failed to connect camera preview port to resizer input\n" );
        exit( -1 );
    }
    
    // Connect the resizer to the small image encoder
    if ( gpSmallImageEncoder->connectPortToEncoder( 
        gpResizer->getOutputPort() ) != MMAL_SUCCESS )
    {
        fprintf( stderr, "Error: Failed to connect resizer port to small image encoder input\n" );
        exit( -1 );
    }
    
    // Enable the h264 encoder output port and tell it its callback function
    if ( gpH264Encoder->enableOutputPort( h264EncoderBufferCallback, &options ) != MMAL_SUCCESS )
    {
        fprintf( stderr, "Error: Failed to setup h264 encoder output\n" );
        exit( -1 );
    }
    
    // Enable the main image encoder output port and tell it its callback function
    if ( gpMainImageEncoder->enableOutputPort( 
        mainImageEncoderBufferCallback, NULL ) != MMAL_SUCCESS )
    {
        fprintf( stderr, "Error: Failed to setup image encoder output\n" );
        exit( -1 );
    }
    
    // Enable the small image encoder output port and tell it its callback function
    if ( gpSmallImageEncoder->enableOutputPort( 
        smallImageEncoderBufferCallback, NULL ) != MMAL_SUCCESS )
    {
        fprintf( stderr, "Error: Failed to setup small image encoder output\n" );
        exit( -1 );
    }
    
    //vertical rotate 180
    mmal_port_parameter_set_int32(gpCamera->output[ MMAL_CAMERA_VIDEO_PORT ],MMAL_PARAMETER_ROTATION,180);

    // Start the capture
    if ( mmal_port_parameter_set_boolean( gpCamera->output[ MMAL_CAMERA_VIDEO_PORT ], 
        MMAL_PARAMETER_CAPTURE, 1 ) != MMAL_SUCCESS )
    {
        fprintf( stderr, "Error: Unable to start camera capture\n" );
        exit( -1 );
    }
    
    // Set up a HTTP server to send out the images
    gHttpServerContext.id = 0;
    gHttpServerContext.conf.port = htons( 8080 );
    gHttpServerContext.conf.credentials = NULL;
    gHttpServerContext.conf.www_folder = NULL;
    gHttpServerContext.conf.nocommands = 0;
    
    gHttpServerContext.mImageSlots = (uint8_t*)IMAGE_SLOTS;
    gHttpServerContext.mImageSizes = (uint32_t*)IMAGE_SIZES;
    gHttpServerContext.mMaxImageSize = MAX_JPEG_SIZE;
    gHttpServerContext.mNumImageSlots = NUM_BUFFER_SLOTS;
    gHttpServerContext.mpCurWriteImageSlotIdx = &gCurImageSlotIdx;
    
    gHttpServerContext.mSmallImageSlots = (uint8_t*)SMALL_IMAGE_SLOTS;
    gHttpServerContext.mSmallImageSizes = (uint32_t*)SMALL_IMAGE_SIZES;
    gHttpServerContext.mMaxSmallImageSize = MAX_JPEG_SIZE;
    gHttpServerContext.mNumSmallImageSlots = NUM_BUFFER_SLOTS;
    gHttpServerContext.mpCurWriteSmallImageSlotIdx = &gCurSmallImageSlotIdx;
    
    gHttpServerContext.mMotionVectorsSlots = (uint8_t*)MOTION_VECTORS_SLOTS;
    gHttpServerContext.mMotionVectorsSizes = (uint32_t*)MOTION_VECTORS_SIZES;
    gHttpServerContext.mMaxMotionVectorsSize = MAX_MOTION_VECTORS_SIZE;
    gHttpServerContext.mNumMotionVectorsSlots = NUM_BUFFER_SLOTS;
    gHttpServerContext.mpCurWriteMotionVectorsSlotIdx = &gCurMotionVectorsSlotIdx;
    
    gHttpServerContext.mWebServerFPS = options.mWebServerFPS;
    
    pthread_create( &gHttpServerContext.threadID, NULL, server_thread, &gHttpServerContext );
    pthread_detach( gHttpServerContext.threadID );
    
    while ( true )
    {
        vcos_sleep( 1000 ); // Sleep ms
    }
    
    // Capture video to a file
    
    return 0;
}
