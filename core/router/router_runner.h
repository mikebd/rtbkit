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
    std::string exchangeConfigurationFile;

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

    /*************************************************************************/
    /* CONFIGURATION                                                         */
    /*************************************************************************/

    float lossSeconds;

    bool logAuctions;
    bool logBids;

    float maxBidPrice;

    /*************************************************************************/
    /* TRACE METRICS (GRAPHITE)                                              */
    /*************************************************************************/

    /* Metric tracing limits per second - slow mode.
       Perhaps higher than normal mode to assist in diagnosing slow mode condition.
       Avoid a large max at high QPS. */
    uint16_t maxSlowModeTraceAuctionMetrics;            // default = 1000
    uint16_t maxSlowModeTraceBidMetrics;                // default =  500
    uint16_t minSlowModeTraceAuctionMetrics;            // default =  100
    uint16_t minSlowModeTraceBidMetrics;                // default =   50

    /* Metric tracing limits per second - normal mode.
       Perhaps lower than slow mode for maximum performance and scalability under usual conditions.
       Avoid a large max at high QPS. */
    uint16_t maxTraceAuctionMetrics;                    // default =  500
    uint16_t maxTraceBidMetrics;                        // default =  250
    uint16_t minTraceAuctionMetrics;                    // default =   50
    uint16_t minTraceBidMetrics;                        // default =   25

    /* Force metric tracing (within max limits) - normal + slow mode.
       TODO: Consider adding separate slow mode controls.
       TODO: Convert to a probability control for randomization,
             replace: auction->id.hash() % 10 == 0 */
    bool traceAllAuctionMetrics;
    bool traceAllBidMetrics;

    /*************************************************************************/
    /* TRACE MESSAGES                                                        */
    /*************************************************************************/

    /* Message tracing limits per second - slow mode.
       Perhaps higher than normal mode to assist in diagnosing slow mode condition.
       Avoid a large max at high QPS. */
    uint16_t maxSlowModeTraceAuctionMessages;           // default =    0
    uint16_t maxSlowModeTraceBidMessages;               // default =    0
    uint16_t minSlowModeTraceAuctionMessages;           // default =    0
    uint16_t minSlowModeTraceBidMessages;               // default =    0

    /* Message tracing limits per second - normal mode.
       Perhaps lower than slow mode for maximum performance and scalability under usual conditions.
       Avoid a large max at high QPS. */
    uint16_t maxTraceAuctionMessages;                   // default =    0
    uint16_t maxTraceBidMessages;                       // default =    0
    uint16_t minTraceAuctionMessages;                   // default =    0
    uint16_t minTraceBidMessages;                       // default =    0

    /* Force message tracing (within max limits) - normal + slow mode.
       TODO: Consider adding separate slow mode controls.
       TODO: Convert to a probability control for randomization,
             replace: auction->id.hash() % 10 == 0 */
    bool traceAllAuctionMessages;
    bool traceAllBidMessages;
};

} // namespace RTBKIT

