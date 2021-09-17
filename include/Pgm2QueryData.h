// ======================================================================
/*!
 * \file
 * \brief Interface of the Pgm2QueryData namespace
 */
// ======================================================================

#ifndef FMI_RADCONTOUR_PGM2QUERYDATA_H
#define FMI_RADCONTOUR_PGM2QUERYDATA_H

#include <string>

class NFmiQueryData;

namespace FMI
{
namespace RadContour
{
struct PgmReadOptions
{
  bool force = false;
  bool verbose = false;
  int agelimit = -1;
  int producer_number = 1014;
  int maxtimesteps = 0;            // all
  std::string ellipsoid = "intl";  // See 'cs2cs -le'
  std::string producer_name = "NRD";
#ifdef UNIX
  std::string tmpdir = "/tmp";
#else
  std::string tmpdir = "C:\\tmp";
#endif
  std::string indata;
  std::string outdata;
};

NFmiQueryData *Pgm2QueryData(const std::string &theFileName,
                             const PgmReadOptions &theOptions,
                             std::ostream &theReportStream);

}  // namespace RadContour
}  // namespace FMI

#endif  // FMI_RADCONTOUR_PGM2QUERYDATA_H

// ======================================================================
