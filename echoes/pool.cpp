#include <SoapySDR/Device.hpp>
#include "pool.h"


void _releaseBufCallback( void *_stream , const size_t handle, void *_dev )
{
    SoapySDR::Stream *stream = (SoapySDR::Stream *) _stream;
    SoapySDR::Device *dev = (SoapySDR::Device *) _dev;
    dev->releaseReadBuffer(stream, handle);
}
