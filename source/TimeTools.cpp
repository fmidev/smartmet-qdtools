// ======================================================================
/*!
 * \file
 * \brief Implementation of namespace TimeTools
 */
// ======================================================================

#include "TimeTools.h"
#include <cstdlib>

using namespace std;

namespace TimeTools
{
// ----------------------------------------------------------------------
/*!
 * \brief Convert UTC time to local time using current TZ
 *
 * \param theUtcTime The UTC time
 * \return The local time
 */
// ----------------------------------------------------------------------

const NFmiTime toLocalTime(const NFmiTime &theUtcTime)
{
  // The UTC time
  struct ::tm utc;
  utc.tm_sec = theUtcTime.GetSec();
  utc.tm_min = theUtcTime.GetMin();
  utc.tm_hour = theUtcTime.GetHour();
  utc.tm_mday = theUtcTime.GetDay();
  utc.tm_mon = theUtcTime.GetMonth() - 1;     // tm months start from 0
  utc.tm_year = theUtcTime.GetYear() - 1900;  // tm years start from 1900
  utc.tm_wday = -1;
  utc.tm_yday = -1;
  utc.tm_isdst = -1;

  ::time_t epochtime = NFmiStaticTime::my_timegm(&utc);

  struct ::tm local;
  ::localtime_r(&epochtime, &local);

  // And build a NFmiTime from the result

  NFmiTime out(local.tm_year + 1900,
               local.tm_mon + 1,
               local.tm_mday,
               local.tm_hour,
               local.tm_min,
               local.tm_sec);

  return out;
}

// ----------------------------------------------------------------------
/*!
 * Palauttaa kellonajan halutulla aikavyöhykkeellä
 * Erikoistapaukset:
 *  fin = Europe/elsinki
 *  utc = UTC
 *  local = coordinate based approximation
 *
 */
// ----------------------------------------------------------------------

const NFmiTime timezone_time(const NFmiTime &theUTCTime, const string &theZone)
{
  string zone = theZone;
  if (theZone == "fin")
    zone = "Europe/Helsinki";
  else if (theZone == "utc")
    zone = "UTC";

  static string tzvalue = "TZ=" + zone;
  putenv(const_cast<char *>(tzvalue.c_str()));
  tzset();

  return toLocalTime(theUTCTime);
}
}
