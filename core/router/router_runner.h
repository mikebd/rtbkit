/** router_runner.h                                                -*- C++ -*-
    Jeremy Barnes, 17 December 2012
    Copyright (c) 2012 Datacratic.  All rights reserved.

    Program to run the router.
*/

#pragma once


#include <boost/program_options/options_description.hpp>
#include "rtbkit/core/router/router.h"
#include "rtbkit/core/banker/slave_banker.h"
#include "soa/service/service_utils.h"

namespace RTBKIT {


/*****************************************************************************/
/* ROUTER RUNNER                                                             */
/*****************************************************************************/

struct RouterRunner {

    RouterRunner();

    ServiceProxyArguments serviceArgs;
    std::vector<std::string> logUris;  ///< TODO: zookeeper

    //std::string routerConfigurationFile;
    std::string exchangeConfigurationFile;
    float lossSeconds;

    bool logAuctions;
    bool logBids;

    float maxBidPrice;

    /* Metric tracing limits per second - slow mode.
       Perhaps higher than normal mode to assist in diagnosing slow mode condition.
       Avoid max = 0 (unlimited) at high QPS. */
    uint16_t maxSlowModeTraceAuctionMetrics;            // default = 1000,  0 = unlimited
    uint16_t maxSlowModeTraceBidMetrics;                // default =  500,  0 = unlimited
    uint16_t minSlowModeTraceAuctionMetrics;            // default =  100
    uint16_t minSlowModeTraceBidMetrics;                // default =   50

    /* Metric tracing limits per second - normal mode.
       Perhaps lower than slow mode for maximum performance and scalability under usual conditions.
       Avoid max = 0 (unlimited) at high QPS. */
    uint16_t maxTraceAuctionMetrics;                    // default =  500,  0 = unlimited
    uint16_t maxTraceBidMetrics;                        // default =  250,  0 = unlimited
    uint16_t minTraceAuctionMetrics;                    // default =   50
    uint16_t minTraceBidMetrics;                        // default =   25

    bool traceAllAuctionMetrics;
    bool traceAllBidMetrics;

    bool traceAuctionMessages;
    bool traceBidMessages;

    void doOptions(int argc, char ** argv,
                   const boost::program_options::options_description & opts
                   = boost::program_options::options_description());

    std::shared_ptr<ServiceProxies> proxies;
    std::shared_ptr<SlaveBanker> banker;
    std::shared_ptr<Router> router;
    Json::Value exchangeConfig;

    void init();

    void start();

    void shutdown();

};

} // namespace RTBKIT

