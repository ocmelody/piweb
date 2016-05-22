
#ifndef IMAGE_ENCODER_H
#define IMAGE_ENCODER_H

#include <mmal/mmal.h>
#include <mmal/util/mmal_connection.h>
#include <mmal/util/mmal_default_components.h>

//--------------------------------------------------------------------------------------------------
class ImageEncoder
{
    public: typedef void (*EncoderBufferCB)( MMAL_BUFFER_HEADER_T* pBuffer, void* pUserData );
    
    public: ImageEncoder( MMAL_FOURCC_T encoding, int jpegQFactor );
    public: ~ImageEncoder();

    public: MMAL_STATUS_T connectPortToEncoder( MMAL_PORT_T* pPort );
    public: MMAL_STATUS_T enableOutputPort( EncoderBufferCB callback, void* pUserData );
    
    private: static void mainEncoderBufferCB( MMAL_PORT_T* pPort, MMAL_BUFFER_HEADER_T* pBuffer );
    
    private: MMAL_COMPONENT_T* mpEncoder;
    private: MMAL_CONNECTION_T* mpInputConnection;
    private: MMAL_POOL_T* mpEncoderPool;
    private: EncoderBufferCB mUserEncoderBufferCB;
    private: void* mpUserData;
};

#endif // IMAGE_ENCODER_H