
#ifndef UTILS_H
#define UTILS_H

#include <mmal/mmal.h>
#include <mmal/util/mmal_connection.h>

class Utils
{
    public: static MMAL_STATUS_T connectPorts( MMAL_PORT_T* pOutputPort, 
        MMAL_PORT_T* pInputPort, MMAL_CONNECTION_T** ppConnectionOut );
};

#endif // UTILS_H