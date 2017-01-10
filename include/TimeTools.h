// ======================================================================
/*!
 * \file
 * \brief Interface of namespace TimeTools
 */
// ======================================================================

#ifndef TIMETOOLS_H
#define TIMETOOLS_H

#include <NFmiTime.h>
#include <NFmiMetTime.h>
#include <string>

namespace TimeTools
{
const NFmiTime toLocalTime(const NFmiTime& theUtcTime);

const NFmiTime timezone_time(const NFmiTime& theUTCTime, const std::string& theZone);
}

#endif  // TIMETOOLS_H

// ======================================================================
