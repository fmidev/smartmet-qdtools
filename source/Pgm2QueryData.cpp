// ======================================================================
/*!
 * \file
 * \brief Implementation of namespace Pgm2QueryData
 */
// ======================================================================

#include "ProjectionStore.h"
#include "ProjectionParser.h"
#include "DataTransform.h"
#include "Projection.h"
#include "Pgm2QueryData.h"

#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiFastQueryInfo.h>

#include <boost/shared_ptr.hpp>

#include <fstream>

using namespace std;

namespace FMI
{
namespace RadContour
{
class PgmHeaderInfo
{
 public:
  PgmHeaderInfo(void)
      : projections(),
        obstime(),
        fortime(),
        param(),
        partnum(0),
        totalparts(0),
        scale(1.0f),
        base(0.0f),
        level()  // alustamaton level tieto jätetään huomiotta
  {
  }

  ProjectionStore projections;
  std::string obstime;
  std::string fortime;
  std::string param;
  int partnum;
  int totalparts;
  float scale;
  float base;
  NFmiLevel level;
};

static void ReadPgmHeader(std::istream &input,
                          PgmHeaderInfo &thePgmHeaderInfo,
                          const PgmReadOptions &theOptions,
                          std::ostream &theReportStream)
{
  const std::string levelSearchStr("SMET-");
  std::string token;
  std::string line;
  std::string foo;
  while (input.good() && (input >> token))
  {
    if (token == "obstime")
      input >> thePgmHeaderInfo.obstime;
    else if (token == "fortime")
      input >> thePgmHeaderInfo.fortime;
    else if (token == "projection")
      ProjectionParser::parse(input, thePgmHeaderInfo.projections);
    else if (token == "param")
    {
      input >> thePgmHeaderInfo.param;
      thePgmHeaderInfo.scale = DataTransform::multiplier(thePgmHeaderInfo.param);
      thePgmHeaderInfo.base = DataTransform::offset(thePgmHeaderInfo.param);
    }
    else if (token == "multiplier")
      input >> thePgmHeaderInfo.scale;
    else if (token == "offset")
      input >> thePgmHeaderInfo.base;
    else if (token == "part")
      input >> thePgmHeaderInfo.partnum >> thePgmHeaderInfo.totalparts;
    else if (token == "level")
    {  // tästä tulkitaan väliaikaisesti level tieto
      input >> foo;
      try
      {
        // foo:ssa pilkulla erotettuna level tyyppi, arvo ja nimi eli esim.
        // hpa,950,"950"
        std::vector<std::string> splitStrs = NFmiStringTools::Split(foo);
        if (splitStrs.size() != 3)
        {
          if (theOptions.verbose)
            theReportStream << "Warning: Unrecognized level line: '" << token << " " << foo << "'"
                            << endl;
        }
        else
        {
          FmiLevelType levType = kFmiHeight;
          if (splitStrs[0] == "hpa") levType = kFmiPressureLevel;

          std::string levelName = splitStrs[2];
          levelName = NFmiStringTools::Trim(levelName, '"');
          NFmiLevel aLevel(
              levType, levelName, NFmiStringTools::Convert<unsigned long>(splitStrs[1]));
          thePgmHeaderInfo.level = aLevel;
        }
      }
      catch (std::exception &e)
      {
        if (theOptions.verbose)
          theReportStream << "Warning: error in level info: " << e.what() << endl
                          << "In string: " << token << " " << foo << endl;
      }
    }
    // Currently just skip extra params
    else if (token == "radar")
    {
      // Existing examples:
      // radar VAN 1 24.8730 60.2710
      // radar SUR
      // We just assume each radar is on its own line and skip the remaining line
      getline(input, foo);
    }
    else if (token == "producttype" || token == "productname" || token == "metersperpixel_x" ||
             token == "metersperpixel_y" || token == "mindbz" || token == "dbz_threshold" ||
             token == "composite_area" || token == "projection_name" || token == "height_level")
    {
      input >> foo;
    }
    else
    {
      getline(input, line);
      if (theOptions.verbose)
        theReportStream << "Warning: Unrecognized header line: '" << token << line << "'" << endl;
    }
  }
  if (input.bad()) throw std::runtime_error("Header stopped before complete");
}

// ----------------------------------------------------------------------
/*!
 * Convert timestamp string into a NFmiMetTime
 *
 * \param theStamp The time stamp of form YYYYMMHHDDMI in UTC time
 * \return NFmiMetTime
 */
// ----------------------------------------------------------------------

NFmiMetTime str2time(const std::string &theStamp)
{
  short yy = atoi(theStamp.substr(0, 4).c_str());
  short mm = atoi(theStamp.substr(4, 2).c_str());
  short dd = atoi(theStamp.substr(6, 2).c_str());
  short hh = atoi(theStamp.substr(8, 2).c_str());
  short mi = atoi(theStamp.substr(10, 2).c_str());
  NFmiMetTime ret(yy, mm, dd, hh, mi, 0, 1);
  return ret;
}

static NFmiQueryInfo MakeQdInfo(const PgmHeaderInfo &thePgmHeaderInfo,
                                int width,
                                int height,
                                const PgmReadOptions &theOptions,
                                std::ostream &theReportStream)
{
  std::string projname = *thePgmHeaderInfo.projections.names().begin();
  const Projection &proj = thePgmHeaderInfo.projections.projection(projname);
  boost::shared_ptr<NFmiArea> area = proj.area(width, height);

  NFmiMetTime origtime = str2time(thePgmHeaderInfo.obstime);

  NFmiMetTime starttime =
      (thePgmHeaderInfo.fortime.empty() ? origtime : str2time(thePgmHeaderInfo.fortime));
  NFmiMetTime endtime = starttime;

  NFmiEnumConverter converter;
  FmiParameterName paramnum = FmiParameterName(converter.ToEnum(thePgmHeaderInfo.param));
  if (paramnum == kFmiBadParameter)
  {
    if (theOptions.verbose)
      theReportStream << "Parameter name " << thePgmHeaderInfo.param << " is unknown" << endl
                      << "--> Using parameter number " << static_cast<int>(kFmiBadParameter)
                      << endl;
  }

  NFmiParamBag pbag;
  NFmiParam p(paramnum, thePgmHeaderInfo.param);
  p.InterpolationMethod(kLinearly);
  pbag.Add(NFmiDataIdent(p));
  NFmiParamDescriptor pdesc(pbag);

  NFmiGrid tmpgrid(area.get(), width, height);
  NFmiHPlaceDescriptor hdesc(tmpgrid);
  NFmiVPlaceDescriptor vdesc;
  if (!thePgmHeaderInfo.level.IsMissing())
  {
    NFmiLevelBag levBag;
    levBag.AddLevel(thePgmHeaderInfo.level);
    vdesc = NFmiVPlaceDescriptor(levBag);
  }

  NFmiTimeBag tbag(starttime, endtime, 5);
  NFmiTimeDescriptor tdesc(origtime, tbag);
  return NFmiQueryInfo(pdesc, tdesc, hdesc, vdesc);
}

static bool FillQueryData(NFmiQueryData &theData,
                          const PgmHeaderInfo &thePgmHeaderInfo,
                          const std::string &theFileName,
                          const PgmReadOptions &theOptions,
                          std::ostream &theReportStream,
                          int bytes,
                          int valid_size1,
                          int header_end_pos)
{
  NFmiFastQueryInfo info(&theData);
  info.First();
  NFmiGrid grid(info.Area());

  int bytesize = (bytes == valid_size1 ? 1 : 2);
  bool xfirst = true;
  bool littleendian = false;  // pgm is always big endian!!!
  bool issigned = false;
  string skipstring = "";

  double beforemissing = bytes;  // = -1
  float aftermissing = kFloatMissing;

  if (!grid.Init(theFileName,
                 info.GridXNumber(),
                 info.GridYNumber(),
                 bytesize,
                 littleendian,
                 header_end_pos,
                 skipstring,
                 kTopLeft,
                 xfirst,
                 beforemissing,
                 aftermissing,
                 issigned,
                 thePgmHeaderInfo.scale,
                 thePgmHeaderInfo.base))
  {
    if (theOptions.verbose) theReportStream << "Failed to read " << theFileName << endl;
    return false;
  }

  if (!info.Grid2Info(grid))
  {
    if (theOptions.verbose) theReportStream << "Failed to make grid from " << theFileName << endl;
    return false;
  }

  // Finally set the desired producer
  info.SetProducer(NFmiProducer(theOptions.producer_number, theOptions.producer_name));
  return true;
}

// Luetaan annetusta PGM-tiedostosta data ja muutetaan se queryDataksi.
NFmiQueryData *Pgm2QueryData(const std::string &theFileName,
                             const PgmReadOptions &theOptions,
                             std::ostream &theReportStream)
{
  // Skip the file if it has the wrong suffix
  if (NFmiStringTools::Suffix(theFileName) != "pgm")
  {
    if (theOptions.verbose) theReportStream << "Skipping non .pgm file " << theFileName << endl;
    return 0;
  }

  // Establish the output name from the header of the file
  // Note that the file may have been removed due to a cleanup,
  // we do not consider it an error.
  if (!NFmiFileSystem::FileReadable(theFileName))
  {
    if (theOptions.verbose) theReportStream << "Skipping nonexistent " << theFileName << endl;
    return 0;
  }

  // Skip the file if it is too old
  if (theOptions.agelimit > 0)
  {
    std::time_t modtime = NFmiFileSystem::FileModificationTime(theFileName);

    if (::time(NULL) - modtime >= 60 * theOptions.agelimit)
    {
      if (theOptions.verbose)
        theReportStream << "Skipping " << theFileName << " as too old" << endl;
      return 0;
    }
  }

  // Read the header
  std::ifstream infile(theFileName.c_str(), ios::in | ios::binary);
  if (!infile)
  {
    if (theOptions.verbose) theReportStream << "Could not open " << theFileName << endl;
    return 0;
  }

  std::string line;

  // P5
  getline(infile, line);
  if (line != "P5")
  {
    infile.close();
    if (theOptions.verbose) theReportStream << "Skipping non-pgm file " << theFileName << endl;
    return 0;
  }

  // Read all comment lines, strip the comments away on the fly
  std::string header;
  while (infile.peek() == '#')
  {
    infile.get();
    getline(infile, line);
    header += line;
    header += '\n';
  }

  // Extract the information on the data
  std::istringstream input(header.c_str());

  PgmHeaderInfo pgmHeaderInfo;
  try
  {
    ReadPgmHeader(input, pgmHeaderInfo, theOptions, theReportStream);
  }
  catch (const std::runtime_error &e)
  {
    infile.close();
    if (theOptions.verbose)
      theReportStream << "Invalid header in file " << theFileName << endl
                      << " --> " << e.what() << endl;
    return 0;
  }

  // Read the P5 specs
  int width, height, bytes;
  infile >> width >> height >> bytes;
  if (!infile.good())
  {
    infile.close();
    if (theOptions.verbose)
      theReportStream << "Failed to read pgm width, height and bytes from " << theFileName << endl;
    return 0;
  }

  // Skip the rest of the line after the bytesize indicator
  getline(infile, line);

  // Check that width height and bytes seem OK
  if (width <= 0 || height <= 0)
  {
    infile.close();
    if (theOptions.verbose) theReportStream << "Nonnegative size fields in " << theFileName << endl;
    return 0;
  }

  const int valid_size1 = (1 << 8) - 1;
  const int valid_size2 = (1 << 16) - 1;

  if (bytes != valid_size1 && bytes != valid_size2)
  {
    infile.close();
    if (theOptions.verbose)
      theReportStream << "Invalid bytesize " << bytes << " in " << theFileName << endl;
    return 0;
  }

  // Establish the position so that newbase can skip to this
  // current point
  int header_end_pos = static_cast<int>(infile.tellg());
  infile.close();

  // We must have an observation time
  if (pgmHeaderInfo.obstime.empty())
  {
    if (theOptions.verbose)
      theReportStream << "Observation time is missing in " << theFileName << endl;
    return 0;
  }

  // Must have parameter name
  if (pgmHeaderInfo.param.empty())
  {
    if (theOptions.verbose) theReportStream << "Parameter missing from " << theFileName << endl;
    return 0;
  }

  // Establish the projection
  if (pgmHeaderInfo.projections.names().size() == 0)
  {
    theReportStream << "Header does not contain projection in " << theFileName << endl;
    return 0;
  }

  if (pgmHeaderInfo.projections.names().size() > 1)
  {
    theReportStream << "Header contains multiple projections in " << theFileName << endl;
    return 0;
  }

  NFmiQueryInfo tmpinfo = MakeQdInfo(pgmHeaderInfo, width, height, theOptions, theReportStream);

  NFmiQueryData *data = NFmiQueryDataUtil::CreateEmptyData(tmpinfo);
  if (!FillQueryData(*data,
                     pgmHeaderInfo,
                     theFileName,
                     theOptions,
                     theReportStream,
                     bytes,
                     valid_size1,
                     header_end_pos))
  {
    infile.close();
    return 0;
  }
  infile.close();

  return data;
}
}
}  // namespace FMI::RadContour

// ======================================================================
