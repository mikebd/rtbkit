/* router_runner.cc
   Jeremy Barnes, 13 December 2012
   Copyright (c) 2012 Datacratic.  All rights reserved.

   Tool to run the router.
*/

#include "router_runner.h"

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread/thread.hpp>

#include "rtbkit/core/router/router.h"
#include "rtbkit/core/banker/slave_banker.h"
#include "jml/arch/timers.h"
#include "jml/utils/file_functions.h"

using namespace std;
using namespace ML;
using namespace Datacratic;
using namespace RTBKIT;

static inline Json::Value loadJsonFromFile(const std::string & filename)
{
    ML::File_Read_Buffer buf(filename);
    return Json::parse(std::string(buf.start(), buf.end()));
}


/*****************************************************************************/
/* ROUTER RUNNER                                                             */
/*****************************************************************************/


RouterRunner::
RouterRunner() :
    exchangeConfigurationFile("examples/router-config.json"),
    checkConfig(false),
    debug(false),
    lossSeconds(15.0),
    logAuctions(false),
    logBids(false),
    maxBidPrice(200),
    // Trace Metrics (Graphite):
    slowModeTraceSettingsAuctionMetrics{
        Router::DefaultMaxSlowModeTraceAuctionMetrics,
        Router::DefaultMinSlowModeTraceAuctionMetrics,
        Router::DefaultModSlowModeTraceAuctionMetrics},
    slowModeTraceSettingsBidMetrics{
        Router::DefaultMaxSlowModeTraceBidMetrics,
        Router::DefaultMinSlowModeTraceBidMetrics,
        Router::DefaultModSlowModeTraceBidMetrics},
    traceSettingsAuctionMetrics{
        Router::DefaultMaxTraceAuctionMetrics,
        Router::DefaultMinTraceAuctionMetrics,
        Router::DefaultModTraceAuctionMetrics},
    traceSettingsBidMetrics{
        Router::DefaultMaxTraceBidMetrics,
        Router::DefaultMinTraceBidMetrics,
        Router::DefaultModTraceBidMetrics},
    // Trace Messages:
    slowModeTraceSettingsAuctionMessages{
        Router::DefaultMaxSlowModeTraceAuctionMessages,
        Router::DefaultMinSlowModeTraceAuctionMessages,
        Router::DefaultModSlowModeTraceAuctionMessages},
    slowModeTraceSettingsBidMessages{
        Router::DefaultMaxSlowModeTraceBidMessages,
        Router::DefaultMinSlowModeTraceBidMessages,
        Router::DefaultModSlowModeTraceBidMessages},
    traceSettingsAuctionMessages{
        Router::DefaultMaxTraceAuctionMessages,
        Router::DefaultMinTraceAuctionMessages,
        Router::DefaultModTraceAuctionMessages},
    traceSettingsBidMessages{
        Router::DefaultMaxTraceBidMessages,
        Router::DefaultMinTraceBidMessages,
        Router::DefaultModTraceBidMessages}
{
}

void
RouterRunner::
doOptions(int argc, char ** argv,
          const boost::program_options::options_description & opts)
{
    using namespace boost::program_options;

    options_description router_options("Router options");
    router_options.add_options()
        ("check-config", value<bool>(&checkConfig)->zero_tokens(),
         "trace and validate configuration, exit without initializing or starting")
        ("debug", value<bool>(&debug)->zero_tokens(),
         "enable debug mechanisms, not recommended at high QPS")
        ("loss-seconds,l", value<float>(&lossSeconds),
         "number of seconds after which a loss is assumed")
        ("log-uri", value<vector<string> >(&logUris),
         "URI to publish logs to")
        ("exchange-configuration,x", value<string>(&exchangeConfigurationFile),
         "configuration file with exchange data")
        ("log-auctions", value<bool>(&logAuctions)->zero_tokens(),
         "log auction requests")
        ("log-bids", value<bool>(&logBids)->zero_tokens(),
         "log bid responses")
        ("max-bid-price", value(&maxBidPrice),
         "maximum bid price accepted by router")
        // Trace Metrics (Graphite):
            // Slow - Auction:
            ("min-slow-trace-auction-metrics", value<uint16_t>(&slowModeTraceSettingsAuctionMetrics.min),
             "minimum auction metrics to trace per second in slow mode")
            ("max-slow-trace-auction-metrics", value<uint16_t>(&slowModeTraceSettingsAuctionMetrics.max),
             "maximum auction metrics to trace per second in slow mode")
            ("mod-slow-trace-auction-metrics", value<uint16_t>(&slowModeTraceSettingsAuctionMetrics.mod),
             "modulus: Id.hash() % <mod-...> == 0")
            // Slow - Bid:
            ("min-slow-trace-bid-metrics", value<uint16_t>(&slowModeTraceSettingsBidMetrics.min),
             "minimum bids metrics to trace per second in slow mode")
            ("max-slow-trace-bid-metrics", value<uint16_t>(&slowModeTraceSettingsBidMetrics.max),
             "maximum bid metrics to trace per second in slow mode")
            ("mod-slow-trace-bid-metrics", value<uint16_t>(&slowModeTraceSettingsBidMetrics.mod),
             "modulus: Id.hash() % <mod-...> == 0")
            // Normal - Auction:
            ("min-trace-auction-metrics", value<uint16_t>(&traceSettingsAuctionMetrics.min),
             "minimum auction metrics to trace per second")
            ("max-trace-auction-metrics", value<uint16_t>(&traceSettingsAuctionMetrics.max),
             "maximum auction metrics to trace per second")
            ("mod-trace-auction-metrics", value<uint16_t>(&traceSettingsAuctionMetrics.mod),
             "modulus: Id.hash() % <mod-...> == 0")
            // Normal - Bid:
            ("min-trace-bid-metrics", value<uint16_t>(&traceSettingsBidMetrics.min),
             "minimum bids metrics to trace per second")
            ("max-trace-bid-metrics", value<uint16_t>(&traceSettingsBidMetrics.max),
             "maximum bid metrics to trace per second")
            ("mod-trace-bid-metrics", value<uint16_t>(&traceSettingsBidMetrics.mod),
             "modulus: Id.hash() % <mod-...> == 0")
        // Trace Messages:
            // Slow - Auction:
            ("min-slow-trace-auction-messages", value<uint16_t>(&slowModeTraceSettingsAuctionMessages.min),
             "minimum auction messages to trace per second in slow mode")
            ("max-slow-trace-auction-messages", value<uint16_t>(&slowModeTraceSettingsAuctionMessages.max),
             "maximum auction messages to trace per second in slow mode")
            ("mod-slow-trace-auction-messages", value<uint16_t>(&slowModeTraceSettingsAuctionMessages.mod),
             "modulus: Id.hash() % <mod-...> == 0")
            // Slow - Bid:
            ("min-slow-trace-bid-messages", value<uint16_t>(&slowModeTraceSettingsBidMessages.min),
             "minimum bids messages to trace per second in slow mode")
            ("max-slow-trace-bid-messages", value<uint16_t>(&slowModeTraceSettingsBidMessages.max),
             "maximum bid messages to trace per second in slow mode")
            ("mod-slow-trace-bid-messages", value<uint16_t>(&slowModeTraceSettingsBidMessages.mod),
             "modulus: Id.hash() % <mod-...> == 0")
            // Normal - Auction:
            ("min-trace-auction-messages", value<uint16_t>(&traceSettingsAuctionMessages.min),
             "minimum auction messages to trace per second")
            ("max-trace-auction-messages", value<uint16_t>(&traceSettingsAuctionMessages.max),
             "maximum auction messages to trace per second")
            ("mod-trace-auction-messages", value<uint16_t>(&traceSettingsAuctionMessages.mod),
             "modulus: Id.hash() % <mod-...> == 0")
            // Normal - Bid:
            ("min-trace-bid-messages", value<uint16_t>(&traceSettingsBidMessages.min),
             "minimum bids messages to trace per second")
            ("max-trace-bid-messages", value<uint16_t>(&traceSettingsBidMessages.max),
             "maximum bid messages to trace per second")
            ("mod-trace-bid-messages", value<uint16_t>(&traceSettingsBidMessages.mod),
             "modulus: Id.hash() % <mod-...> == 0");

    options_description all_opt = opts;
    all_opt
        .add(serviceArgs.makeProgramOptions())
        .add(router_options);
    all_opt.add_options()
        ("help,h", "print this message");

    variables_map vm;
    store(command_line_parser(argc, argv)
          .options(all_opt)
          //.positional(p)
          .run(),
          vm);
    notify(vm);

    if (vm.count("help")) {
        cerr << all_opt << endl;
        exit(1);
    }
}

bool
RouterRunner::
init()
{
    auto proxies = serviceArgs.makeServiceProxies();
    auto serviceName = serviceArgs.serviceName("router");

    exchangeConfig = loadJsonFromFile(exchangeConfigurationFile);

    router = std::make_shared<Router>(proxies, serviceName, lossSeconds,
                                      true,     // connectPostAuctionLoop
                                      debug,
                                      logAuctions, logBids,
                                      USD_CPM(maxBidPrice),
                                      // Trace Metrics (Graphite):
                                      slowModeTraceSettingsAuctionMetrics,
                                      slowModeTraceSettingsBidMetrics,
                                      traceSettingsAuctionMetrics,
                                      traceSettingsBidMetrics,
                                      // Trace Messages:
                                      slowModeTraceSettingsAuctionMessages,
                                      slowModeTraceSettingsBidMessages,
                                      traceSettingsAuctionMessages,
                                      traceSettingsBidMessages);

    bool initialized = false;

    // Trace and validate configuration
    const bool validConfig = router->checkConfig(Router::TraceConfig::All);

    if (! checkConfig   // Otherwise, trace and validate configuration without
                        // initialization and startup.

        && validConfig) // Abort if the configuration is invalid
    {
        router->init();

        banker = std::make_shared<SlaveBanker>(proxies->zmqContext,
                                               proxies->config,
                                               router->serviceName() + ".slaveBanker");

        router->setBanker(banker);
        router->bindTcp();

        initialized = true;
    }

    return initialized;
}

void
RouterRunner::
start()
{
    banker->start();
    router->start();

    // Start all exchanges
    for (auto & exchange: exchangeConfig)
        router->startExchange(exchange);
}

void
RouterRunner::
shutdown()
{
    router->shutdown();
    banker->shutdown();
}

int main(int argc, char ** argv)
{
    RouterRunner runner;

    runner.doOptions(argc, argv);
    if (runner.init()) {
        runner.start();

        runner.router->forAllExchanges([](std::shared_ptr<ExchangeConnector> const & item) {
            item->enableUntil(Date::positiveInfinity());
        });

        for (;;) {
            ML::sleep(10.0);
        }
    }
}
