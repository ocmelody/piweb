
#include "utils.h"

//--------------------------------------------------------------------------------------------------
// Utils
//--------------------------------------------------------------------------------------------------
MMAL_STATUS_T Utils::connectPorts( MMAL_PORT_T* pOutputPort, MMAL_PORT_T* pInputPort, 
                                   MMAL_CONNECTION_T** ppConnectionOut )
{
    MMAL_STATUS_T status;

    status = mmal_connection_create( ppConnectionOut, pOutputPort, pInputPort, 
        MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT );

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