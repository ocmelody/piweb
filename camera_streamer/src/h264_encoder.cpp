
#include <stdexcept>
#include <stdio.h>
#include <mmal/util/mmal_util.h>
#include <mmal/util/mmal_util_params.h>

#include "h264_encoder.h"
#include "utils.h"

//--------------------------------------------------------------------------------------------------
// H264Encoder
//--------------------------------------------------------------------------------------------------
H264Encoder::H264Encoder( int bitrate, bool bGenerateMotionVectors )
    : mpEncoder( NULL ),
    mpInputConnection( NULL ),
    mpEncoderPool( NULL ),
    mUserEncoderBufferCB( NULL ),
    mpUserData( NULL )
{
    MMAL_PORT_T* pEncoderInputPort = NULL;
    MMAL_PORT_T* pEncoderOutputPort = NULL;
    MMAL_STATUS_T status;
    
    status = mmal_component_create( MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &mpEncoder );

    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Unable to create video encoder component\n" );
        goto error;
    }

    if ( 0 == mpEncoder->input_num || 0 == mpEncoder->output_num )
    {
        status = MMAL_ENOSYS;
        fprintf( stderr, "Error: Video encoder doesn't have input/output ports\n" );
        goto error;
    }

    pEncoderInputPort = mpEncoder->input[ 0 ];
    pEncoderOutputPort = mpEncoder->output[ 0 ];

    // We want same format on input and output
    mmal_format_copy( pEncoderOutputPort->format, pEncoderInputPort->format );

    // Encode video as H264
    pEncoderOutputPort->format->encoding = MMAL_ENCODING_H264;
    pEncoderOutputPort->format->bitrate = bitrate;

    pEncoderOutputPort->buffer_size = pEncoderOutputPort->buffer_size_recommended;

    if ( pEncoderOutputPort->buffer_size < pEncoderOutputPort->buffer_size_min )
    {
        pEncoderOutputPort->buffer_size = pEncoderOutputPort->buffer_size_min;
    }
 
    pEncoderOutputPort->buffer_num = pEncoderOutputPort->buffer_num_recommended;

    if ( pEncoderOutputPort->buffer_num < pEncoderOutputPort->buffer_num_min )
    {
        pEncoderOutputPort->buffer_num = pEncoderOutputPort->buffer_num_min;
    }

    // We need to set the frame rate on output to 0, to ensure it gets
    // updated correctly from the input framerate when port connected
    pEncoderOutputPort->format->es->video.frame_rate.num = 0;
    pEncoderOutputPort->format->es->video.frame_rate.den = 1;

    // Commit the port changes to the output port
    status = mmal_port_format_commit( pEncoderOutputPort );

    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Unable to set format on video encoder output port\n" );
        goto error;
    }
    
    // Set INLINE VECTORS flag to request motion vector estimates if needed
    if ( mmal_port_parameter_set_boolean( pEncoderOutputPort, 
        MMAL_PARAMETER_VIDEO_ENCODE_INLINE_VECTORS, ( bGenerateMotionVectors ? 1 : 0 ) ) != MMAL_SUCCESS )
    {
        fprintf( stderr, "Error: Failed to set INLINE VECTORS parameters\n" );
        // Continue rather than abort..
    }
    
    //  Enable component
    status = mmal_component_enable( mpEncoder );

    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Unable to enable video encoder component\n" );
        goto error;
    }

    // Create pool of buffer headers for the output port to consume
    mpEncoderPool = mmal_port_pool_create( pEncoderOutputPort, 
        pEncoderOutputPort->buffer_num, pEncoderOutputPort->buffer_size );

    if ( NULL == mpEncoderPool )
    {
        fprintf( stderr, "Error: Failed to create buffer header pool for encoder output port %s\n", pEncoderOutputPort->name );
        goto error;
    }

    return;

error:
    if ( NULL != mpEncoderPool )
    {
        mmal_port_pool_destroy( pEncoderOutputPort, mpEncoderPool );
    }

    if ( NULL != mpEncoder )
    {
        mmal_component_destroy( mpEncoder );
    }

    throw std::runtime_error( "Unable to create H264 encoder" );
}

//--------------------------------------------------------------------------------------------------
H264Encoder::~H264Encoder()
{
    // Shut down output port
    if ( NULL != mpEncoder )
    {
        MMAL_PORT_T* pEncoderOutputPort = mpEncoder->output[ 0 ];

        if ( NULL != pEncoderOutputPort
            && pEncoderOutputPort->is_enabled )
        {
            mmal_port_disable( pEncoderOutputPort );
        }
    }
    
    // Shut down connections
    if ( NULL != mpInputConnection )
    {
        mmal_connection_destroy( mpInputConnection );
        mpInputConnection = NULL;
    }

    // Disable and destroy components
    if ( NULL != mpEncoder )
    {
        mmal_component_disable( mpEncoder );

        if ( NULL != mpEncoderPool )
        {
            mmal_port_pool_destroy( mpEncoder->output[ 0 ], mpEncoderPool );
            mpEncoderPool = NULL;
        }
        
        mmal_component_destroy( mpEncoder );
        mpEncoder = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
MMAL_STATUS_T H264Encoder::connectPortToEncoder( MMAL_PORT_T* pPort )
{
    return Utils::connectPorts( pPort, mpEncoder->input[ 0 ], &mpInputConnection );
}

//--------------------------------------------------------------------------------------------------
MMAL_STATUS_T H264Encoder::enableOutputPort( EncoderBufferCB callback, void* pUserData )
{    
    // Setup data for the encoder buffer callback
    mpEncoder->output[ 0 ]->userdata = (struct MMAL_PORT_USERDATA_T *)this;
    
    mUserEncoderBufferCB = callback;
    mpUserData = pUserData;
    
    // Enable the encoder output port and tell it its callback function
    MMAL_STATUS_T result = mmal_port_enable( 
        mpEncoder->output[ 0 ], H264Encoder::mainEncoderBufferCB );
    if ( MMAL_SUCCESS != result )
    {
        fprintf( stderr, "Error: Failed to setup encoder output\n" );
        return result;
    }
     
    // Send all the buffers to the encoder output port
    int queueLength = mmal_queue_length( mpEncoderPool->queue );
    for ( int bufferIdx = 0; bufferIdx < queueLength; bufferIdx++ )
    {
        MMAL_BUFFER_HEADER_T* pBuffer = mmal_queue_get( mpEncoderPool->queue );
        if ( NULL == pBuffer )
        {
            fprintf( stderr, "Error: Unable to get a required buffer %d from pool queue\n", bufferIdx );
            result = MMAL_ENOSPC;
            return result;
        }
        
        result = mmal_port_send_buffer( mpEncoder->output[ 0 ], pBuffer );
        if ( MMAL_SUCCESS != result )
        {
            fprintf( stderr, "Error: Unable to send a buffer to encoder output port (%d)\n", bufferIdx );
            return result;
        }
    }
    
    return result;
}

//--------------------------------------------------------------------------------------------------
void H264Encoder::mainEncoderBufferCB( MMAL_PORT_T* pPort, MMAL_BUFFER_HEADER_T* pBuffer )
{
    // Get pointer to H264Encoder instance
    H264Encoder* pH264Encoder = (H264Encoder*)pPort->userdata;

    // Call user callback if needed
    if ( NULL != pH264Encoder->mUserEncoderBufferCB )
    {
        pH264Encoder->mUserEncoderBufferCB( pBuffer, pH264Encoder->mpUserData );
    }
    
    // Release buffer back to the pool
    mmal_buffer_header_release( pBuffer );
    
    // and send one back to the port (if still open)
    if ( pPort->is_enabled )
    {
        MMAL_STATUS_T status;

        MMAL_BUFFER_HEADER_T* pNewBuffer = mmal_queue_get( pH264Encoder->mpEncoderPool->queue );

        if ( NULL != pNewBuffer )
        {
            status = mmal_port_send_buffer( pPort, pNewBuffer );

            if ( NULL == pNewBuffer || MMAL_SUCCESS != status )
            {
                fprintf( stderr, "Error: Unable to return a buffer to the encoder port\n" );
            }
        }
    }
}
