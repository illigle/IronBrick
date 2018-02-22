/*
* This Source Code Form is subject to the terms of the Mozilla Public License Version 2.0.
* If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.

* Covered Software is provided on an "as is" basis,
* without warranty of any kind, either expressed, implied, or statutory,
* that the Covered Software is free of defects, merchantable,
* fit for a particular purpose or non-infringing.

* Copyright (c) Wei Dongliang <illigle@163.com>.
*/

#include <string.h>
#include "IrkCmdLine.h"

namespace irk {

// parse command line, options should be seperated by whitespace or tab
void CmdLine::parse(const char* cmdline)
{
    static const char* kWS = " \t";
    m_argv.resize(1);

    // get exename
    const char* ptr = cmdline + ::strspn(cmdline, kWS);
    size_t idx = ::strcspn(ptr, kWS);
    m_argv[0].assign(ptr, idx);

    // parse options
    for (ptr += idx; *ptr; ptr += idx)
    {
        ptr += ::strspn(ptr, kWS);
        idx = ::strcspn(ptr, kWS);
        if (idx == 0)
            break;

        m_argv.emplace_back(ptr, idx);
    }
}

// parse command line passed to main function
void CmdLine::parse(int argc, char** argv)
{
    assert(argc > 0);
    m_argv.resize(1);
    m_argv[0].assign(argv[0]);

    for (int i = 1; i < argc; i++)
        m_argv.emplace_back(argv[i]);
}

// concatenate exe name and options: x.exe -t -f=123
std::string CmdLine::to_string() const
{
    assert(m_argv.size() > 0);
    std::string cmdline(m_argv[0]);

    // concatenate options
    for (size_t i = 1; i < m_argv.size(); i++)
    {
        cmdline += ' ';
        cmdline += m_argv[i];
    }
    return cmdline;
}

// find option value, return nullptr if not found
const char* CmdLine::get_optvalue(const char* optname) const
{
    for (size_t i = 1; i < m_argv.size(); i++)
    {
        size_t pos = m_argv[i].find('=');
        if (pos != std::string::npos && m_argv[i].compare(0, pos, optname) == 0)
            return m_argv[i].c_str() + pos + 1;
    }
    return nullptr;
}

// set option value, if option does NOT exist, add option to the tail
void CmdLine::set_opt(const char* optname, const char* optval)
{
    for (size_t i = 1; i < m_argv.size(); i++)
    {
        size_t pos = m_argv[i].find('=');
        if (pos != std::string::npos && m_argv[i].compare(0, pos, optname) == 0)
        {
            m_argv[i].replace(pos + 1, std::string::npos, optval);
            return;
        }
    }
    this->add_opt(optname, optval);
}

}   // namespace irk
