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
  bool force;
  bool verbose;
  int agelimit;
  int producer_number;
  int maxtimesteps;
  std::string producer_name;
  std::string tmpdir;
  std::string indata;
  std::string outdata;

  PgmReadOptions()
      : force(false),
        verbose(false),
        agelimit(-1),
        producer_number(1014),
        maxtimesteps(0)  // = all possible timesteps included
        ,
        producer_name("NRD")
#ifdef UNIX
        ,
        tmpdir("/tmp")
#else  // Windows
        ,
        tmpdir("C:\\tmp")
#endif
        ,
        indata(),
        outdata()
  {
  }
};

NFmiQueryData *Pgm2QueryData(const std::string &theFileName,
                             const PgmReadOptions &theOptions,
                             std::ostream &theReportStream);

}  // namespace RadContour
}  // namespace FMI

#endif  // FMI_RADCONTOUR_PGM2QUERYDATA_H

// ======================================================================
