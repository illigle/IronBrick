/*
* This Source Code Form is subject to the terms of the Mozilla Public License Version 2.0.
* If a copy of the MPL was not distributed with this file, 
* You can obtain one at http://mozilla.org/MPL/2.0/.

* Covered Software is provided on an ¡°as is¡± basis, 
* without warranty of any kind, either expressed, implied, or statutory,
* that the Covered Software is free of defects, merchantable, 
* fit for a particular purpose or non-infringing.
 
* Copyright (c) Wei Dongliang <illigle@163.com>.
*/

#ifndef _IRONBRICK_BITSUTILITY_H_
#define _IRONBRICK_BITSUTILITY_H_

#include "IrkCommon.h"

#ifdef _MSC_VER

extern "C" {
unsigned char _bittest( long const*, long );
unsigned char _bittestandset( long*, long );
unsigned char _bittestandreset( long*, long );
unsigned char _bittestandcomplement( long*, long );
unsigned char _BitScanForward( unsigned long*, unsigned long );
unsigned char _BitScanReverse( unsigned long*, unsigned long );
unsigned short _byteswap_ushort( unsigned short );
unsigned long _byteswap_ulong( unsigned long );
unsigned __int64 _byteswap_uint64( unsigned __int64 );
}
#pragma intrinsic( _bittest, _bittestandset, _bittestandreset, _bittestandcomplement )
#pragma intrinsic( _BitScanForward, _BitScanReverse )
#pragma intrinsic( _byteswap_ushort, _byteswap_ulong, _byteswap_uint64 )

#ifdef _M_X64
extern "C" {
    unsigned char _bittest64( __int64 const*, __int64 );
    unsigned char _bittestandset64( __int64*, __int64 );
    unsigned char _bittestandreset64( __int64*, __int64 );
    unsigned char _bittestandcomplement64( __int64*, __int64 );
    unsigned char _BitScanForward64( unsigned long*, unsigned __int64 );
    unsigned char _BitScanReverse64( unsigned long*, unsigned __int64 );
}
#pragma intrinsic( _bittest64, _bittestandset64, _bittestandreset64, _bittestandcomplement64 )
#pragma intrinsic( _BitScanForward64, _BitScanReverse64 )
#endif

typedef int16_t     int16_as;
typedef int32_t     int32_as;
typedef int64_t     int64_as;
typedef uint16_t    uint16_as;
typedef uint32_t    uint32_as;
typedef uint64_t    uint64_as;

#elif defined(__GNUC__) || defined(__clang__)

// for strict aliasing
typedef int16_t __attribute__((may_alias))  int16_as;
typedef int32_t __attribute__((may_alias))  int32_as;
typedef int64_t __attribute__((may_alias))  int64_as;
typedef uint16_t __attribute__((may_alias)) uint16_as;
typedef uint32_t __attribute__((may_alias)) uint32_as;
typedef uint64_t __attribute__((may_alias)) uint64_as;

#else
#error unsupported compiler
#endif

//======================================================================================================================

// little-endian read-write
#define LE_READ16(src)          (*(uint16_as*)(src))
#define LE_READ32(src)          (*(uint32_as*)(src))
#define LE_READ64(src)          (*(uint64_as*)(src))
#define LE_WRITE16(dst, val)    *(uint16_as*)(dst) = (val);
#define LE_WRITE32(dst, val)    *(uint32_as*)(dst) = (val);
#define LE_WRITE64(dst, val)    *(uint64_as*)(dst) = (val);

// big-endian read-write
#ifdef _MSC_VER
#define BE_READ32(src)          ((uint32_t)_byteswap_ulong( *(uint32_t*)(src) ))
#define BE_READ64(src)          ((uint64_t)_byteswap_uint64( *(uint64_t*)(src) ))
#define BE_WRITE32(dst, val)    *(uint32_t*)(dst) = _byteswap_ulong( (uint32_t)(val) )
#define BE_WRITE64(dst, val)    *(uint64_t*)(dst) = _byteswap_uint64( (uint64_t)(val) )
#else
#define BE_READ32(src)          ((uint32_t)__builtin_bswap32( *(uint32_as*)(src) ))
#define BE_READ64(src)          ((uint64_t)__builtin_bswap64( *(uint64_as*)(src) ))
#define BE_WRITE32(dst, val)    *(uint32_as*)(dst) = __builtin_bswap32( (uint32_t)(val) )
#define BE_WRITE64(dst, val)    *(uint64_as*)(dst) = __builtin_bswap64( (uint64_t)(val) )
#endif
#define BE_READ16(src)          ((uint16_t)((*(uint8_t*)(src)<<8) | *((uint8_t*)(src)+1)))
#define BE_WRITE16(dst, val)    do{ *(uint8_t*)(dst) = (uint8_t)((val)>>8); \
                                    *((uint8_t*)(dst)+1) = (uint8_t)(val); }while(0)
// aliasing copy
#define COPY_16(dst, src)       *(uint16_as*)(dst) = *(uint16_as*)(src);
#define COPY_32(dst, src)       *(uint32_as*)(dst) = *(uint32_as*)(src);
#define COPY_64(dst, src)       *(uint64_as*)(dst) = *(uint64_as*)(src);

//======================================================================================================================
namespace irk {

#ifdef _MSC_VER

// return specified bit
__forceinline uint8_t bit_get( uint32_t val, int idx )
{
    return _bittest( (const long*)&val, idx );
}
__forceinline uint8_t bit_get( int32_t val, int idx )
{
    return _bittest( (const long*)&val, idx );
}
// set specified bit
__forceinline void bit_set( uint32_t* pval, int idx )
{
    *pval |= (1u << idx);
}
// reset specified bit
__forceinline void bit_reset( uint32_t* pval, int idx )
{
    *pval &= ~(1u << idx);
}
// complement specified bit
__forceinline void bit_flip( uint32_t* pval, int idx )
{
    *pval ^= (1u << idx);
}
// set specified bit and return old bit
__forceinline uint8_t bit_get_set( uint32_t* pval, int idx )
{
    return _bittestandset( (long*)pval, idx );
}
// reset specified bit and return old bit
__forceinline uint8_t bit_get_reset( uint32_t* pval, int idx )
{
    return _bittestandreset( (long*)pval, idx );
}
// complement specified bit and return old bit
__forceinline uint8_t bit_get_flip( uint32_t* pval, int idx )
{
    return _bittestandcomplement( (long*)pval, idx );
}
// byte-swap
__forceinline uint16_t byte_swap( uint16_t value )
{
    return _byteswap_ushort( value );
}
__forceinline uint32_t byte_swap( uint32_t value )
{
    return _byteswap_ulong( value );
}
__forceinline uint64_t byte_swap( uint64_t value )
{
    return _byteswap_uint64( value );
}
// return MSB position of a set bit, -1 if value == 0
__forceinline int msb_index( uint32_t value )
{
#ifndef _M_X64
    _asm
    {
        mov   ecx, -1;
        bsr   eax, value;
        cmovz eax, ecx;
    }
#else
    unsigned long msb;
    int res = _BitScanReverse( &msb, value );
    return static_cast<int>(msb) | (res - 1);
#endif
}
// return LSB position of a set bit, -1 if value == 0
__forceinline int lsb_index( uint32_t value )
{
#ifndef _M_X64
    _asm
    {
        mov   ecx, -1;
        bsf   eax, value;
        cmovz eax, ecx;
    }
#else
    unsigned long lsb;
    int res = _BitScanForward( &lsb, value );
    return static_cast<int>(lsb) | (res - 1);
#endif
}
// return the minimum bits count required to store value
__forceinline int bit_mincnt( uint32_t value )
{
    unsigned long msb = 0;
    if( value != 0 )
        _BitScanReverse( &msb, value );
    return static_cast<int>(1 + msb);
}

#ifdef _M_X64   // WIN64

// return specified bit
__forceinline uint8_t bit_get( uint64_t val, int idx )
{
    return _bittest64( (const __int64*)&val, idx );
}
__forceinline uint8_t bit_get( int64_t val, int idx )
{
    return _bittest64( (const __int64*)&val, idx );
}
// set specified bit
__forceinline void bit_set( uint64_t* pval, int idx )
{
    *pval |= (1ull << idx);
}
// reset specified bit
__forceinline void bit_reset( uint64_t* pval, int idx )
{
    *pval &= ~(1ull << idx);
}
// complement specified bit
__forceinline void bit_flip( uint64_t* pval, int idx )
{
    *pval ^= (1ull << idx);
}
// set specified bit and return old bit
__forceinline uint8_t bit_get_set( uint64_t* pval, int idx )
{
    return _bittestandset64( (__int64*)pval, idx );
}
// reset specified bit and return old bit
__forceinline uint8_t bit_get_reset( uint64_t* pval, int idx )
{
    return _bittestandreset64( (__int64*)pval, idx );
}
// complement specified bit and return old bit
__forceinline uint8_t bit_get_flip( uint64_t* pval, int idx )
{
    return _bittestandcomplement64( (__int64*)pval, idx );
}
// return MSB position of a set bit, -1 if value == 0
__forceinline int msb_index( uint64_t value )
{
    unsigned long msb;
    int res = _BitScanReverse64( &msb, value );
    return static_cast<int>(msb) | (res - 1);
}
// return LSB position of a set bit, -1 if value == 0
__forceinline int lsb_index( uint64_t value )
{
    unsigned long lsb;
    int res = _BitScanForward64( &lsb, value );
    return static_cast<int>(lsb) | (res - 1);
}
// return the minimum bits count required to store value
__forceinline int bit_mincnt( uint64_t value )
{
    unsigned long msb = 0;
    if( value != 0 )
        _BitScanReverse64( &msb, value );
    return static_cast<int>(1 + msb);
}

#endif  // WIN64

#else   // GCC or Clang

// return specified bit
inline uint8_t bit_get( uint32_t val, int idx )
{
    return static_cast<uint8_t>((val >> idx) & 1);
}
inline uint8_t bit_get( int32_t val, int idx )
{
    return static_cast<uint8_t>((val >> idx) & 1);
}
// set specified bit
inline void bit_set( uint32_t* pval, int idx )
{
    *pval |= (1u << idx);
}
// reset specified bit
inline void bit_reset( uint32_t* pval, int idx )
{
    *pval &= ~(1u << idx);
}
// complement specified bit
inline void bit_flip( uint32_t* pval, int idx )
{
    *pval ^= (1u << idx);
}
// set specified bit and return old bit
inline uint8_t bit_get_set( uint32_t* pval, int idx )
{
    uint8_t res;
    __asm__ __volatile__
    (
        "btsl %2, %1\n\t"
        "setc %0"
        : "=q"(res), "+m"(*pval)
        : "r"(idx)
        : "cc"
    );
    return res;
}
// reset specified bit and return old bit
inline uint8_t bit_get_reset( uint32_t* pval, int idx )
{
    uint8_t res;
    __asm__ __volatile__
    (
        "btrl %2, %1\n\t"
        "setc %0"
        : "=q"(res), "+m"(*pval)
        : "r"(idx)
        : "cc"
    );
    return res;
}
// complement specified bit and return old bit
inline uint8_t bit_get_flip( uint32_t* pval, int idx )
{
    uint8_t res;
    __asm__ __volatile__
    (
        "btcl %2, %1\n\t"
        "setc %0"
        : "=q"(res), "+m"(*pval)
        : "r"(idx)
        : "cc"
    );
    return res;
}
// byte-swap
inline uint16_t byte_swap( uint16_t value )
{
    return ((value & 0xFF) << 8) | (value >> 8);
}
inline uint32_t byte_swap( uint32_t value )
{
    return __builtin_bswap32( value );
}
inline uint64_t byte_swap( uint64_t value )
{
    return __builtin_bswap64( value );
}
// return MSB position of a set bit, -1 if value == 0
inline int msb_index( uint32_t value )
{
    if( value != 0 )
        return 31 - __builtin_clz( value );
    return -1;
}
// return LSB position of a set bit, -1 if value == 0
inline int lsb_index( uint32_t value )
{
    if( value != 0 )
        return __builtin_ctz( value );
    return -1;
}
// return the minimum bits count required to store value
inline int bit_mincnt( uint32_t value )
{
    if( value != 0 )
        return 32 - __builtin_clz( value );
    return 1;
}

#ifdef __x86_64__       // Linux64

// return specified bit
inline uint8_t bit_get( uint64_t val, int idx )
{
    return static_cast<uint8_t>((val >> idx) & 1);
}
inline uint8_t bit_get( int64_t val, int idx )
{
    return static_cast<uint8_t>((val >> idx) & 1);
}
// set specified bit
inline void bit_set( uint64_t* pval, int idx )
{
    *pval |= (1ull << idx);
}
// reset specified bit
inline void bit_reset( uint64_t* pval, int idx )
{
    *pval &= ~(1ull << idx);
}
// complement specified bit
inline void bit_flip( uint64_t* pval, int idx )
{
    *pval ^= (1ull << idx);
}
// set specified bit and return old bit
inline uint8_t bit_get_set( uint64_t* pval, int idx )
{
    uint8_t res;
    __asm__ __volatile__
    (
        "btsq %2, %1\n\t"
        "setc %0"
        : "=q"(res), "+m"(*pval)
        : "r"((uint64_t)idx)
        : "cc"
    );
    return res;
}
// reset specified bit and return old bit
inline uint8_t bit_get_reset( uint64_t* pval, int idx )
{
    uint8_t res;
    __asm__ __volatile__
    (
        "btrq %2, %1\n\t"
        "setc %0"
        : "=q"(res), "+m"(*pval)
        : "r"((uint64_t)idx)
        : "cc"
    );
    return res;
}
// complement specified bit and return old bit
inline uint8_t bit_get_flip( uint64_t* pval, int idx )
{
    uint8_t res;
    __asm__ __volatile__
    (
        "btcq %2, %1\n\t"
        "setc %0"
        : "=q"(res), "+m"(*pval)
        : "r"((uint64_t)idx)
        : "cc"
    );
    return res;
}
// return MSB position of a set bit, -1 if value == 0
inline int msb_index( uint64_t value )
{
    if( value != 0 )
        return 63 - __builtin_clzll( value );
    return -1;
}
// return LSB position of a set bit, -1 if value == 0
inline int lsb_index( uint64_t value )
{
    if( value != 0 )
        return __builtin_ctzll( value );
    return -1;
}
// return the minimum bits count required to store value
inline int bit_mincnt( uint64_t value )
{
    if( value != 0 )
        return 64 - __builtin_clzll( value );
    return 1;
}
#endif  // __x86_64__

#endif // #ifdef _MSC_VER

//======================================================================================================================
// big-endian bits stream reader
// WARNING: the reader will prefetch 4 bytes, so make sure the input buffer has at least 4-byte padding!

class BitsReader : IrkNocopy
{
public:
    BitsReader() : m_pBuf(nullptr), m_Size(0), m_pCur(nullptr), m_Offset(0) {}
    BitsReader( const uint8_t* buff, size_t size, size_t offset = 0 )
    {
        this->set_buffer( buff, size, offset );
    }

    // set read buffer, the buffer should have at least 4-byte padding
    void set_buffer( const uint8_t* buff, size_t size, size_t offset = 0 )
    {
        m_pBuf = buff;
        m_Size = size;
        m_pCur = buff + (offset >> 3);
        m_Offset = (uint32_t)(offset & 7);
    }

    // set current read offset( bits )
    void set_offset( size_t offset )
    {
        assert( offset <= m_Size * 8 );
        m_pCur = m_pBuf + (offset >> 3);
        m_Offset = (uint32_t)(offset & 7);
    }

    const uint8_t* buffer() const   { return m_pBuf; }
    size_t buffer_size() const      { return m_Size; }
    size_t offset() const           { return (size_t)(m_pCur - m_pBuf) * 8 + m_Offset; }

    // is read buffer exhausted
    bool exhausted() const          { return m_pCur > m_pBuf + m_Size; }

    // is current read position byte-aligned
    bool byte_aligned() const       { return m_Offset == 0; }

    // read from next byte-aligned position
    void make_byte_aligned()
    {
        m_pCur += (m_Offset + 7) >> 3;
        m_Offset = 0;
    }

    // read specified count of bits, requires bitCnt > 0 && bitCnt <= 24
    uint32_t read_bits( uint32_t bitCnt )
    {
        assert( bitCnt > 0 && bitCnt <= 24 );
        uint32_t value = BE_READ32( m_pCur );
        value = (value << m_Offset) >> (32 - bitCnt);
        m_Offset += bitCnt;
        m_pCur += m_Offset >> 3;
        m_Offset &= 7;
        return value;
    }
    // read specified count of bits, requires bitCnt > 0 && bitCnt <= 32
    uint32_t read_bits_long( uint32_t bitCnt )
    {
        assert( bitCnt > 0 && bitCnt <= 32 );
        uint32_t value = BE_READ32( m_pCur );
        value = (value << m_Offset) | (m_pCur[4] >> (8 - m_Offset));
        value >>= (32 - bitCnt);
        m_Offset += bitCnt;
        m_pCur += m_Offset >> 3;
        m_Offset &= 7;
        return value;
    }
    // read next one bit
    uint8_t read1()
    {
        uint8_t value = m_pCur[0] >> (m_Offset ^ 7);
        m_Offset += 1;
        m_pCur += m_Offset >> 3;
        m_Offset &= 7;
        return (value & 1);
    }
    // read next 8 bits
    uint8_t read8()
    {
        uint32_t value = (m_pCur[0] << 8) | m_pCur[1];
        value >>= (8 - m_Offset);
        m_pCur += 1;
        return (uint8_t)(value & 0xFF);
    }
    // read next 32 bits
    uint32_t read32()
    {
        uint32_t value = BE_READ32( m_pCur );
        value = (value << m_Offset) | (m_pCur[4] >> (8 - m_Offset));
        m_pCur += 4;
        return value;
    }
    
    // check specified count of bits, requires bitCnt > 0 && bitCnt <= 24
    uint32_t peek_bits( uint32_t bitCnt ) const
    {
        assert( bitCnt > 0 && bitCnt <= 24 );
        uint32_t value = BE_READ32( m_pCur );
        return (value << m_Offset) >> (32 - bitCnt);
    }
    // check specified count of bits, requires bitCnt > 0 && bitCnt <= 32
    uint32_t peek_bits_long( uint32_t bitCnt ) const
    {
        assert( bitCnt > 0 && bitCnt <= 32 );
        uint32_t value = BE_READ32( m_pCur );
        value = (value << m_Offset) | (m_pCur[4] >> (8 - m_Offset));
        value >>= (32 - bitCnt);
        return value;
    }
    // check next one bit
    uint8_t peek1() const
    {
        return (m_pCur[0] >> (m_Offset ^ 7)) & 1;
    }
    // check next 8 bits
    uint8_t peek8() const
    {
        uint32_t value = (m_pCur[0] << 8) | m_pCur[1];
        value >>= (8 - m_Offset);
        return (uint8_t)(value & 0xFF);
    }
    // check next 32 bits
    uint32_t peek32() const
    {
        uint32_t value = BE_READ32( m_pCur );
        value = (value << m_Offset) | (m_pCur[4] >> (8 - m_Offset));
        return value;
    }

    // skip specified bits
    void skip_bits( uint32_t bitCnt )
    {
        m_Offset += bitCnt;
        m_pCur += m_Offset >> 3;
        m_Offset &= 7;
    }

    // order 0 unsigned exp golomb code in range [0,254], return 255 if out of range
    uint8_t read_ue8();

    // order 0 unsigned exp golomb code in range [0,65534], return 65535 if out of range
    uint16_t read_ue16();

    // order 0 unsigned exp golomb code, return UINT32_MAX if out of range
    uint32_t read_ue32();

    // order 0 signed exp golomb code in range [-127,127], return -128 if out of range
    int8_t read_se8()
    {
        uint32_t data = this->read_ue8();
        int32_t mask = (int32_t)(data & 1) - 1;
        return (int8_t)(((data >> 1) ^ mask) + 1);
    }

    // order 0 signed exp golomb code in range [-32767, 32767], return -32768 if out of range
    int16_t read_se16()
    {
        uint32_t data = this->read_ue16();
        int32_t mask = (int32_t)(data & 1) - 1;
        return (int16_t)(((data >> 1) ^ mask) + 1);
    }

    // order 0 signed exp golomb code, return INT32_MIN if out of range
    int32_t read_se32()
    {
        uint32_t data = this->read_ue32();
        int32_t mask = (int32_t)(data & 1) - 1;
        return (int32_t)((data >> 1) ^ mask) + 1;
    }

private:
    const uint8_t*  m_pBuf;
    size_t          m_Size;
    const uint8_t*  m_pCur;     // current read position
    uint32_t        m_Offset;   // [0, 7]
};

//======================================================================================================================
// big-endian bits stream writer

class BitsWriter : IrkNocopy
{
public:
    explicit BitsWriter( int bitCapacity );
    BitsWriter() : m_pBuf(nullptr), m_BitCapacity(0), m_BitCnt(0) {}
    ~BitsWriter() { delete[] m_pBuf; }

    // init/reset bits writer, clear existing data
    // NOTE: will free inner buffer if bitCapacity == 0
    void reset( int bitCapacity );

    // reserve capacity in bits, copy existing data to new buffer(if needed)
    void reserve( int bitCapacity );

    // get inner buffer, return NULL if not allocated
    const uint8_t* buffer() const   { return m_pBuf; }

    // current bits count written
    int bits() const                { return m_BitCnt; }

    // current buffer capacity in bits
    int capacity() const            { return m_BitCapacity; }

    // clear existing data
    void clear();

    // write one bit(0/1)
    void write1( uint8_t bit );

    // write least bitCnt bits of value
    void write_bits( uint32_t value, int bitCnt );

    // write order 0 unsigned exp-golomb value, requires value < 0xFFFFFFFF
    void write_ue( uint32_t value );

    // write order 0 signed exp-golomb value,
    void write_se( int32_t value );

    // make next write positon byte aligned
    void make_byte_aligned( uint8_t padding = 0 );

    // append bits
    void append( const uint8_t* data, int bitCnt );

    // special function, take inner buffer, new onwner is responsible for buffer deallocaion
    uint8_t* take_buffer();

private:
    uint8_t*    m_pBuf;  
    int         m_BitCapacity;
    int         m_BitCnt;           // current written bits count
};

// write one bit(0/1)
inline void BitsWriter::write1( uint8_t bit )
{
    assert( bit == 0 || bit == 1 );
    if( m_BitCnt + 1 > m_BitCapacity )
        this->reserve( m_BitCapacity * 3 / 2 );

    int i = m_BitCnt >> 3;
    m_pBuf[i] |= bit << ((m_BitCnt & 7) ^ 7);
    m_BitCnt++;
}

// write least bitCnt bits of value
inline void BitsWriter::write_bits( uint32_t value, int bitCnt )
{
    // check buffer
    assert( bitCnt > 0 && bitCnt <= 32 );
    if( m_BitCnt + bitCnt > m_BitCapacity )
        this->reserve( m_BitCapacity * 3 / 2 );

    int j = 8 - (m_BitCnt & 7);
    uint8_t* dst = m_pBuf + (m_BitCnt >> 3);
    value <<= (32 - bitCnt);
    dst[0] |= (uint8_t)(value >> (32 - j));
    BE_WRITE32( dst + 1, (value << j) );
    m_BitCnt += bitCnt;
}

// make next write positon byte aligned
inline void BitsWriter::make_byte_aligned( uint8_t padding )
{
    if( m_BitCnt & 7 )
    {
        if( padding != 0 && m_pBuf )
        {
            int i = m_BitCnt >> 3;
            int j = 8 - (m_BitCnt & 7);
            m_pBuf[i] |= (1u << j) - 1;
        }
        m_BitCnt = (m_BitCnt + 7) & ~7;
    }
}

// special function, take inner buffer, new onwner is responsible for buffer deallocaion
inline uint8_t* BitsWriter::take_buffer()
{
    uint8_t* pbuf = m_pBuf;
    m_pBuf = nullptr;
    m_BitCapacity = 0;
    m_BitCnt = 0;
    return pbuf;
}

} // namespace irk
#endif
