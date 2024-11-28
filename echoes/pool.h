/******************************************************************************

    Echoes is a RF spectrograph for SDR devices designed for meteor scatter
    Copyright (C) 2018-2022  Giuseppe Massimo Bertani gmbertani(a)users.sourceforge.net


    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.
    

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, http://www.gnu.org/copyleft/gpl.html






*******************************************************************************
$Rev:: 255                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-07-17 13:37:43 +02#$: Date of last commit
$Id: pool.h 255 2018-07-17 11:37:43Z gmbertani $
*******************************************************************************/

#ifndef POOL_H
#define POOL_H

#include <QtCore>
#include "setup.h"
#include "settings.h"

void _releaseBufCallback( void *_stream , const size_t handle, void *_dev );

//T must be a pointer!
template <class T> class Pool
{

protected:

    //pool of T
    QVector<T>* pool;

    QQueue<int>* freeBufs;          ///<contains the free buffer indexes ready to be taken
    QQueue<int>* writtenBufs;       ///<contains the buffer written ready to be processed

    QMutex          protectionMutex;///<generic mutex to protect against concurrent calls

    QMutex          freedMutex;     ///< mutex released when a buffer is freed and locked when the required
                                    ///buffer is not found
                                    ///
    QWaitCondition  freedBuf;       ///< triggers when a buffer is freed and put in freeBufs queue

    //callback registration infos
    void (* releaseBufCallback)( void* , const size_t, void* );
    void* dev;
    void* stream;

    //debug only
    int locks;
    int unlocks;


public:

    ///
    /// \brief Pool
    ///
    ///
    ///
    explicit Pool()
    {
        QMutexLocker ml(&protectionMutex);

        pool = new QVector<T>;
        MY_ASSERT( nullptr !=   pool )

        freeBufs = new QQueue<int>;
        MY_ASSERT( nullptr !=   freeBufs )

        writtenBufs = new QQueue<int>;
        MY_ASSERT( nullptr !=   writtenBufs )

        releaseBufCallback = nullptr;
        dev = nullptr;
        stream = nullptr;

        locks = 0;
        unlocks = 0;
    }

    ~Pool()
    {
        if(pool != nullptr)
        {
            clear();
            delete pool;
        }

        if(writtenBufs != nullptr)
        {
            delete writtenBufs;
            writtenBufs = nullptr;
        }

        if(freeBufs != nullptr)
        {
            delete freeBufs;
            freeBufs = nullptr;
        }

    }

    void dump()
    {
        MYINFO << "---------------------------------";
        MYINFO << "Pool's dump";
        MYINFO << "Total elements: " << size();
        MYINFO << "Free elements: " << freeBufs->count();
        MYINFO << "Forwarded elements: " << writtenBufs->count();
        MYINFO << "In use elements: " << peekBusy();
        MYINFO << "---------------------------------";
    }

    ///
    /// \brief registerReleaseCallback
    /// \param _dev
    /// \param _stream
    ///
    void registerReleaseCallback(  void (*callback)( void*, const size_t, void* ), void* _dev, void* _stream  )
    {
        QMutexLocker ml(&protectionMutex);

        dev = _dev;
        stream = _stream;
        releaseBufCallback = callback;
    }

    ///
    /// \brief unregisterReleaseCallback
    ///
    void unregisterReleaseCallback()
    {
        QMutexLocker ml(&protectionMutex);
        releaseBufCallback = nullptr;
        dev = nullptr;
        stream = nullptr;
    }

    ///
    /// \brief useExternBuffers
    /// \return
    ///
    bool useExternalBuffers()
    {
        return (releaseBufCallback == nullptr)? false : true;
    }

    //general rule for return values:
    // boolean methods: false means failure
    // integer methods: -1 means failure
    // pointer methods: zero means failure


    ///
    /// \brief clear
    /// \return
    ///destroys all elements in the pool
    bool clear()
    {
        QMutexLocker ml(&protectionMutex);

        //MYDEBUG << "pool: " << this << " clear()";
        if(pool != 0)
        {
            T elem;
            for(int i = 0; i < pool->size(); i++)
            {
                elem = pool->at(i);
                MY_ASSERT( nullptr !=   elem )
                //MYDEBUG << "destroying element [" << elem->myUID() << "]";

                delete elem;
            }
            pool->clear();
            freeBufs->clear();
            writtenBufs->clear();
            MY_ASSERT(pool->count() == 0);
            return true;
        }
        return false;

    }


    ///
    /// \brief take
    /// Gets a clean buffer to write in
    /// returns -1 when all buffers have been written
    /// and no free ones are available, otherwise returns
    /// the buffer index. The related pointer is returned
    /// by getBuffer().
    /// \return

    int take(bool blocking = false)
    {
        //QMutexLocker ml(&protectionMutex);

        //MYDEBUG << "take()";
        int index = -1, count = 0;
        if( blocking == false )
        {
            count = freeBufs->size();
            if(count == 0)
            {
                //No more free  buffers available
                MYDEBUG << "no more free buffers";
                return -1;
            }
        }
        else
        {
            while(index == -1)
            {
                count = freeBufs->size();
                if(count == 0)
                {
                    MY_ASSERT( freedMutex.tryLock() == true )
                    locks++;
                    freedBuf.wait(&freedMutex);
                    freedMutex.unlock();
                }
            }
        }
        QMutexLocker ml(&protectionMutex);
        index = freeBufs->dequeue();
        //MYDEBUG << "buffer " << index << " taken";
        return index;
    }

    ///
    /// \brief take
    /// Gets the clean buffer having index id to write in
    /// returns -1 when all buffers have been written
    /// and no free ones are available, otherwise returns
    /// the buffer index. The related pointer is returned
    /// by getBuffer()
    /// \return
    int take(int handle, bool blocking = false)
    {
        //QMutexLocker ml(&protectionMutex);

        //MYDEBUG << "take(" << handle << ")";
        int index = -1, count = 0;

        if( blocking == false )
        {
            count = freeBufs->size();
            if(count == 0)
            {
                //No more free  buffers available
                return index;
            }

            QMutexLocker ml(&protectionMutex);
            index = freeBufs->indexOf(handle);
            if(index >= 0)
            {
                //required element found
                freeBufs->removeAt(index);
                return handle;
            }
        }
        else
        {
            while(index == -1)
            {
                count = freeBufs->size();
                if(count >= 0)
                {
                    MY_ASSERT( freedMutex.tryLock() == true )
                    locks++;

                    freedBuf.wait(&freedMutex);
                    freedMutex.unlock();
                }

                QMutexLocker ml(&protectionMutex);
                index = freeBufs->indexOf(handle);
                if(index >= 0)
                {
                    //required element found
                    freeBufs->removeAt(index);
                    return handle;
                }
            }
        }

        return index;
    }



    ///
    /// \brief forward
    /// enqueue the data buffer to be processed
    /// \param index
    /// \return
    bool forward( int index )
    {
        QMutexLocker ml(&protectionMutex);

        //MYDEBUG << "forward(" << index << ")";

        int count = freeBufs->size();
        if( count < 0 || count > pool->size() )
        {
            //MYDEBUG << "in freebuf: " << count << " elements";
            //MYDEBUG << "freeBufs=" << freeBufs;
        }

        if( index < 0 || index > pool->size()  )
        {
            //size cannot exceed the given pool->size()
            MYWARNING << "index " << index << " out of range";
            return false;
        }

        if( freeBufs->indexOf( index ) != -1 )
        {
            MYWARNING << "index " << index << " must be taken first";
            return false;
        }

        writtenBufs->enqueue( index );


        return true;
    }

    ///
    /// \brief discard
    /// releases a taken data buffer without processing
    /// \param index
    /// \return
    ///
    bool discard( int index )
    {
        QMutexLocker ml(&protectionMutex);

        //MYDEBUG << "discard(" << index << ")";

        int count = freeBufs->size();
        if( count < 0 || count > pool->size() )
        {
            //MYDEBUG << "in freebuf: " << count << " elements";
            //MYDEBUG << "freeBufs=" << freeBufs;
        }

        if( index < 0 || index > pool->size()  )
        {
            //size cannot exceed the given pool->size()
            MYWARNING << "index " << index << " out of range";
            return false;
        }

        if( freeBufs->indexOf( index ) != -1 )
        {
            MYWARNING << "index " << index << " must be taken first";
            return false;
        }

        T s = pool->value(index);
        MY_ASSERT( nullptr !=  s )
        freeBufs->enqueue( index );

        /*
         * NO! generates a race condition when stopping radio thread
        QQueue<int>::iterator it = writtenBufs->begin();
        while(it != writtenBufs->end())
        {
            if(*it == index)
            {
                writtenBufs->erase(it);
                break;
            }
        }
        */

        if(releaseBufCallback != nullptr)
        {
            releaseBufCallback( stream, index, dev );
        }
        freedBuf.wakeAll();
        unlocks++;
        return true;
    }


    ///
    /// \brief getData
    /// gets the index of the next data buffer to be processed
    /// \return
    ///

    int getData()
    {
        QMutexLocker ml(&protectionMutex);

        //MYDEBUG << "getData()";

        if( writtenBufs->isEmpty() == true )
        {
            //No data buffers available yet.
            //MYDEBUG << "getData() no data buffers available";
            QThread::yieldCurrentThread();
            return -1;
        }
        return writtenBufs->dequeue();
    }

    int getLastData()
    {
        QMutexLocker ml(&protectionMutex);

        //MYDEBUG << "getData()";

        if( writtenBufs->isEmpty() == true )
        {
            //No data buffers available yet.
            //MYDEBUG << "getData() no data buffers available";
            QThread::yieldCurrentThread();
            return -1;
        }
        int lastElement = writtenBufs->last();
        return lastElement;
    }

    ///
    /// \brief release
    /// releases the processed data buffer for recycling
    /// return false in case of failure
    ///
    /// \param index
    /// \return

    bool release( int index )
    {
        //QMutexLocker ml(&protectionMutex);

        if( freeBufs->count() > 0 && freeBufs->contains( index ) == true )
        {
            //No data buffers available yet.
            MYWARNING << "pool: " << this << " index " << index << " has already been freed (already present in freeBufs[])";
            QThread::yieldCurrentThread();
            return false;
        }

        if( index < 0)
        {
            MYWARNING << "pool: " << this << " index " << index << " is invalid";
            QThread::yieldCurrentThread();
            return false;
        }


        QMutexLocker ml(&protectionMutex);
        T s = pool->value(index);
        if( nullptr !=  s )
        {
            freeBufs->enqueue( index );
            if(releaseBufCallback != nullptr)
            {
                releaseBufCallback( stream, index, dev );
            }
        }
        //MYDEBUG << "buffer " << index << " released";
        freedBuf.wakeAll();
        unlocks++;
        return true;
    }

    ///
    /// \brief flush
    /// \return
    /// clears the writtenBufs from unprocessed
    /// buffers
    int flush()
    {
        QMutexLocker ml(&protectionMutex);

        //MYDEBUG << "flushing all unprocessed written buffers";
        int released = 0;

        while(!writtenBufs->isEmpty())
        {
            int index = writtenBufs->dequeue();
            freeBufs->enqueue( index );
            if(releaseBufCallback != nullptr)
            {
                releaseBufCallback( stream, index, dev );
            }
            freedBuf.wakeAll();
            released++;
            QThread::yieldCurrentThread();
        }
        return released;
    }

    ///
    /// \brief flush
    /// \return
    /// clears the oldest unprocessed
    /// buffer and makes it a free one
    void flushOne()
    {
        QMutexLocker ml(&protectionMutex);
        if(writtenBufs->count() > 0)
        {
            //MYDEBUG << "flushing oldest unprocessed written buffer";
            int index = writtenBufs->dequeue();
            freeBufs->enqueue( index );
            if(releaseBufCallback != nullptr)
            {
                releaseBufCallback( stream, index, dev );
            }
            freedBuf.wakeAll();
        }
    }

    ///
    /// \brief peekFree
    /// \return
    ///
    int peekFree()
    {
        QMutexLocker ml(&protectionMutex);
        if( freeBufs->isEmpty() == true )
        {
            QThread::yieldCurrentThread();
            return 0;
        }
        return freeBufs->count();
    }

    ///
    /// \brief peekBusy
    /// \return
    ///
    int peekBusy()
    {
        QMutexLocker ml(&protectionMutex);
        int busy = pool->size();
        if( writtenBufs->isEmpty() == false )
        {
            busy -=  writtenBufs->count();
        }

        if( freeBufs->isEmpty() == false )
        {
            busy -=  freeBufs->count();
        }

        return busy;
    }

    ///
    /// \brief peekData
    /// gets the number of data buffers ready without extraction
    /// \return
    ///

    int peekWritten()
    {
        QMutexLocker ml(&protectionMutex);

        if( writtenBufs->isEmpty() == true )
        {
            QThread::yieldCurrentThread();
            return 0;
        }

        return writtenBufs->count();
    }

    ///
    /// \brief size
    /// \return
    ///
    int size() const
    {
        return pool->size();
    }

    ///
    /// \brief getElem
    /// returns the given buffer or zero if the given index is
    /// out of range or a free one (not taken).
    /// Note that the element is not extracted,
    /// simply its pointer is returned.
    /// \param index
    /// \return

    T getElem( int index )
    {
        QMutexLocker ml(&protectionMutex);

        if( index < 0 ||
            index > pool->size() )
        {
            MYWARNING << "invalid parameter";
            return 0;
        }

        T s = pool->value(index);
        MY_ASSERT( nullptr !=  s)
        return s;
    }

    ///
    /// \brief insert
    /// \param elem
    ///
    void insert( int index, T elem )
    {
        QMutexLocker ml(&protectionMutex);
        pool->insert(index, elem);
        freeBufs->enqueue(index);
    }


};

#endif // POOL_H
