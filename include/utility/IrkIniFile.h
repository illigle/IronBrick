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

#ifndef _IRONBRICK_INIFILE_H_
#define _IRONBRICK_INIFILE_H_

#include "IrkFlatMap.h"
#include "IrkMemPool.h"

// simple parser and writer for small INI file

namespace irk {

class CFile;

// name=value
struct IniProperty
{
    const char* name;
    const char* value;
};

// [section]
// a=1
// b=2
class IniSection
{
public:
    explicit IniSection( const char* secname ) : m_secName(secname), m_props(8) {}

    const char* section_name() const    { return m_secName; }

    // number of properties in the section
    unsigned int property_count() const { return m_props.size(); }

    // iterate properties by index
    const IniProperty& operator[]( unsigned int idx ) const
    {
        return m_props[idx];
    }

    // get property value by name, return null if not found
    const char* get_property_value( const char* name ) const
    {
        int idx = m_props.indexof( name );
        if( idx >= 0 )
            return m_props[idx].value;
        return nullptr;
    }

private:
    friend class IniParser;
    friend class IniWriter;

    // add property, if already exists overwrite old value
    void add_property( const char* name, const char* value )
    {
        m_props.insert_or_assign( name, IniProperty{name, value} );
    }

    // remove property
    bool remove_property( const char* name )
    {
        return m_props.remove( name );
    }

    const char* m_secName;
    FlatMap<const char*,IniProperty> m_props;
};

class IniParser : IrkNocopy
{
public:
    explicit IniParser( bool checkTailComment=false, bool checkEscaped=false ) : m_sections(8), m_memChunk(1024)
    {
        m_bTailComment = checkTailComment;  // check and remove tailing comment
        m_bUnescape = checkEscaped;         // check and unescape escaped characters
    }
    ~IniParser() { this->clear(); }

    bool parse_file( const char* filepath );
    bool parse_text( const std::string& text );
    bool parse_text( const char* text, int size = -1 );  // if size < 0, text must be null-terminated

#ifdef _WIN32
    bool parse_file( const wchar_t* filepath );
#endif

    // number of sections
    unsigned int section_count() const  { return m_sections.size(); }

    // iterate sections by index
    const IniSection& operator[]( unsigned int idx ) const
    {
        return *m_sections[idx]; 
    }

    // get section by name, return null if not found
    const IniSection* get_section( const char* section ) const
    {
        int idx = m_sections.indexof( section );
        if( idx >= 0 )
            return m_sections[idx];
        return nullptr;
    }

    // get property name from the section, return null if not found
    const char* get_property_value( const char* section, const char* propertyName ) const
    {
        int idx = m_sections.indexof( section );
        if( idx >= 0 )
            return m_sections[idx]->get_property_value( propertyName );
        return nullptr;
    }

private:
    bool parse_file( CFile& );
    bool parse( char* buf, size_t len );
    void clear();
    FlatMap<const char*,IniSection*> m_sections;
    MemChunks   m_memChunk;
    MallocedBuf m_fileBuf;
    bool        m_bTailComment;
    bool        m_bUnescape;
};

class IniWriter : IrkNocopy
{
public:
    explicit IniWriter( bool enableEscape=false ) : m_sections(8), m_memChunk(4096), m_bEscape(enableEscape) {}
    ~IniWriter() { this->clear(); }

    // add property, if already exists overwrite old value 
    void add_property( const char* section, const char* name, const char* value );
    void add_property( const std::string& section, const std::string& name, const std::string& value );

    // remove property, also remove section if it become empty
    bool remove_property( const char* section, const char* name );

    // remove section
    bool remove_section( const char* section );

    // make empty
    void clear();

    // write to file
    bool dump_file( const char* filepath ) const;

    // write to string
    void dump_text( std::string& out ) const;

#ifdef _WIN32
    bool dump_file( const wchar_t* filepath ) const;
#endif

    // number of sections already added
    unsigned int section_count() const  { return m_sections.size(); }

    // iterate existing sections by index
    const IniSection& operator[]( unsigned int idx ) const
    {
        return *m_sections[idx]; 
    }

    // get existing section by name, return null if not found
    const IniSection* get_section( const char* section ) const
    {
        int idx = m_sections.indexof( section );
        if( idx >= 0 )
            return m_sections[idx];
        return nullptr;
    }

    // get existing property name, return null if not found
    const char* get_property_value( const char* section, const char* name ) const
    {
        int idx = m_sections.indexof( section );
        if( idx >= 0 )
            return m_sections[idx]->get_property_value( name );
        return nullptr;
    }
private:
    bool dump_file( CFile& ) const;
    char* dupstr( const char* );
    char* dupstr( const std::string& );
    char* escaped( const char* );
    FlatMap<const char*,IniSection*> m_sections;
    MemChunks m_memChunk;
    bool m_bEscape;
};

}   // namespace irk
#endif
