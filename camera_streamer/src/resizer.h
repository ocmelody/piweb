
#ifndef RESIZER_H
#define RESIZER_H

#include <mmal/mmal.h>
#include <mmal/util/mmal_connection.h>
#include <mmal/util/mmal_default_components.h>

//--------------------------------------------------------------------------------------------------
class Resizer
{
    public: Resizer( int width, int height );
    public: ~Resizer();

    public: MMAL_STATUS_T connectPortToResizer( MMAL_PORT_T* pPort );
    public: MMAL_PORT_T* getOutputPort() { return mpResizer->output[ 0 ]; }
     
    private: MMAL_COMPONENT_T* mpResizer;
    private: MMAL_CONNECTION_T* mpInputConnection;
    private: int mWidth;
    private: int mHeight;
};

#endif // RESIZER_H