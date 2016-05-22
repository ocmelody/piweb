
#include <stdexcept>
#include <stdio.h>
#include <mmal/util/mmal_util.h>
#include <mmal/util/mmal_util_params.h>

#include "splitter.h"
#include "utils.h"

//--------------------------------------------------------------------------------------------------
// Splitter
//--------------------------------------------------------------------------------------------------
Splitter::Splitter()
    : mpSplitter( NULL ),
    mpInputConnection( NULL )
{
    MMAL_STATUS_T status;
    
    status = mmal_component_create( MMAL_COMPONENT_DEFAULT_VIDEO_SPLITTER, &mpSplitter );

    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Unable to create splitter component\n" );
        goto error;
    }

    return;

error:
    if ( NULL != mpSplitter )
    {
        mmal_component_destroy( mpSplitter );
    }

    throw std::runtime_error( "Unable to create splitter" );
}

//--------------------------------------------------------------------------------------------------
Splitter::~Splitter()
{
    // Shut down output ports
    if ( NULL != mpSplitter )
    {
        for ( int portIdx = 0; portIdx < mpSplitter->output_num; portIdx++ )
        {
            if ( NULL != mpSplitter->output[ portIdx ]
                && mpSplitter->output[ portIdx ]->is_enabled )
            {
                mmal_port_disable( mpSplitter->output[ portIdx ] );
            }
        }
    }
    
    // Shut down input connection
    if ( NULL != mpInputConnection )
    {
        mmal_connection_destroy( mpInputConnection );
        mpInputConnection = NULL;
    }

    // Disable and destroy component
    if ( NULL != mpSplitter )
    {
        mmal_component_disable( mpSplitter );        
        mmal_component_destroy( mpSplitter );
        mpSplitter = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
MMAL_STATUS_T Splitter::connectPortToSplitter( MMAL_PORT_T* pPort )
{
    MMAL_STATUS_T status;
    
    MMAL_PORT_T* pInputPort = mpSplitter->input[ 0 ];
    mmal_format_copy( pInputPort->format, pPort->format );
    pInputPort->buffer_num = 3;
    status = mmal_port_format_commit( pInputPort );
    if ( MMAL_SUCCESS != status )
    {
        fprintf( stderr, "Error: Couldn't set splitter input port format : error %d", status );
        return status;
    }

    for( int i = 0; i < mpSplitter->output_num; i++ )
    {
        MMAL_PORT_T* pOutputPort = mpSplitter->output[ i ];
        pOutputPort->buffer_num = 3;
        mmal_format_copy( pOutputPort->format, pInputPort->format );
        status = mmal_port_format_commit( pOutputPort );
        if ( MMAL_SUCCESS != status )
        {
            fprintf( stderr, "Error: Couldn't set splitter output port format : error %d", status );
            return status;
        }
    }
    
    return Utils::connectPorts( pPort, mpSplitter->input[ 0 ], &mpInputConnection );
}
