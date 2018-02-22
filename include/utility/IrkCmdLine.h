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

#ifndef _IRONBRICK_CMDLINE_H_
#define _IRONBRICK_CMDLINE_H_

#include <string>
#include <vector>
#include <assert.h>

namespace irk {

// simple command line parsing utility
class CmdLine
{
public:
    CmdLine()
    {
        m_argv.reserve(8);
        m_argv.resize(1);
    }
    void reset()
    {
        m_argv.resize(1);
        m_argv[0].clear();
    }

    // parse command line, options should be seperated by whitespace or tab
    void parse(const char* cmdline);

    // parse command line passed to main function
    void parse(int argc, char** argv);

    // concatenate exe name and options: x.exe -t -f=123
    std::string to_string() const;

    // 1 + number of options
    unsigned arg_count() const { return (unsigned)m_argv.size(); }

    // number of options
    unsigned opt_count() const { return (unsigned)m_argv.size() - 1; }

    // iterate by index
    // NOTE: [0] is exe path, option begin from index 1
    const std::string& operator[](unsigned idx) const
    {
        assert(idx < m_argv.size());
        return m_argv[idx];
    }
    std::string& operator[](unsigned idx)
    {
        assert(idx < m_argv.size());
        return m_argv[idx];
    }

    // get/set exe path
    const std::string& exepath() const { return m_argv[0]; }
    void set_exepath(const char* name) { m_argv[0].assign(name); }

    // add option to the tail
    void add_opt(const char* opt)
    {
        m_argv.emplace_back(opt);
    }

    // add "optname=optvalue" to the tail
    void add_opt(const char* optname, const char* optval)
    {
        std::string opt(optname);
        opt += '=';
        opt += optval;
        m_argv.push_back(opt);
    }

    // modify option value, if option does NOT exist, add option to the tail
    void set_opt(const char* optname, const char* optval);

    // get option value from "optname=optvalue", return nullptr if not found
    const char* get_optvalue(const char* optname) const;

private:
    std::vector<std::string> m_argv;
};

}   // namespace irk
#endif
