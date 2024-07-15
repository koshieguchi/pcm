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

class IPlatform
{
    void init();

public:
    IPlatform(PCM *m, bool csv, bool bandwidth, bool verbose);
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

    void printBandwidth()
    {
        std::cout << "Read Bandwidth: " << getReadBw() << " bytes" << std::endl;
        std::cout << "Write Bandwidth: " << getWriteBw() << " bytes" << std::endl;
    }

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
