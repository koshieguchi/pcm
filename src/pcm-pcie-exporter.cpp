// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2009-2022, Intel Corporation
// originally written by Patrick Lu
// redesigned by Roman Sudarikov


/*!     \file pcm-pcie.cpp
  \brief Example of using uncore CBo counters: implements a performance counter monitoring utility for monitoring PCIe bandwidth
  */
#ifdef _MSC_VER
#include <windows.h>
#include "windows/windriver.h"
#else
#include <unistd.h>
#include <signal.h>
#endif
#include <math.h>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <assert.h>
#include "pcm-pcie.h"

// This is for prometheus exporter
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/counter.h>

using namespace std;

IPlatform *IPlatform::getPlatform(PCM *m, bool csv, bool print_bandwidth, bool print_additional_info, uint32 delay)
{
    switch (m->getCPUModel()) {
        case PCM::SRF:
            cout << "Birch" << endl;
            return new BirchStreamPlatform(m, csv, print_bandwidth, print_additional_info, delay);
        case PCM::SPR:
        case PCM::EMR:
            cout << "Eagle" << endl;
            return new EagleStreamPlatform(m, csv, print_bandwidth, print_additional_info, delay);
        case PCM::ICX:
        case PCM::SNOWRIDGE:
            cout << "Whitley" << endl;
            return new WhitleyPlatform(m, csv, print_bandwidth, print_additional_info, delay);
        case PCM::SKX:
            cout << "Purley" << endl;
            return new PurleyPlatform(m, csv, print_bandwidth, print_additional_info, delay);
        case PCM::BDX_DE:
        case PCM::BDX:
        case PCM::KNL:
        case PCM::HASWELLX:
            cout << "Grantley" << endl;
            return new GrantleyPlatform(m, csv, print_bandwidth, print_additional_info, delay);
        case PCM::IVYTOWN:
        case PCM::JAKETOWN:
            cout << "Bromolow" << endl;
            return new BromolowPlatform(m, csv, print_bandwidth, print_additional_info, delay);
        default:
          return NULL;
    }
}

int main(int argc, char * argv[])
{
    if(print_version(argc, argv))
        exit(EXIT_SUCCESS);

    double delay = -1.0;
    bool csv = false;
    bool print_bandwidth = true; // true default
	bool print_additional_info = false;

    PCM * m = PCM::getInstance();

    // Delay in milliseconds
    unique_ptr<IPlatform> platform(IPlatform::getPlatform(m, csv, print_bandwidth,
                                    print_additional_info, (uint)(delay * 1000)));

    for(uint i=0; i < NUM_SAMPLES; i++)
        platform->getEvents();

    cout << "PCIe Read Bandwidth: " << platform->getReadBw() << " B/s, ";
    cout << "PCIe Write Bandwidth: " << platform->getWriteBw() << " B/s" << endl;

    exit(EXIT_SUCCESS);
}
