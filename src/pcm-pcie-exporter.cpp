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
#include <memory>
#include <thread>
#include <chrono>

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

    // Create a Prometheus exporter
    prometheus::Exposer exposer{"127.0.0.1:9402"};

    // Create a metrics registry
    auto registry = std::make_shared<prometheus::Registry>();

    // Add the metrics registry to the exposer
    exposer.RegisterCollectable(registry);

    // Create gauge metrics for PCIe bandwidths
    auto& pcie_bandwidth_family = prometheus::BuildGauge()
        .Name("pcie_bandwidth")
        .Help("PCIe bandwidth in bytes per second")
        .Register(*registry);

    auto& read_bw_gauge = pcie_bandwidth_family.Add({{"direction", "read"}});
    auto& write_bw_gauge = pcie_bandwidth_family.Add({{"direction", "write"}});

    // Create the platform
    double delay = 1.0; // Default delay of 1 second
    bool csv = false;
    bool print_bandwidth = true;
    bool print_additional_info = false;
    PCM * m = PCM::getInstance();
    unique_ptr<IPlatform> platform(IPlatform::getPlatform(m, csv, print_bandwidth,
                                    print_additional_info, (uint)(delay * 1000)));

    // Monitoring loop
    while (true) {
        platform->getEvents();

        double read_bw = platform->getReadBw();
        double write_bw = platform->getWriteBw();

        // Update Prometheus gauges
        read_bw_gauge.Set(read_bw);
        write_bw_gauge.Set(write_bw);

        if (print_bandwidth) {
            cout << "PCIe Read Bandwidth: " << read_bw << " B/s, ";
            cout << "PCIe Write Bandwidth: " << write_bw << " B/s" << endl;
        }

        // reset the counters
        platform->cleanup();

        // Wait for the specified delay (1s)
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    exit(EXIT_SUCCESS);
}
