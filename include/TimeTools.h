// ======================================================================
/*!
 * \file
 * \brief Interface of namespace TimeTools
 */
// ======================================================================

#ifndef TIMETOOLS_H
#define TIMETOOLS_H

#include <newbase/NFmiTime.h>
#include <newbase/NFmiMetTime.h>
#include <string>

namespace TimeTools
{
const NFmiTime toLocalTime(const NFmiTime& theUtcTime);

const NFmiTime timezone_time(const NFmiTime& theUTCTime, const std::string& theZone);
}

#endif  // TIMETOOLS_H

// ======================================================================
