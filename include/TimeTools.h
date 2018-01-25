// ======================================================================
/*!
 * \file
 * \brief Interface of namespace TimeTools
 */
// ======================================================================

#ifndef TIMETOOLS_H
#define TIMETOOLS_H

#include <newbase/NFmiMetTime.h>
#include <newbase/NFmiTime.h>
#include <string>

namespace TimeTools
{
const NFmiTime toLocalTime(const NFmiTime& theUtcTime);

const NFmiTime timezone_time(const NFmiTime& theUTCTime, const std::string& theZone);
}  // namespace TimeTools

#endif  // TIMETOOLS_H

// ======================================================================
