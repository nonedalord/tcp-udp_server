#include <boost/log/core.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#include "Logging.h"

namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;

static boost::shared_ptr<sinks::synchronous_sink<sinks::text_ostream_backend>> g_console_sink;

void InitConsoleLogging(boost::log::formatter& log_format)
{
    g_console_sink = boost::make_shared<sinks::synchronous_sink<sinks::text_ostream_backend>>();
    
    auto backend = g_console_sink->locked_backend();
    backend->add_stream(boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter()));
    backend->auto_flush(true);
    
    g_console_sink->set_formatter(log_format);
    
    g_console_sink->set_filter(
        logging::trivial::severity >= logging::trivial::info
    );
    
    logging::core::get()->add_sink(g_console_sink);
}

void LogHelper::InitLogging(const int rotation_size, const int max_files, const int max_size)
{
    boost::log::formatter log_format = (
        expr::stream 
        << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S")
        << "] [" << logging::trivial::severity 
        << "] [" << expr::attr<std::string>("Channel") 
        << "] " << expr::smessage
    );
    
    InitConsoleLogging(log_format);
    logging::add_common_attributes();
    
    logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);
}

void LogHelper::SetLevel(logging::trivial::severity_level level)
{
    auto filter_expr = logging::trivial::severity >= level;
    logging::core::get()->set_filter(filter_expr);
    
    if (g_console_sink)
    {
        g_console_sink->set_filter(filter_expr);
    }
}