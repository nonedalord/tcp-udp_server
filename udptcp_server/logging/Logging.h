#pragma once

#include <string>

#include <boost/log/trivial.hpp>
#include <boost/log/sources/channel_logger.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>

#define LOG(logger, level, message) BOOST_LOG_SEV(logger, level) << "<" << __func__ << "> " << message

namespace LogHelper
{
static constexpr auto info = boost::log::trivial::info;
static constexpr auto error = boost::log::trivial::error;
static constexpr auto fatal = boost::log::trivial::fatal;
static constexpr auto debug = boost::log::trivial::debug;
static constexpr auto warning = boost::log::trivial::warning;

void InitLogging(const std::string& log_file_name, const int rotation_size = 10 * 1024 * 1024, const int max_files = 5, const int max_size = 5 * 1024 * 1024);
void SetLevel(boost::log::trivial::severity_level level);
} // namespace LogHelper
