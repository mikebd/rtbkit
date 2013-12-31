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
    lossSeconds(15.0),
    logAuctions(false),
    logBids(false),
    maxBidPrice(200),
    maxSlowModeTraceAuctionMetrics(Router::DefaultMaxSlowModeTraceAuctionMetrics),
    maxSlowModeTraceBidMetrics(Router::DefaultMaxSlowModeTraceBidMetrics),
    minSlowModeTraceAuctionMetrics(Router::DefaultMinSlowModeTraceAuctionMetrics),
    minSlowModeTraceBidMetrics(Router::DefaultMinSlowModeTraceBidMetrics),
    maxTraceAuctionMetrics(Router::DefaultMaxTraceAuctionMetrics),
    maxTraceBidMetrics(Router::DefaultMaxTraceBidMetrics),
    minTraceAuctionMetrics(Router::DefaultMinTraceAuctionMetrics),
    minTraceBidMetrics(Router::DefaultMinTraceBidMetrics),
    traceAllAuctionMetrics(false),
    traceAllBidMetrics(false),
    traceAuctionMessages(false),
    traceBidMessages(false)
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
        ("max-slow-mode-trace-auction-metrics", value<uint16_t>(&maxSlowModeTraceAuctionMetrics),
         "maximum auction metrics to trace per second in slow mode")
        ("max-slow-mode-trace-bid-metrics", value<uint16_t>(&maxSlowModeTraceBidMetrics),
         "maximum bid metrics to trace per second in slow mode")
        ("min-slow-mode-trace-auction-metrics", value<uint16_t>(&minSlowModeTraceAuctionMetrics),
         "minimum auction metrics to trace per second in slow mode")
        ("min-slow-mode-trace-bid-metrics", value<uint16_t>(&minSlowModeTraceBidMetrics),
         "minimum bids metrics to trace per second in slow mode")
        ("max-trace-auction-metrics", value<uint16_t>(&maxTraceAuctionMetrics),
         "maximum auction metrics to trace per second")
        ("max-trace-bid-metrics", value<uint16_t>(&maxTraceBidMetrics),
         "maximum bid metrics to trace per second")
        ("min-trace-auction-metrics", value<uint16_t>(&minTraceAuctionMetrics),
         "minimum auction metrics to trace per second")
        ("min-trace-bid-metrics", value<uint16_t>(&minTraceBidMetrics),
         "minimum bids metrics to trace per second")
        ("trace-all-auction-metrics", value<bool>(&traceAllAuctionMetrics),
         "trace metrics for all auctions")
        ("trace-all-bid-metrics", value<bool>(&traceAllBidMetrics),
         "trace metrics for all bids")
        ("trace-auction-messages", value<bool>(&traceAuctionMessages),
         "trace messages for auctions that are tracing metrics")
        ("trace-bid-messages", value<bool>(&traceBidMessages),
         "trace messages for bids that are tracing metrics");

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

void
RouterRunner::
init()
{
    auto proxies = serviceArgs.makeServiceProxies();
    auto serviceName = serviceArgs.serviceName("router");

    exchangeConfig = loadJsonFromFile(exchangeConfigurationFile);

    router = std::make_shared<Router>(proxies, serviceName, lossSeconds,
                                      true, logAuctions, logBids,
                                      USD_CPM(maxBidPrice),
                                      maxSlowModeTraceAuctionMetrics,
                                      maxSlowModeTraceBidMetrics,
                                      minSlowModeTraceAuctionMetrics,
                                      minSlowModeTraceBidMetrics,
                                      maxTraceAuctionMetrics,
                                      maxTraceBidMetrics,
                                      minTraceAuctionMetrics,
                                      minTraceBidMetrics,
                                      traceAllAuctionMetrics,
                                      traceAllBidMetrics,
                                      traceAuctionMessages,
                                      traceBidMessages);
    router->init();

    banker = std::make_shared<SlaveBanker>(proxies->zmqContext,
                                           proxies->config,
                                           router->serviceName() + ".slaveBanker");

    router->setBanker(banker);
    router->bindTcp();
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
    runner.init();
    runner.start();

    runner.router->forAllExchanges([](std::shared_ptr<ExchangeConnector> const & item) {
        item->enableUntil(Date::positiveInfinity());
    });

    for (;;) {
        ML::sleep(10.0);
    }
}
