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

// int main(int argc, char * argv[])
// {
//     if(print_version(argc, argv))
//         exit(EXIT_SUCCESS);

//     double delay = -1.0;
//     bool csv = false;
//     bool print_bandwidth = true; // true default
// 	bool print_additional_info = false;

//     PCM * m = PCM::getInstance();

//     // Delay in milliseconds
//     unique_ptr<IPlatform> platform(IPlatform::getPlatform(m, csv, print_bandwidth,
//                                     print_additional_info, (uint)(delay * 1000)));

//     for(uint i=0; i < NUM_SAMPLES; i++)
//         platform->getEvents();

//     cout << "PCIe Read Bandwidth: " << platform->getReadBw() << " B/s, ";
//     cout << "PCIe Write Bandwidth: " << platform->getWriteBw() << " B/s" << endl;

//     exit(EXIT_SUCCESS);
// }

int main() {
  using namespace prometheus;

  // create an http server running on port 8080
  Exposer exposer{"127.0.0.1:8080"};

  // create a metrics registry
  // @note it's the users responsibility to keep the object alive
  auto registry = std::make_shared<Registry>();

  // add a new counter family to the registry (families combine values with the
  // same name, but distinct label dimensions)
  //
  // @note please follow the metric-naming best-practices:
  // https://prometheus.io/docs/practices/naming/
  auto& packet_counter = BuildCounter()
                             .Name("observed_packets_total")
                             .Help("Number of observed packets")
                             .Register(*registry);

  // add and remember dimensional data, incrementing those is very cheap
  auto& tcp_rx_counter =
      packet_counter.Add({{"protocol", "tcp"}, {"direction", "rx"}});
  auto& tcp_tx_counter =
      packet_counter.Add({{"protocol", "tcp"}, {"direction", "tx"}});
  auto& udp_rx_counter =
      packet_counter.Add({{"protocol", "udp"}, {"direction", "rx"}});
  auto& udp_tx_counter =
      packet_counter.Add({{"protocol", "udp"}, {"direction", "tx"}});

  // add a counter whose dimensional data is not known at compile time
  // nevertheless dimensional values should only occur in low cardinality:
  // https://prometheus.io/docs/practices/naming/#labels
  auto& http_requests_counter = BuildCounter()
                                    .Name("http_requests_total")
                                    .Help("Number of HTTP requests")
                                    .Register(*registry);

  // ask the exposer to scrape the registry on incoming HTTP requests
  exposer.RegisterCollectable(registry);

  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const auto random_value = std::rand();

    if (random_value & 1) tcp_rx_counter.Increment();
    if (random_value & 2) tcp_tx_counter.Increment();
    if (random_value & 4) udp_rx_counter.Increment();
    if (random_value & 8) udp_tx_counter.Increment();

    const std::array<std::string, 4> methods = {"GET", "PUT", "POST", "HEAD"};
    auto method = methods.at(random_value % methods.size());
    // dynamically calling Family<T>.Add() works but is slow and should be
    // avoided
    http_requests_counter.Add({{"method", method}}).Increment();
  }
  return 0;
}
