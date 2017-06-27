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

#include "IrkIniFile.h"
#include "IrkCFile.h"

namespace irk {

// unescape ini escaped characters
static void ini_unescape( char* str )
{
    char *s = str, *t = str;
    while( *s != 0 )
    {
        if( *s == '\\' )
        {
            if( (s[1] == ';') || (s[1] == '#') || (s[1] == '=') || (s[1] == ':') || (s[1] == '\\') )
            {
                *t++ = s[1];
                s += 2;
            }
            else if( s[1] == 't' )
            {
                *t++ = '\t';
                s += 2;
            }
            else if( s[1] == 'r' )
            {
                *t++ = '\r';
                s += 2;
            }
            else if( s[1] == 'n' )
            {
                *t++ = '\n';
                s += 2;
            }
            else
            {
                *t++ = *s++;
            }
        }
        else
        {
            *t++ = *s++;
        }
    }
    *t = 0;
}

bool IniParser::parse_file( const char* filepath )
{
    CFile file( filepath, "r" );
    return this->parse_file( file );
}

#ifdef _WIN32

bool IniParser::parse_file( const wchar_t* filepath )
{
    CFile file( filepath, L"r" );
    return this->parse_file( file );
}

#endif

bool IniParser::parse_file( CFile& file )
{
    if( m_sections.size() > 0 ) // clear old data if exists
        this->clear();

    if( !file )
        return false;

    int64_t fsize = file.file_size();
    if( fsize < 0 || fsize > 1024 * 1024 * 64 ) // only for small ini file
        return false;
    uint8_t* buf = (uint8_t*)m_fileBuf.alloc( fsize + 4 );
    size_t len = ::fread( buf, 1, fsize, file );
    if( ::ferror( file ) )
        return false;
    file.close();

    // check BOM, has padding, so always safe
    if( buf[0]==0xEF && buf[1]==0xBB && buf[2]==0xBF )  // UTF-8
    {
        buf += 3;      // skip BOM
        len -= 3;
    }
    else if( (buf[0]==0xFF && buf[1]==0xFE) || (buf[0]==0xFE && buf[1]==0xFF) ) // UTF-16
    {
        return false;
    }

    if( !this->parse( (char*)buf, len ) )
    {
        this->clear();
        return false;
    }
    return true;
}

bool IniParser::parse_text( const std::string& text )
{
    if( m_sections.size() > 0 ) // clear old data if exists
        this->clear();

    size_t len = text.length();
    char* buf = (char*)m_fileBuf.alloc( len + 4 );
    memcpy( buf, text.data(), len );

    if( !this->parse( buf, len ) )
    {
        this->clear();
        return false;
    }
    return true;
}

bool IniParser::parse_text( const char* text, int size )
{
    if( m_sections.size() > 0 ) // clear old data if exists
        this->clear();

    size_t len = (size >= 0) ? size : strlen( text );
    char* buf = (char*)m_fileBuf.alloc( len + 4 );
    memcpy( buf, text, len );
    
    if( !this->parse( buf, len ) )
    {
        this->clear();
        return false;
    }
    return true;
}

void IniParser::clear()
{
    for( unsigned i = 0; i < m_sections.size(); i++ )
    {
        irk_delete( m_memChunk, m_sections[i] );
    }
    m_sections.clear();
    m_memChunk.clear();
    m_fileBuf.rebind( nullptr );
}

bool IniParser::parse( char* buf, size_t len )
{
    // add delimit to make parsing easier, has 4 bytes padding, alwarys safe
    buf[len] = '\n';
    buf[len+1] = 0;

    IniSection* curSec = nullptr;
    char *s1 = buf, *s2 = nullptr, *x1 = nullptr;
    while( *s1 != 0 )
    {
        for( ; IS_SPACE(*s1); s1++ )    // skip space and empty line
        {}
        if( *s1 == 0 )
            break;

        if( *s1 == '[' )    // new section
        {
            // find ']'
            for( s2 = ++s1; (*s2 != ']' && *s2 != '\n'); ++s2 )
            {}
            if( *s2 != ']' || s2 == s1 )
                return false;

            *s2 = 0;
            curSec = irk_new<IniSection>( m_memChunk, s1 );
            m_sections.insert_or_assign( s1, curSec );

            // goto EOL
            for( ++s2; *s2 != '\n'; ++s2 )
            {
                if( !IS_SPACE(*s2) && !m_bTailComment )
                    return false;
            }
            s1 = s2 + 1;
        }
        else if( *s1 == ';' || *s1 == '#' )     // comments, skip this line
        {
            for( ++s1; *s1 != '\n'; ++s1 )
            {}
            s1++;
        }
        else    // new property
        {
            if( !curSec )
                return false;

            // find '='
            for( x1 = s1 + 1; (*x1 != '=' && *x1 != '\n'); ++x1 )
            {}
            if( *x1 != '=' )
                return false;
            *x1 = 0;    // convert '=' to '\0'

            // remove property name's tailing whitespace(if exists)
            for( char* c = x1 - 1; (*c == ' ' || *c == '\t'); --c )
                *c = 0;

            // skip value's leading whitespace(if exists)
            for( ++x1; (*x1 == ' ' || *x1 == '\t'); ++x1 )
            {}
            s2 = x1;    // property value

            // find EOL or tailing comment
            for( ; *x1 != '\n'; ++x1 )
            {
                if( m_bTailComment && (*x1 == ';' || *x1 == '#') && *(x1-1) != '\\' )   // tailing comment
                    break;
            }
            const bool eol = (*x1 == '\n');
            *x1 = 0;

            // removing property value's tailing whitespace(if exists)
            for( char* c = x1 - 1; IS_SPACE(*c); --c )
                *c = 0;

            // add property
            if( m_bUnescape )
                ini_unescape( s2 );
            curSec->add_property( s1, s2 );

            // goto next line
            if( !eol )
            {
                for( ++x1; *x1 != '\n'; ++x1 )
                {}
            }
            s1 = x1 + 1;
        }
    }

    return true;
}

//======================================================================================================================

char* IniWriter::dupstr( const char* cstr )
{
    size_t len = ::strlen( cstr );
    char* buf = (char*)m_memChunk.alloc( len + 1, 4 );
    memcpy( buf, cstr, len );
    buf[len] = 0;
    return buf;
}

char* IniWriter::dupstr( const std::string& str )
{
    size_t len = str.length();
    char* buf = (char*)m_memChunk.alloc( len + 1, 4 );
    memcpy( buf, str.data(), len );
    buf[len] = 0;
    return buf;
}

char* IniWriter::escaped( const char* cstr )
{
    size_t len = 0;
    bool needEscape = false;
    for( const char* s = cstr; *s != 0; s++ )
    {
        len++;
        if( (*s == ';') || (*s == '#') || (*s == '=') || (*s == ':') ||
            (*s == '\\') || (*s == '\t') || (*s == '\r') || (*s == '\n') )
        {
            len++;
            needEscape = true;
        }
    }

    char* buf = (char*)m_memChunk.alloc( len + 1, 4 );

    if( !needEscape )   // no character needs escape
    {
        memcpy( buf, cstr, len );
        buf[len] = 0;
    }
    else
    {
        char* t = buf;
        for( const char* s = cstr; *s != 0; s++ )
        {
            if( *s == '\t' )
            {
                *t++ = '\\';
                *t++ = 't';
            }
            else if( *s == '\r' )
            {
                *t++ = '\\';
                *t++ = 'r';
            }
            else if( *s == '\n' )
            {
                *t++ = '\\';
                *t++ = 'n';
            }
            else
            {
                if( (*s == ';') || (*s == '#') || (*s == '=') || (*s == ':') || (*s == '\\') )
                    *t++ = '\\';
                *t++ = *s;
            }
        }
        *t = 0;
    }
    
    return buf;
}

void IniWriter::add_property( const char* section, const char* name, const char* value )
{
    // NOTE: external string must be duplicated in memory chunk
    // because IniSection and IniProperty only store pointers!

    IniSection* psec = nullptr;
    if( !m_sections.get( section, &psec ) )
    {
        const char* secName = this->dupstr( section );
        psec = irk_new<IniSection>( m_memChunk, secName );
        m_sections.insert_or_assign( secName, psec );
    }

    assert( psec );
    const char* propName = this->dupstr(name);
    const char* propValue = m_bEscape ? this->escaped(value) : this->dupstr(value);
    psec->add_property( propName, propValue );
}

void IniWriter::add_property( const std::string& section, const std::string& name, const std::string& value )
{
    // NOTE: external string must be duplicated in memory chunk
    // because IniSection and IniProperty only store pointers!

    IniSection* psec = nullptr;
    if( !m_sections.get( section.c_str(), &psec ) )
    {
        const char* secName = this->dupstr( section );
        psec = irk_new<IniSection>( m_memChunk, secName );
        m_sections.insert_or_assign( secName, psec );
    }

    assert( psec );
    const char* propName = this->dupstr(name);
    const char* propValue = m_bEscape ? this->escaped(value.c_str()) : this->dupstr(value);
    psec->add_property( propName, propValue );
}

bool IniWriter::remove_property( const char* section, const char* name )
{
    int idx = m_sections.indexof( section );
    if( idx >= 0 )
    {
        IniSection* psec = m_sections[idx];
        if( psec->remove_property( name ) )
        {
            if( psec->property_count() == 0 )   // no more property in this section
            {
                irk_delete( m_memChunk, psec );
                m_sections.erase( idx );
            }
            return true;
        }
    }
    return false;
}

bool IniWriter::remove_section( const char* section )
{
    int idx = m_sections.indexof( section );
    if( idx >= 0 )
    {
        irk_delete( m_memChunk, m_sections[idx] );
        m_sections.erase( idx );
        return true;
    }
    return false;
}

void IniWriter::clear()
{
    for( unsigned i = 0; i < m_sections.size(); i++ )
    {
        irk_delete( m_memChunk, m_sections[i] );
    }
    m_sections.clear();
    m_memChunk.clear();
}

bool IniWriter::dump_file( CFile& file ) const
{
    if( !file )
        return false;

    for( unsigned i = 0; i < m_sections.size(); i++ )
    {
        const IniSection& sec = *m_sections[i];
        fprintf( file, "[%s]\n", sec.section_name() );

        for( unsigned k = 0; k < sec.property_count(); k++ )
            fprintf( file, "%s=%s\n", sec[k].name, sec[k].value );
    }
    return true;
}

bool IniWriter::dump_file( const char* filepath ) const
{
    CFile file( filepath, "w" );
    return this->dump_file( file );
}

#ifdef _WIN32

bool IniWriter::dump_file( const wchar_t* filepath ) const
{
    CFile file( filepath, L"w" );
    return this->dump_file( file );
}

#endif

void IniWriter::dump_text( std::string& out ) const
{
    out.clear();

    for( unsigned i = 0; i < m_sections.size(); i++ )
    {
        const IniSection& sec = *m_sections[i];
        out += "[";
        out += sec.section_name();
        out += "]\n";

        for( unsigned k = 0; k < sec.property_count(); k++ )
        {
            out += sec[k].name;
            out += '=';
            out += sec[k].value;
            out += '\n';
        }
    }
}

} // namespace irk
