
#ifndef SPLITTER_H
#define SPLITTER_H

#include <mmal/mmal.h>
#include <mmal/util/mmal_connection.h>
#include <mmal/util/mmal_default_components.h>

//--------------------------------------------------------------------------------------------------
class Splitter
{
    public: Splitter();
    public: ~Splitter();

    public: MMAL_STATUS_T connectPortToSplitter( MMAL_PORT_T* pPort );
    public: MMAL_PORT_T* getOutputPort( int portIdx ) { return mpSplitter->output[ portIdx ]; }
     
    private: MMAL_COMPONENT_T* mpSplitter;
    private: MMAL_CONNECTION_T* mpInputConnection;
};

#endif // SPLITTER_H