
#include <stdexcept>
#include <stdio.h>
#include <mmal/util/mmal_util.h>
#include <mmal/util/mmal_util_params.h>

#include "resizer.h"
#include "utils.h"

//--------------------------------------------------------------------------------------------------
// Resizer
//--------------------------------------------------------------------------------------------------
Resizer::Resizer( int width, int height )
    : mpResizer( NULL ),
    mpInputConnection( NULL ),
    mWidth( width ),
    mHeight( height )
{
    MMAL_STATUS_T status;
    
    status = mmal_component_create( "vc.ril.resize", &mpResizer );

    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Unable to create resizer component\n" );
        goto error;
    }

    return;

error:
    if ( NULL != mpResizer )
    {
        mmal_component_destroy( mpResizer );
    }

    throw std::runtime_error( "Unable to create resizer" );
}

//--------------------------------------------------------------------------------------------------
Resizer::~Resizer()
{
    // Shut down output port
    if ( NULL != mpResizer )
    {
        if ( NULL != mpResizer->output[ 0 ]
            && mpResizer->output[ 0 ]->is_enabled )
        {
            mmal_port_disable( mpResizer->output[ 0 ] );
        }
    }
    
    // Shut down input connection
    if ( NULL != mpInputConnection )
    {
        mmal_connection_destroy( mpInputConnection );
        mpInputConnection = NULL;
    }

    // Disable and destroy component
    if ( NULL != mpResizer )
    {
        mmal_component_disable( mpResizer );        
        mmal_component_destroy( mpResizer );
        mpResizer = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
MMAL_STATUS_T Resizer::connectPortToResizer( MMAL_PORT_T* pPort )
{
    MMAL_STATUS_T status;
    
    MMAL_PORT_T* pInputPort = mpResizer->input[ 0 ];
    mmal_format_copy( pInputPort->format, pPort->format );
    pInputPort->buffer_num = 3;
    status = mmal_port_format_commit( pInputPort );
    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Couldn't set resizer input port format : error %d", status );
        return status;
    }


    MMAL_PORT_T* pOutputPort = mpResizer->output[ 0 ];
    pOutputPort->buffer_num = 3;
    mmal_format_copy( pOutputPort->format, pInputPort->format );
    
    pOutputPort->format->es->video.width = mWidth;
    pOutputPort->format->es->video.height = mHeight;
    pOutputPort->format->es->video.crop.x = 0;
    pOutputPort->format->es->video.crop.y = 0;
    pOutputPort->format->es->video.crop.width = mWidth;
    pOutputPort->format->es->video.crop.height = mHeight;
    
    status = mmal_port_format_commit( pOutputPort );
    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Couldn't set resizer output port format : error %d", status );
        return status;
    }
    
    return Utils::connectPorts( pPort, mpResizer->input[ 0 ], &mpInputConnection );
}
