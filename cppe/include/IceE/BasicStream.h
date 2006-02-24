// **********************************************************************
//
// Copyright (c) 2003-2005 ZeroC, Inc. All rights reserved.
//
// This copy of Ice-E is licensed to you under the terms described in the
// ICEE_LICENSE file included in this distribution.
//
// **********************************************************************

#ifndef ICEE_BASIC_STREAM_H
#define ICEE_BASIC_STREAM_H

#include <IceE/ProxyF.h>

#include <IceE/Buffer.h>
#include <IceE/AutoArray.h>
#include <IceE/Protocol.h>

namespace Ice
{

class UserException;

}

namespace IceInternal
{

class Instance;

class ICE_API BasicStream : public Buffer
{
public:

    BasicStream(Instance* instance, int messageSizeMax) :
	_instance(instance),
	_currentReadEncaps(0),
	_currentWriteEncaps(0),
	_messageSizeMax(messageSizeMax),
	_seqDataStack(0)
    {
	// Inlined for performance reasons.
    }

    ~BasicStream()
    {
	// Inlined for performance reasons.

	if(_currentReadEncaps != &_preAllocatedReadEncaps ||
	   _currentWriteEncaps != &_preAllocatedWriteEncaps ||
	   _seqDataStack)
	{
	    clear(); // Not inlined.
	}
    }

    void clear();

    //
    // Must return Instance*, because we don't hold an InstancePtr for
    // optimization reasons (see comments below).
    //
    Instance* instance() const { return _instance; } // Inlined for performance reasons.

    void swap(BasicStream&);

    void resize(Container::size_type sz)
    {
	if(sz > _messageSizeMax)
	{
	    throwMemoryLimitException(__FILE__, __LINE__);
	}
	
	b.resize(sz);
    }

    void reset() // Inlined for performance reasons.
    {
        b.reset();
	i = b.begin();
    }

    void startSeq(int, int);
    void checkSeq();
    void checkSeq(int);
    void checkFixedSeq(int, int); // For sequences of fixed-size types.
    void endElement()
    {
	assert(_seqDataStack);
	--_seqDataStack->numElements;
    }
    void endSeq(int);

    void startWriteEncaps()
    {
	WriteEncaps* oldEncaps = _currentWriteEncaps;
	if(!oldEncaps) // First allocated encaps?
	{
	    _currentWriteEncaps = &_preAllocatedWriteEncaps;
	}
	else
	{
	    _currentWriteEncaps = new WriteEncaps();
	    _currentWriteEncaps->previous = oldEncaps;
	}
	_currentWriteEncaps->start = b.size();

	write(Ice::Int(0)); // Placeholder for the encapsulation length.
	write(encodingMajor);
	write(encodingMinor);
    }
    void endWriteEncaps()
    {
	assert(_currentWriteEncaps);
	Container::size_type start = _currentWriteEncaps->start;
	Ice::Int sz = static_cast<Ice::Int>(b.size() - start); // Size includes size and version.
	Ice::Byte* dest = &(*(b.begin() + start));

#ifdef ICE_BIG_ENDIAN
	const Ice::Byte* src = reinterpret_cast<const Ice::Byte*>(&sz) + sizeof(Ice::Int) - 1;
	*dest++ = *src--;
	*dest++ = *src--;
	*dest++ = *src--;
	*dest = *src;
#else
	const Ice::Byte* src = reinterpret_cast<const Ice::Byte*>(&sz);
	*dest++ = *src++;
	*dest++ = *src++;
	*dest++ = *src++;
	*dest = *src;
#endif

	WriteEncaps* oldEncaps = _currentWriteEncaps;
	_currentWriteEncaps = _currentWriteEncaps->previous;
	if(oldEncaps == &_preAllocatedWriteEncaps)
	{
	    oldEncaps->reset();
	}
	else
	{
	    delete oldEncaps;
	}
    }

    void startReadEncaps()
    {
	ReadEncaps* oldEncaps = _currentReadEncaps;
	if(!oldEncaps) // First allocated encaps?
	{
	    _currentReadEncaps = &_preAllocatedReadEncaps;
	}
	else
	{
	    _currentReadEncaps = new ReadEncaps();
	    _currentReadEncaps->previous = oldEncaps;
	}
	_currentReadEncaps->start = i - b.begin();
	
	//
	// I don't use readSize() and writeSize() for encapsulations,
	// because when creating an encapsulation, I must know in advance
	// how many bytes the size information will require in the data
	// stream. If I use an Int, it is always 4 bytes. For
	// readSize()/writeSize(), it could be 1 or 5 bytes.
	//
	Ice::Int sz;
	read(sz);
	if(sz < 0)
	{
	    throwNegativeSizeException(__FILE__, __LINE__);
	}
	if(i - sizeof(Ice::Int) + sz > b.end())
	{
	    throwUnmarshalOutOfBoundsException(__FILE__, __LINE__);
	}
	_currentReadEncaps->sz = sz;
	
	Ice::Byte eMajor;
	Ice::Byte eMinor;
	read(eMajor);
	read(eMinor);
	if(eMajor != encodingMajor
	   || static_cast<unsigned char>(eMinor) > static_cast<unsigned char>(encodingMinor))
	{
	    throwUnsupportedEncodingException(__FILE__, __LINE__, eMajor, eMinor);
	}
	_currentReadEncaps->encodingMajor = eMajor;
	_currentReadEncaps->encodingMinor = eMinor;
    }
    void endReadEncaps()
    {
	assert(_currentReadEncaps);
	Container::size_type start = _currentReadEncaps->start;
	Ice::Int sz = _currentReadEncaps->sz;
	i = b.begin() + start + sz;

	ReadEncaps* oldEncaps = _currentReadEncaps;
	_currentReadEncaps = _currentReadEncaps->previous;
	if(oldEncaps == &_preAllocatedReadEncaps)
	{
	    oldEncaps->reset();
	}
	else
	{
	    delete oldEncaps;
	}
    }
    Ice::Int getReadEncapsSize();
    void skipEncaps();

    void startWriteSlice();
    void endWriteSlice();

    void startReadSlice();
    void endReadSlice();
    void skipSlice();

    void writeSize(Ice::Int v) // Inlined for performance reasons.
    {
	assert(v >= 0);
	if(v > 254)
	{
	    write(Ice::Byte(255));
	    write(v);
	}
	else
	{
	    write(static_cast<Ice::Byte>(v));
	}
    }

    void
    readSize(Ice::Int& v) // Inlined for performance reasons.
    {
	Ice::Byte byte;
	read(byte);
	unsigned val = static_cast<unsigned char>(byte);
	if(val == 255)
	{
	    read(v);
	    if(v < 0)
	    {
		throwNegativeSizeException(__FILE__, __LINE__);
	    }
	}
	else
	{
	    v = static_cast<Ice::Int>(static_cast<unsigned char>(byte));
	}
    }

    void writeBlob(const std::vector<Ice::Byte>&);
    void readBlob(std::vector<Ice::Byte>&, Ice::Int);

    void writeBlob(const Ice::Byte* v, Container::size_type sz)
    {
	if(sz > 0)
	{
	    Container::size_type pos = b.size();
	    resize(pos + sz);
	    memcpy(&b[pos], &v[0], sz);
	}
    }

    void readBlob(const Ice::Byte*& v, Container::size_type sz)
    {
	if(sz > 0)
	{
	    v = i;
	    if(static_cast<Container::size_type>(b.end() - i) < sz)
	    {
		throwUnmarshalOutOfBoundsException(__FILE__, __LINE__);
	    }
	    i += sz;
	}
	else
	{
	    v = i;
	}
    }

    void write(Ice::Byte v) // Inlined for performance reasons.
    {
	b.push_back(v);
    }
    void read(Ice::Byte& v) // Inlined for performance reasons.
    {
	if(i >= b.end())
	{
	    throwUnmarshalOutOfBoundsException(__FILE__, __LINE__);
	}
	v = *i++;
    }

    void write(const Ice::Byte*, const Ice::Byte*);
    void read(std::pair<const Ice::Byte*, const Ice::Byte*>&);

    void write(bool v) // Inlined for performance reasons.
    {
	b.push_back(static_cast<Ice::Byte>(v));
    }
    void write(const std::vector<bool>&);
    void write(const bool*, const bool*);
    void read(bool& v) // Inlined for performance reasons.
    {
	if(i >= b.end())
	{
	    throwUnmarshalOutOfBoundsException(__FILE__, __LINE__);
	}
	v = *i++;
    }
    void read(std::vector<bool>&);
    void read(std::pair<const bool*, const bool*>&, IceUtil::auto_array<bool>&);

    void write(Ice::Short);
    void read(Ice::Short&);
    void write(const Ice::Short*, const Ice::Short*);
    void read(std::vector<Ice::Short>&);
    void read(std::pair<const Ice::Short*, const Ice::Short*>&, IceUtil::auto_array<Ice::Short>&);

    void
    write(Ice::Int v) // Inlined for performance reasons.
    {
	Container::size_type pos = b.size();
	resize(pos + sizeof(Ice::Int));
	Ice::Byte* dest = &b[pos];
#ifdef ICE_BIG_ENDIAN
	const Ice::Byte* src = reinterpret_cast<const Ice::Byte*>(&v) + sizeof(Ice::Int) - 1;
	*dest++ = *src--;
	*dest++ = *src--;
	*dest++ = *src--;
	*dest = *src;
#else
	const Ice::Byte* src = reinterpret_cast<const Ice::Byte*>(&v);
	*dest++ = *src++;
	*dest++ = *src++;
	*dest++ = *src++;
	*dest = *src;
#endif
    }

    void read(Ice::Int& v) // Inlined for performance reasons.
    {
	if(b.end() - i < static_cast<int>(sizeof(Ice::Int)))
	{
	    throwUnmarshalOutOfBoundsException(__FILE__, __LINE__);
	}
	const Ice::Byte* src = &(*i);
	i += sizeof(Ice::Int);
#ifdef ICE_BIG_ENDIAN
	Ice::Byte* dest = reinterpret_cast<Ice::Byte*>(&v) + sizeof(Ice::Int) - 1;
	*dest-- = *src++;
	*dest-- = *src++;
	*dest-- = *src++;
	*dest = *src;
#else
	Ice::Byte* dest = reinterpret_cast<Ice::Byte*>(&v);
	*dest++ = *src++;
	*dest++ = *src++;
	*dest++ = *src++;
	*dest = *src;
#endif
    }

    void write(const Ice::Int*, const Ice::Int*);
    void read(std::vector<Ice::Int>&);
    void read(std::pair<const Ice::Int*, const Ice::Int*>&, IceUtil::auto_array<Ice::Int>&);

    void write(Ice::Long);
    void read(Ice::Long&);
    void write(const Ice::Long*, const Ice::Long*);
    void read(std::vector<Ice::Long>&);
    void read(std::pair<const Ice::Long*, const Ice::Long*>&, IceUtil::auto_array<Ice::Long>&);

    void write(Ice::Float);
    void read(Ice::Float&);
    void write(const Ice::Float*, const Ice::Float*);
    void read(std::vector<Ice::Float>&);
    void read(std::pair<const Ice::Float*, const Ice::Float*>&, IceUtil::auto_array<Ice::Float>&);

    void write(Ice::Double);
    void read(Ice::Double&);
    void write(const Ice::Double*, const Ice::Double*);
    void read(std::vector<Ice::Double>&);
    void read(std::pair<const Ice::Double*, const Ice::Double*>&, IceUtil::auto_array<Ice::Double>&);

    //
    // NOTE: This function is not implemented. It is declared here to
    // catch programming errors that assume a call such as write("")
    // will invoke write(const std::string&), when in fact the compiler
    // will silently select a different overloading. A link error is the
    // intended result.
    //
    void write(const char*);

    void write(const std::string& v)
    {
	Ice::Int sz = static_cast<Ice::Int>(v.size());
	writeSize(sz);
	if(sz > 0)
	{
	    Container::size_type pos = b.size();
	    resize(pos + sz);
	    memcpy(&b[pos], v.c_str(), sz);
	}
    }
    void write(const std::string*, const std::string*);
    void read(std::string& v)
    {
	Ice::Int sz;
	readSize(sz);
	if(sz > 0)
	{
	    if(b.end() - i < sz)
	    {
		throwUnmarshalOutOfBoundsException(__FILE__, __LINE__);
	    }
	    std::string(reinterpret_cast<const char*>(&*i), reinterpret_cast<const char*>(&*i) + sz).swap(v);
//	    v.assign(reinterpret_cast<const char*>(&(*i)), sz);
	    i += sz;
	}
	else
	{
	    v.clear();
	}
    }
    void read(std::vector<std::string>&);

    void write(const Ice::ObjectPrx&);
    void read(Ice::ObjectPrx&);

    void write(const Ice::UserException&);
    void throwException();

private:

    //
    // I can't throw these exception from inline functions from within
    // this file, because I cannot include the header with the
    // exceptions. Doing so would screw up the whole include file
    // ordering.
    //
    void throwUnmarshalOutOfBoundsException(const char*, int);
    void throwMemoryLimitException(const char*, int);
    void throwNegativeSizeException(const char*, int);
    void throwUnsupportedEncodingException(const char*, int, Ice::Byte, Ice::Byte);

    //
    // Optimization. The instance may not be deleted while a
    // stack-allocated BasicStream still holds it.
    //
    Instance* _instance;

    class ICE_API ReadEncaps : private ::IceUtil::noncopyable
    {
    public:

	ReadEncaps() : previous(0) { } // Inlined for performance reasons.
	~ReadEncaps() { } // Inlined for performance reasons.

	void reset() { previous = 0; } // Inlined for performance reasons.
	void swap(ReadEncaps&);

	Container::size_type start;
	Ice::Int sz;

	Ice::Byte encodingMajor;
	Ice::Byte encodingMinor;

	ReadEncaps* previous;
    };

    class ICE_API WriteEncaps : private ::IceUtil::noncopyable
    {
    public:

	WriteEncaps() : writeIndex(0), previous(0) { } // Inlined for performance reasons.
	~WriteEncaps() { } // Inlined for performance reasons.

	void reset() { writeIndex = 0; previous = 0; } // Inlined for performance reasons.
	void swap(WriteEncaps&);

	Container::size_type start;

	Ice::Int writeIndex;

	WriteEncaps* previous;
    };

    ReadEncaps* _currentReadEncaps;
    WriteEncaps* _currentWriteEncaps;

    ReadEncaps _preAllocatedReadEncaps;
    WriteEncaps _preAllocatedWriteEncaps;

    Container::size_type _readSlice;
    Container::size_type _writeSlice;

    const Container::size_type _messageSizeMax;

    struct SeqData
    {
	SeqData(int, int);
	int numElements;
	int minSize;
	SeqData* previous;
    };
    SeqData* _seqDataStack;
};

} // End namespace IceInternal

#endif
