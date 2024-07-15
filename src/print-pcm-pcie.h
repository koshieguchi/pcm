// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2020-2022, Intel Corporation
#pragma once
//written by Roman Sudarikov

#include <iostream>
#include "cpucounters.h"
#include <vector>
#include <array>
#include <string>
#include <initializer_list>
#include <algorithm>

#if defined(_MSC_VER)
typedef unsigned int uint;
#endif

using namespace std;
using namespace pcm;
#define NUM_SAMPLES (1)

static uint getIdent (const string &s)
{
    /*
     * We are adding "|  " before and "  " after the event name hence +5 to
     * strlen(eventNames). Rest of the logic is to center the event name.
     */
    uint ident = 5 + (uint)s.size();
    return (3 + ident / 2);
}

class IPlatform
{
    void init();

public:
    IPlatform(PCM *m, bool csv, bool bandwidth, bool verbose);
    // virtual void getEvents() = 0;
    // virtual void printEvents() = 0;
    // static IPlatform *getPlatform(PCM* m, bool csv, bool bandwidth,
                                        // bool verbose, uint32 delay);
    virtual ~IPlatform() { }

protected:
    PCM *m_pcm;
    bool m_csv;
    bool m_bandwidth;
    bool m_verbose;
    uint m_socketCount;

    enum eventFilter {TOTAL, MISS, HIT, fltLast};

    vector<string> filterNames, bwNames;
};

void IPlatform::init()
{
    // print_cpu_details();

    if (m_pcm->isSomeCoreOfflined())
    {
        cerr << "Core offlining is not supported. Program aborted\n";
        exit(EXIT_FAILURE);
    }
}

IPlatform::IPlatform(PCM *m, bool csv, bool bandwidth, bool verbose) :
        m_pcm(m),
        filterNames {"(Total)", "(Miss)", "(Hit)"},
        bwNames {"PCIe Rd (B)", "PCIe Wr (B)"}
{
    m_csv = csv;
    m_bandwidth = bandwidth;
    m_verbose = verbose;
    m_socketCount = m_pcm->getNumSockets();

    init();
}

/*
 * Common API to program, access and represent required Uncore counters.
 * The only difference is event opcodes and the way how bandwidth is calculated.
 */
class LegacyPlatform: public IPlatform
{
    enum {
        before,
        after,
        total
    };
    vector<string> eventNames;
    vector<eventGroup_t> eventGroups;
    uint32 m_delay;
    typedef vector <vector <uint64>> eventCount_t;
    array<eventCount_t, total> eventCount;

    // virtual void printEvents() final;

    // void printBandwidth(uint socket, eventFilter filter);
    // void printSocketScopeEvents(uint socket, eventFilter filter);

public:
    LegacyPlatform(initializer_list<string> events, initializer_list <eventGroup_t> eventCodes,
        PCM *m, bool csv, bool bandwidth, bool verbose, uint32 delay) :
        IPlatform(m, csv, bandwidth, verbose),
            eventNames(events), eventGroups(eventCodes)
    {
        int eventsCount = 0;
        for (auto &group : eventGroups) eventsCount += (int)group.size();

        // Delay for each multiplexing group. Counters will be scaled.
        m_delay = uint32(delay / eventGroups.size() / NUM_SAMPLES);

        eventSample.resize(m_socketCount);
        for (auto &e: eventSample)
            e.resize(eventsCount);

        for (auto &run : eventCount) {
            run.resize(m_socketCount);
            for (auto &events_ : run)
                events_.resize(eventsCount);
        }
    };

protected:
    vector<vector<uint64>> eventSample;
    virtual uint64 getReadBw(uint socket, eventFilter filter) = 0;
    virtual uint64 getWriteBw(uint socket, eventFilter filter) = 0;
    virtual uint64 event(uint socket, eventFilter filter, uint idx) = 0;
};

// void LegacyPlatform::printBandwidth(uint skt, eventFilter filter)
// {
//     // filter -> TOTAL
//     typedef uint64 (LegacyPlatform::*bwFunc_t)(uint, eventFilter);
//     vector<bwFunc_t> bwFunc = {
//         &LegacyPlatform::getReadBw,
//         &LegacyPlatform::getWriteBw,
//     };

//     // if (!m_csv)
//         // for(auto& bw_f : bwFunc) {
//         //     int ident = getIdent(bwNames[&bw_f - bwFunc.data()]);
//         //     cout << setw(ident)
//         //          << unit_format((this->*bw_f)(skt,filter))
//         //          << setw(5 + bwNames[&bw_f - bwFunc.data()].size() - ident)
//         //          << ' ';
//         // }
//     // else
//         for(auto& bw_f : bwFunc)
//             cout << ',' << (this->*bw_f)(skt,filter);
// }

// void LegacyPlatform::printSocketScopeEvent(uint skt, eventFilter filter, uint idx)
// {
//     uint64 value = event(skt, filter, idx);

//     if (m_csv)
//         cout << ',' << value;
//     else
//     {
//         int ident = getIdent(eventNames[idx]);
//         cout << setw(ident)
//              << unit_format(value)
//              << setw(5 + eventNames[idx].size() - ident)
//              << ' ';
//     }
// }

// void LegacyPlatform::printSocketScopeEvents(uint skt, eventFilter filter)
// {
//     if (!m_csv) {
//         int ident = (int)strlen("Skt |") / 2;
//         cout << setw(ident) << skt << setw(ident) << ' ';
//     } else
//         cout << skt;

//     // for(uint idx = 0; idx < eventNames.size(); ++idx)
//         // printSocketScopeEvent(skt, filter, idx);

//     if (m_bandwidth)
//         printBandwidth(skt, filter);

//     // if(m_verbose)
//         // cout << filterNames[filter];

//     cout << "\n";
// }

// void LegacyPlatform::printEvents()
// {
//     for(uint skt =0; skt < m_socketCount; ++skt)
//         if (!m_verbose)
//             printSocketScopeEvents(skt, TOTAL);
//         // else
//             // for (uint flt = TOTAL; flt < fltLast; ++flt)
//                 // printSocketScopeEvents(skt, static_cast<eventFilter>(flt));
// }

//SPR
class EagleStreamPlatform: public LegacyPlatform
{
public:
    EagleStreamPlatform(PCM *m, bool csv, bool bandwidth, bool verbose, uint32 delay) :
        LegacyPlatform( {"PCIRdCur", "ItoM", "ItoMCacheNear", "UCRdF", "WiL", "WCiL", "WCiLF"},
                        {
                            {0xC8F3FE00000435, 0xC8F3FD00000435, 0xCC43FE00000435, 0xCC43FD00000435},
                            {0xCD43FE00000435, 0xCD43FD00000435, 0xC877DE00000135, 0xC87FDE00000135},
                            {0xC86FFE00000135, 0xC867FE00000135,},
                        },
                        m, csv, bandwidth, verbose, delay)
    {
    };

private:
    enum eventIdx {
        PCIRdCur,
        ItoM,
        ItoMCacheNear,
        UCRdF,
        WiL,
        WCiL,
        WCiLF
    };

    enum Events {
            PCIRdCur_miss,
            PCIRdCur_hit,
            ItoM_miss,
            ItoM_hit,
            ItoMCacheNear_miss,
            ItoMCacheNear_hit,
            UCRdF_miss,
            WiL_miss,
            WCiL_miss,
            WCiLF_miss,
            eventLast
    };

    virtual uint64 getReadBw(uint socket, eventFilter filter);
    virtual uint64 getWriteBw(uint socket, eventFilter filter);
    virtual uint64 getReadBw();
    virtual uint64 getWriteBw();
    virtual uint64 event(uint socket, eventFilter filter, uint idx);
};

uint64 EagleStreamPlatform::event(uint socket, eventFilter filter, uint idx)
{
    uint64 event = 0;
    switch (idx)
    {
        case PCIRdCur:
            if(filter == TOTAL)
                event = eventSample[socket][PCIRdCur_miss] +
                        eventSample[socket][PCIRdCur_hit];
                else if (filter == MISS)
                    event = eventSample[socket][PCIRdCur_miss];
                else if (filter == HIT)
                    event = eventSample[socket][PCIRdCur_hit];
            break;
        case ItoM:
            if(filter == TOTAL)
                event = eventSample[socket][ItoM_miss] +
                        eventSample[socket][ItoM_hit];
                else if (filter == MISS)
                    event = eventSample[socket][ItoM_miss];
                else if (filter == HIT)
                    event = eventSample[socket][ItoM_hit];
            break;
        case ItoMCacheNear:
            if(filter == TOTAL)
                event = eventSample[socket][ItoMCacheNear_miss] +
                        eventSample[socket][ItoMCacheNear_hit];
                else if (filter == MISS)
                    event = eventSample[socket][ItoMCacheNear_miss];
                else if (filter == HIT)
                    event = eventSample[socket][ItoMCacheNear_hit];
            break;
        case UCRdF:
                if(filter == TOTAL || filter == MISS)
                    event = eventSample[socket][UCRdF_miss];
            break;
        case WiL:
                if(filter == TOTAL || filter == MISS)
                    event = eventSample[socket][WiL_miss];
            break;
        case WCiL:
                if(filter == TOTAL || filter == MISS)
                    event = eventSample[socket][WCiL_miss];
            break;
        case WCiLF:
                if(filter == TOTAL || filter == MISS)
                    event = eventSample[socket][WCiLF_miss];
            break;
        default:
            break;
    }
    return event;
}

// uint64 EagleStreamPlatform::getReadBw(uint socket, eventFilter filter)
// {
//     uint64 readBw = event(socket, filter, PCIRdCur);
//     return (readBw * 64ULL);
// }

// uint64 EagleStreamPlatform::getWriteBw(uint socket, eventFilter filter)
// {
//     uint64 writeBw = event(socket, filter, ItoM) +
//                      event(socket, filter, ItoMCacheNear);
//     return (writeBw * 64ULL);
// }

uint64 EagleStreamPlatform::getReadBw()
{
    uint64 readBw = 0;
    for (uint socket = 0; socket < m_socketCount; socket++)
        readBw += (event(socket, TOTAL, PCIRdCur));
    return (readBw * 64ULL);
}

uint64 EagleStreamPlatform::getWriteBw()
{
    uint64 writeBw = 0;
    for (uint socket = 0; socket < m_socketCount; socket++)
        writeBw += (event(socket, TOTAL, ItoM) +
                    event(socket, TOTAL, ItoMCacheNear));
    return (writeBw * 64ULL);
}

//ICX
class WhitleyPlatform: public LegacyPlatform
{
public:
    WhitleyPlatform(PCM *m, bool csv, bool bandwidth, bool verbose, uint32 delay) :
        LegacyPlatform( {"PCIRdCur", "ItoM", "ItoMCacheNear", "UCRdF","WiL"},
                        {
                            {0xC8F3FE00000435, 0xC8F3FD00000435, 0xCC43FE00000435, 0xCC43FD00000435},
                            {0xCD43FE00000435, 0xCD43FD00000435, 0xC877DE00000135, 0xC87FDE00000135},
                        },
                        m, csv, bandwidth, verbose, delay)
    {
    };

private:
    enum eventIdx {
        PCIRdCur,
        ItoM,
        ItoMCacheNear,
        UCRdF,
        WiL,
    };

    enum Events {
            PCIRdCur_miss,
            PCIRdCur_hit,
            ItoM_miss,
            ItoM_hit,
            ItoMCacheNear_miss,
            ItoMCacheNear_hit,
            UCRdF_miss,
            WiL_miss,
            eventLast
    };

    virtual uint64 getReadBw(uint socket, eventFilter filter);
    virtual uint64 getWriteBw(uint socket, eventFilter filter);
    virtual uint64 getReadBw();
    virtual uint64 getWriteBw();
    virtual uint64 event(uint socket, eventFilter filter, uint idx);
};

uint64 WhitleyPlatform::event(uint socket, eventFilter filter, uint idx)
{
    uint64 event = 0;
    switch (idx)
    {
        case PCIRdCur:
            if(filter == TOTAL)
                event = eventSample[socket][PCIRdCur_miss] +
                        eventSample[socket][PCIRdCur_hit];
                else if (filter == MISS)
                    event = eventSample[socket][PCIRdCur_miss];
                else if (filter == HIT)
                    event = eventSample[socket][PCIRdCur_hit];
            break;
        case ItoM:
            if(filter == TOTAL)
                event = eventSample[socket][ItoM_miss] +
                        eventSample[socket][ItoM_hit];
                else if (filter == MISS)
                    event = eventSample[socket][ItoM_miss];
                else if (filter == HIT)
                    event = eventSample[socket][ItoM_hit];
            break;
        case ItoMCacheNear:
            if(filter == TOTAL)
                event = eventSample[socket][ItoMCacheNear_miss] +
                        eventSample[socket][ItoMCacheNear_hit];
                else if (filter == MISS)
                    event = eventSample[socket][ItoMCacheNear_miss];
                else if (filter == HIT)
                    event = eventSample[socket][ItoMCacheNear_hit];
            break;
        case UCRdF:
                if(filter == TOTAL || filter == MISS)
                    event = eventSample[socket][UCRdF_miss];
            break;
        case WiL:
                if(filter == TOTAL || filter == MISS)
                    event = eventSample[socket][WiL_miss];
            break;
        default:
            break;
    }
    return event;
}

uint64 WhitleyPlatform::getReadBw()
{
    uint64 readBw = 0;
    for (uint socket = 0; socket < m_socketCount; socket++)
        readBw += (event(socket, TOTAL, PCIRdCur));
    return (readBw * 64ULL);
}

uint64 WhitleyPlatform::getWriteBw()
{
    uint64 writeBw = 0;
    for (uint socket = 0; socket < m_socketCount; socket++)
        writeBw += (event(socket, TOTAL, ItoM) +
                    event(socket, TOTAL, ItoMCacheNear));
    return (writeBw * 64ULL);
}
