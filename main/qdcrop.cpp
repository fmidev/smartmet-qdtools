// ======================================================================
/*!
 * \file
 * \brief Implementation of qdcrop - a querydata cropper
 */
// ======================================================================
/*!
 * \page page_qdcrop qdcrop
 *
 * The \c qdcrop program allows one to extract a subgrid from an
 * existing gridded querydata. Simultaneously one may extract also
 * a subset of parameters, levels and times.
 *
 * The available options are:
 * <dl>
 * <dt>-h</dt>
 * <dd>
 * Display help on command line options.
 * </dd>
 * <dt>-V</dt>
 * <dd>Preserve querydata version number</dd>
 * <dt>-g [geometry]</dt>
 * <dd>
 * Define the subgrid to be cropped. The cropped area is given in
 * a form similar to the ImageMagick programs, that is \c [X]x[Y]+dx+dy.
 * </dd>
 * <dt>-G [x1,y1,x2,y2]</dt>
 * <dd>
 * Define the minimal subgrid to be cropped by the bottom left
 * and top right longitude and latitude.
 * </dd>
 * <dt>-d [steps]</dt>
 * <dd>
 * Define the subgrid stepsizes. The default is 1x1.
 * </dd>
 * <dt>-P [projection]</dt>
 * <dd>
 * Define the projection to interpolate to as with qdinterpolatearea.
 * </dd>
 * <dt>-p [param1,param2,...]</dt>
 * <dd>
 * Define the parameters to be extracted. Normally all parameters
 * will be extracted. Names and numerical ids are accepted.
 * </dd>
 * <dt>-r [param1,param2,...]</dt>
 * <dd>
 * Define the parameters to be removed.
 * Names and numerical ids are accepted.
 * </dd>
 * <dt>-a [param1,param2,...]</dt>
 * <dd>
 * Define parameters to be added to the data. The data will be
 * be initialized to missing values.  Names and numerical ids are accepted.
 * </dd>
 * <dt>-A [param1,param2,...]</dt>
 * <dd>
 * Define parameters to be kept, but so that the values for all timesteps
 * will be copied from the origintime.
 * </dd>
 * <dt>-n [oldparam1,newid1,newname1,,oldparam2,newid2,newname2,...]</dt>
 * <dd>
 * Rename parameters using either numbers of names.
 * </dd>
 * <dt>-l [level1,level2,...]</dt>
 * <dd>
 * Define the levels to be extracted. Normally all levels will be
 * extracted.
 * <dt>-t [dt1,dt2,dt]</dt>
 * <dd>
 * Define the time interval to be extracted. Normally all original
 * times will be extracted.
 * </dd>
 * <dt>-S [YYYYMMDDHHMI],...</dt>
 * <dd>
 * Define the time stamp(s) to be extracted in UTC time.
 * </dd>
 * <dt>-T [dt1,dt2,dt]</dt>
 * <dd>
 * Define the time interval to be extracted. Normally all original
 * times will be extracted. This differs from -t in the interpretation
 * of the dt parameter, with -t it is local time, with -T it is UTC time
 * </dd>
 * <dt>-z [yyyymmddhhmi]</dt>
 * <dd>
 * Define the reference time in local time to be used instead of origin time.
 * </dd>
 * <dt>-Z [yyyymmddhhmi]</dt>
 * <dd>
 * Define the reference time in UTC time to be used instead of origin time.
 * </dd>
 * <dt>-i [hour]</dt>
 * <dd>Define the hour to be extracted (local time)</dd>
 * <dt>-I [hour]</dt>
 * <dd>Define the hour to be extracted (UTC time)</dd>
 * <dt>-M [minute]</dt>
 * <dd>Define the minute to be extracted</dd>
 * <dt>-w [wmo1,wmo2,...]</dt>
 * <dd>
 * Define the WMO numbers of the stations to extract from point data.
 * The default is to extract all stations.
 * </dd>
 * <dt>-m [limit] | [parameter,limit]</dt>
 * <dd>
 * Maximum allowed amount of missing values for a time step to be included
 * </dd>
 * <dt>-N [name]</dt>
 * <dd>
 * New producer name
 * </dd>
 * <dt>-D [integer]</dt>
 * <dd>
 * New producer number
 * </dd>
 * </dl>
 */
// ======================================================================

#include <boost/algorithm/string.hpp>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiMultiQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiTimeList.h>
#include <ctime>
#include <deque>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;
using namespace boost;

// ----------------------------------------------------------------------
/*!
 * \brief Print usage information
 */
// ----------------------------------------------------------------------

namespace
{
void usage()
{
  cout << "Usage: qdcrop [options] [inputquerydata] [outputquerydata]\n"
       << "\n"
       << "qdcrop extracts a subgrid from the original gridded querydata.\n"
       << "Simultaneously one may extract only a subset of the available\n"
       << "parameters and levels.\n"
       << "\n"
       << "If the output filename is omitted or it is '-', the querydata\n"
       << "will be output to the standard output\n"
       << "\n"
       << "Options:\n"
       << "\n"
       << "-V\n"
       << "\tPreserve querydata version number.\n"
       << "\n"
       << "-R\n"
       << "\tRead all files in the input directory.\n"
       << "\n"
       << "-g <geometry>\n"
       << "\n"
       << "\tThe area to be cropped in a form similar to ImageMagick.\n"
       << "\tFor example, using 100x200+10+20 would extract a grid\n"
       << "\tof size 100x200 starting from grid coordinates 10,20.\n"
       << "\n"
       << "-G <x1,y1,x2,y2>\n"
       << "\tDefine the minimal subgrid to be cropped by the bottom left\n"
       << "\tand top right longitude and latitude.\n"
       << "\n"
       << "-d <XxY>\n"
       << "\tDefine the data stepsize used in cropping. Default value is 1x1\n"
       << "\n"
       << "-P <projection>\n"
       << "\tDefine the projection to interpolate to as with qdinterpolatearea.\n"
       << "\n"
       << "-p <param1,param2,...,paramN>\n"
       << "\n"
       << "\tThe parameters to be extracted as a comma separated list\n"
       << "\tof parameter names, for example Temperature,Precipitation1h or 4,353.\n"
       << "\tBy default all parameters are extracted.\n"
       << "\n"
       << "-r <param1,param2,...,paramN>\n"
       << "\n"
       << "\tThe parameters to be removed as a comma separated list\n"
       << "\n"
       << "-a <param1,param2,...,paramN>\n"
       << "\n"
       << "\tThe parameters to be added to the data with missing values, unless the parameter is "
          "already defined."
       << "\n"
       << "-A <param1,param2,...,paramN>\n"
       << "\n"
       << "\tThe parameters to be copied from the origin time to all timesteps.\n"
       << "\n"
       << "-n <oldparam1,newid1,newname1,oldparam2,newid2,newname2,...>\n"
       << "\n"
       << "\tRename parameters using either numbers of names.\n"
       << "\n"
       << "-l <level1,level2,...,levelN>\n"
       << "\n"
       << "\tThe levels to be extracted as a comma separated list of\n"
       << "\tlevel numbers. By default all levels are extracted.\n"
       << "\n"
       << "-S <YYYYMMDDHHMI>,...\n"
       << "\n"
       << "\tThe time stamp(s) in UTC time to be extracted.\n"
       << "\n"
       << "-t <dt1,dt2,dt>\n"
       << "\n"
       << "\tThe time interval to be extracted as offsets from the origin\n"
       << "\ttime. For example parameters 24,48 would extract times\n"
       << "\tbetween +24 and +48 hours from the origin time. dt1 may\n"
       << "\tbe omitted, in which case it is assumed to be zero.\n"
       << "\tif the last dt parameter is given, it indicates the\n"
       << "\tdesired time step (in local times)\n"
       << "\n"
       << "-T <dt1,dt2,dt>\n"
       << "\n"
       << "\tSame as -t, but dt is used in UTC-time mode\n"
       << "\n"
       << "-z <yyyymmddhhmi>\n"
       << "\n"
       << "\tThe local reference time to be usead instead of origin time\n"
       << "\n"
       << "-Z <yyyymmddhhmi>\n"
       << "\n"
       << "\tThe UTC reference time to be usead instead of origin time\n"
       << "\n"
       << "-i <hour>\n"
       << "\tThe hour to be extracted (local time)\n"
       << "\n"
       << "-I <hour>\n"
       << "\tThe hour to be extracted (UTC time)\n"
       << "\n"
       << "-M <minute>\n"
       << "\tThe minute to be extracted (UTC time)\n"
       << "\n"
       << "-w <wmo1,wmo2-wmo3,...>\n"
       << "\n"
       << "\tThe stations to extract from point data as identified by\n"
       << "\tthe WMO numbers of the stations or ranges of them\n"
       << "-w <wmo1,wmo2-wmo3,...>\n"
       << "\n"
       << "-W <wmo1,wmo2-wmo3,...>\n"
       << "\n"
       << "\tThe stations to remove from point data as identified by\n"
       << "\tthe WMO numbers of the stations or ranges of them\n"
       << "\n"
       << "-m <limit> | <parameter,limit>\n"
       << "\tMaximum allowed amount of missing values for a time step to be included\n"
       << "\n"
       << "-N <name>\n"
       << "\tNew producer name\n"
       << "\n"
       << "-D <number>\n"
       << "\tNew producer number\n\n";
}

// ----------------------------------------------------------------------
/*!
 * \brief Global instance of enum converter
 */
// ----------------------------------------------------------------------

NFmiEnumConverter converter;

// ----------------------------------------------------------------------
/*!
 * \brief Parse a list of station numbers
 *
 * The input is expected to be a comma separated list of station numbers
 * or ranges of station numbers in the form N-M. For example the following
 * is valid input:
 *
 *    1234,2000-2048,1000-1300
 *
 * Negative numbers are not allowed.
 */
// ----------------------------------------------------------------------

set<int> parse_numberlist(const string& theList)
{
  // Validity check

  if (theList.empty())
    throw runtime_error("Cannot parse the list of station numbers: '" + theList + "'");

  // First separate by commas

  list<string> parts;
  boost::algorithm::split(parts, theList, boost::is_any_of(","));

  // Then generate the full list

  set<int> numbers;
  for (const string& str : parts)
  {
    string::size_type pos = str.find('-');
    if (pos == string::npos)
    {
      int num = Fmi::stoi(str);
      numbers.insert(num);
    }
    else
    {
      vector<string> rangeparts;
      boost::algorithm::split(rangeparts, str, boost::is_any_of("-"));
      if (rangeparts.size() != 2)
        throw runtime_error("Invalid number range in '" + theList + "'");
      int num1 = Fmi::stoi(rangeparts[0]);
      int num2 = Fmi::stoi(rangeparts[1]);
      for (int i = num1; i <= num2; i++)
        numbers.insert(i);
    }
  }

  return numbers;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a parameter description
 */
// ----------------------------------------------------------------------

FmiParameterName parse_param(const string& theName)
{
  // Try ascii name

  auto paramnum = FmiParameterName(converter.ToEnum(theName));
  if (paramnum != kFmiBadParameter)
    return paramnum;

  // Try numerical value

  try
  {
    int value = NFmiStringTools::Convert<int>(theName);
    return FmiParameterName(value);
  }
  catch (...)
  {
    throw runtime_error("Parameter '" + theName + "' is not known to newbase");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Build a new NFmiHPlaceDescriptor from the selected stations
 */
// ----------------------------------------------------------------------

NFmiHPlaceDescriptor MakeStationHPlaceDescriptor(NFmiFastQueryInfo& theQ,
                                                 const set<int>& theStations,
                                                 const set<int>& theNoStations)
{
  if (theQ.IsGrid() || (theStations.empty() && theNoStations.empty()))
    return theQ.HPlaceDescriptor();

  NFmiLocationBag bag;

  // Remove only?

  if (theStations.empty())
  {
    for (theQ.ResetLocation(); theQ.NextLocation();)
    {
      int station = theQ.Location()->GetIdent();
      if (theNoStations.find(station) == theNoStations.end())
        bag.AddLocation(*theQ.Location());
    }
  }

  // Keep and possibly remove

  else
  {
    for (int station : theStations)
    {
      if (theNoStations.find(station) == theNoStations.end())
        if (theQ.Location(station))
          bag.AddLocation(*theQ.Location());
    }
  }
  NFmiHPlaceDescriptor hdesc(bag);
  return hdesc;
}

// ----------------------------------------------------------------------
/*!
 * \brief Build a new NFmiHPlaceDescriptor for the given projection
 */
// ----------------------------------------------------------------------

NFmiHPlaceDescriptor MakeProjectionHPlaceDescriptor(const string& theProj)
{
  std::shared_ptr<NFmiArea> area = NFmiAreaFactory::Create(theProj);
  int width = 0;
  int height = 00;
  if (!Fmi::LegacyMode())
  {
    width = static_cast<int>(round(area->Width()));
    height = static_cast<int>(round(area->Height()));
  }
  else
  {
    width = static_cast<int>(round(area->XYArea(area.get()).Width()));
    height = static_cast<int>(round(area->XYArea(area.get()).Height()));
  }
  NFmiGrid grid(area.get(), width, height);
  grid.Area()->SetGridSize(grid.XNumber(), grid.YNumber());
  NFmiHPlaceDescriptor hdesc(grid);
  return hdesc;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the grid stepsizes
 */
// ----------------------------------------------------------------------

void ParseSteps(const string& theSteps, int& dx, int& dy)
{
  char ch = 0;
  istringstream input(theSteps);
  input >> dx >> ch >> dy;
  if (input.bad() || ch != 'x')
    throw runtime_error("Failed to parse stepsizes in '" + theSteps + "'");

  if (dx <= 0 || dy <= 0)
    throw runtime_error("Stepsizes in '" + theSteps + "' must be positive");
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate the size of the grid when every step'th point is selected
 */
// ----------------------------------------------------------------------

int ExpandedSize(int size, int step)
{
  if (step <= 1)
    return size;
  if ((size - 1) % step == 0)
    return (size - 1) / step;
  return (size - 1) / step + 1;
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert the latlon bounding box to grid geometry
 */
// ----------------------------------------------------------------------

void BoundsToGeometry(NFmiFastQueryInfo& theQ,
                      const string& theBounds,
                      int dx,
                      int dy,
                      int& width,
                      int& height,
                      int& xoff,
                      int& yoff)
{
  auto coords = NFmiStringTools::Split<vector<double> >(theBounds);
  if (coords.size() != 4)
    throw runtime_error("-G option parameter must have exactly 4 comma separated numbers");

  const double lon1 = coords[0];
  const double lat1 = coords[1];
  const double lon2 = coords[2];
  const double lat2 = coords[3];

  const NFmiPoint bottomleft(lon1, lat1);
  const NFmiPoint topright(lon2, lat2);

  const NFmiPoint xy1 = theQ.Grid()->LatLonToGrid(bottomleft);
  const NFmiPoint xy2 = theQ.Grid()->LatLonToGrid(topright);

  xoff = static_cast<int>(floor(xy1.X()));
  yoff = static_cast<int>(floor(xy1.Y()));
  width = static_cast<int>(ceil(xy2.X())) - xoff + 1;
  height = static_cast<int>(ceil(xy2.Y())) - yoff + 1;

  // Must expand more if possible.

  width = ExpandedSize(width, dx);
  height = ExpandedSize(height, dy);
}

// ----------------------------------------------------------------------
/*!
 * \brief Build a new NFmiHPlaceDescriptor
 *
 * \param theQ The query info from which to construct
 * \param theStations The stations to extract, or extract all if empty
 * \param theStations The stations to remove
 * \param theGeometry The geomery to parse
 * \param theBounds The bounds to parse
 * \param theSteps The grid stepsizes to parse
 * \param theProj The projection specification
 * \param x1 The parsed X-coordinate offset in cropping
 * \param y1 The parsed Y-coordinate offset in cropping
 * \param dx The parsed X-step
 * \param dy The parsed Y-step
 * \return The new descriptor
 */
// ----------------------------------------------------------------------

NFmiHPlaceDescriptor MakeHPlaceDescriptor(NFmiFastQueryInfo& theQ,
                                          const set<int>& theStations,
                                          const set<int>& theNoStations,
                                          const string& theGeometry,
                                          const string& theBounds,
                                          const string& theSteps,
                                          const string& theProj,
                                          int& x1,
                                          int& y1,
                                          int& dx,
                                          int& dy)
{
  x1 = y1 = 0;
  dx = dy = 1;

  // No subgrid selected or no interpolation? Then return original projection
  // or a subset of the selected stations

  if (theGeometry.empty() && theBounds.empty() && theSteps.empty() && theProj.empty())
    return MakeStationHPlaceDescriptor(theQ, theStations, theNoStations);

  // Interpolate to another area?

  if (!theProj.empty())
    return MakeProjectionHPlaceDescriptor(theProj);

  if (!theQ.Grid())
    throw runtime_error("The input data does not contain a grid!");

  if (!theSteps.empty())
    ParseSteps(theSteps, dx, dy);

  int nx = theQ.Grid()->XNumber();
  int ny = theQ.Grid()->YNumber();

  int width = 0;
  int height = 0;
  int xoff = 0;
  int yoff = 0;

  if (!theGeometry.empty())
  {
    char ch = 0;
    istringstream input(theGeometry);
    input >> width >> ch >> height >> xoff >> yoff;
    if (input.bad() || ch != 'x')
      throw runtime_error("Failed to parse geometry '" + theGeometry + "'");
  }
  else if (!theBounds.empty())
    BoundsToGeometry(theQ, theBounds, dx, dy, width, height, xoff, yoff);
  else
  {
    xoff = yoff = 0;
    width = nx / dx;
    height = ny / dy;
  }

  // Note that the grid is flipped for NFmiRect

  int x2 = 0;
  int y2 = 0;

  if (!Fmi::LegacyMode())
  {
    y2 = (yoff >= 0 ? yoff : ny + yoff - 1);
    x2 = x1 + (width - 1) * dx;

    x1 = (xoff >= 0 ? xoff : nx + xoff - 1);
    y1 = y2 + (height - 1) * dy;
  }
  else
  {
    x1 = (xoff >= 0 ? xoff : nx + xoff - 1);
    y1 = (yoff >= 0 ? yoff : ny + yoff - 1);

    x2 = x1 + (width - 1) * dx;
    y2 = y1 + (height - 1) * dy;
  }

  if (width <= 0 || height <= 0)
    throw runtime_error("Geometry width and height must be greater than zero");

#if 0
  // We allow cropping outside the box, user's responsibility. Note that the grid fill
  // code will use InterpolateValue for cropped grids, which will return kFloatMissing
  // outside the original bbox
  
  if (x1 < 0 || y1 < 0) throw runtime_error("Geometry starts from negative coordinates");
  if (x2 >= nx || y2 >= ny) throw runtime_error("Geometry exceeds grid bounds");
#endif

  NFmiPoint bl(theQ.Grid()->GridToWorldXY(NFmiPoint(x1, y1)));
  NFmiPoint tr(theQ.Grid()->GridToWorldXY(NFmiPoint(x2, y2)));
  std::shared_ptr<NFmiArea> area(
      NFmiArea::CreateFromBBox(theQ.Grid()->Area()->SpatialReference(), bl, tr));

  if (area == nullptr)
    throw runtime_error("Failed to create the new projection");

  NFmiGrid grid(area.get(), width, height);
  grid.Area()->SetGridSize(grid.XNumber(), grid.YNumber());
  NFmiHPlaceDescriptor hdesc(grid);
  return hdesc;
}

// ----------------------------------------------------------------------
/*!
 * \brief Build a new NFmiVPlaceDescriptor
 *
 * \param theQ The query info from which to construct
 * \param theLevels The levels to extract, empty if everything
 * \return The new descriptor
 */
// ----------------------------------------------------------------------

NFmiVPlaceDescriptor MakeVPlaceDescriptor(NFmiFastQueryInfo& theQ, const list<int>& theLevels)
{
  if (theLevels.empty())
    return theQ.VPlaceDescriptor();

  NFmiLevelBag lbag;

  for (theQ.ResetLevel(); theQ.NextLevel();)
  {
    float value = theQ.Level()->LevelValue();
    if (find(theLevels.begin(), theLevels.end(), value) != theLevels.end())
    {
      lbag.AddLevel(*theQ.Level());
    }
  }

  NFmiVPlaceDescriptor vdesc(lbag);
  return vdesc;
}

// ----------------------------------------------------------------------
/*!
 * \brief Build a new NFmiParamDescriptor
 *
 * \param theQ The query info from which to construct
 * \param theParams The parameters to extract
 * \param theDelParams The parameters to delete
 * \param theNewParams The parameters to be added to the data
 * \param theAnalysisParams The analysis parameters to extracted
 * \param TheRenamedParams The parameter rename rules
 * \return The new descriptor
 */
// ----------------------------------------------------------------------

NFmiParamDescriptor MakeParamDescriptor(NFmiFastQueryInfo& theQ,
                                        const vector<string>& theParams,
                                        const vector<string>& theDelParams,
                                        const vector<string>& theNewParams,
                                        const vector<string>& theAnalysisParams)
{
  NFmiParamBag pbag;

  if (!theParams.empty())
  {
    for (const auto& theParam : theParams)
    {
      FmiParameterName paramnum = parse_param(theParam);
      if (!theQ.Param(paramnum))
        throw runtime_error("Source data does not contain parameter " + theParam);

      pbag.Add(theQ.Param());
    }
  }
  else if (!theDelParams.empty())
  {
    pbag = theQ.ParamBag();
    for (const auto& theDelParam : theDelParams)
    {
      FmiParameterName paramnum = parse_param(theDelParam);
      if (!pbag.SetCurrent(paramnum))
        throw runtime_error("Source data does not contain standalone parameter " + theDelParam);
      if (!pbag.Remove())
        throw runtime_error("Failed to remove parameter " + theDelParam + " from querydata");
    }
  }
  else
  {
    pbag = theQ.ParamBag();
  }

  // Keep analysis parameters
  {
    for (const auto& theAnalysisParam : theAnalysisParams)
    {
      FmiParameterName paramnum = parse_param(theAnalysisParam);
      if (!theQ.Param(paramnum))
        throw runtime_error("Source data does not contain parameter " + theAnalysisParam);
      if (!pbag.SetCurrent(paramnum))
        pbag.Add(theQ.Param());
    }
  }

  // Add new parameters

  {
    for (const auto& theNewParam : theNewParams)
    {
      FmiParameterName paramnum = parse_param(theNewParam);
      // If the parameter is already in the data - we simply keep it
      if (!theQ.Param(paramnum))
      {
        NFmiParam param(paramnum, theNewParam);
        pbag.Add(param);
      }
    }
  }

  NFmiParamDescriptor pdesc(pbag);

  return pdesc;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the time interval specification
 */
// ----------------------------------------------------------------------

void ParseTimeInterval(const list<int>& theTimes, int& dt1, int& dt2, int& dt)
{
  if (theTimes.empty())
  {
    dt1 = -24 * 365 * 100;  // 100 years should be enough for any use
    dt2 = 24 * 365 * 100;
    dt = 1;
  }
  else if (theTimes.size() == 1)
  {
    dt1 = 0;
    dt2 = theTimes.front();
    dt = 1;
  }
  else if (theTimes.size() == 2)
  {
    dt1 = theTimes.front();
    dt2 = theTimes.back();
    dt = 1;
  }
  else
  {
    dt1 = theTimes.front();
    dt2 = *(++theTimes.begin());
    dt = theTimes.back();
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test whether the time matches the time step and the desired hours and minutes
 */
// ----------------------------------------------------------------------

bool IsWantedTime(
    const NFmiMetTime& t, int dt, bool utc, int local_hour, int utc_hour, int utc_minute)
{
  if (dt > 1)
  {
    if (utc)
    {
      if (t.GetHour() % dt != 0)
        return false;
    }
    else
    {
      NFmiTime tlocal = t.CorrectLocalTime();
      if (tlocal.GetHour() % dt != 0)
        return false;
    }
  }

  // Accept desired local hours only
  if (local_hour >= 0)
  {
    NFmiTime tlocal = t.CorrectLocalTime();
    if (tlocal.GetHour() != local_hour)
      return false;
  }

  // Accept desired UTC hours only
  if (utc_hour >= 0)
  {
    if (t.GetHour() != utc_hour)
      return false;
  }

  // Accept desired UTC minutes only
  if (utc_minute >= 0)
  {
    if (t.GetMin() != utc_minute)
      return false;
  }

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Build a new NFmiTimeDescriptor
 *
 * \param theQ The query info from which to construct
 * \param theTimes The time interval to extract, empty if everything
 * \param utc True, if UTC handling is desired
 * \param local_hour 0-23 to extract local hour, -1 otherwise
 * \param utc_hour 0-23 to extract utc hour, -1 otherwise
 * \param utc_minute 0-23 to extract utc minute, -1 otherwise
 * \return The new descriptor
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor MakeTimeDescriptor(NFmiFastQueryInfo& theQ,
                                      const vector<NFmiTime>& theCrops,
                                      const list<int>& theTimes,
                                      bool utc,
                                      int local_hour,
                                      int utc_hour,
                                      int utc_minute,
                                      const NFmiTime& theRefTime,
                                      bool use_reftime)
{
  if (theCrops.empty() && theTimes.empty() && local_hour < 0 && utc_hour < 0 && utc_minute < 0)
    return theQ.TimeDescriptor();

  // Handle the special case of -S options

  if (!theCrops.empty())
  {
    NFmiMetTime origintime = theQ.OriginTime();
    NFmiTimeList datatimes;
    for (const auto& theCrop : theCrops)
    {
      if (theQ.Time(theCrop))
        datatimes.Add(new NFmiMetTime(theCrop));
    }
    NFmiTimeDescriptor tdesc(origintime, datatimes);
    return tdesc;
  }

  if (theTimes.size() > 3)
    throw runtime_error("Cannot extract timeinterval containing more than 3 values");

  int dt1 = 0;
  int dt2 = 0;
  int dt = 0;
  ParseTimeInterval(theTimes, dt1, dt2, dt);

  if (dt < 0 || dt > 24 || 24 % dt != 0)
    throw runtime_error("Time step dt in option -t must divide 24");

  NFmiMetTime origintime = theQ.OriginTime();
  NFmiMetTime reftime = origintime;
  if (use_reftime)
    reftime = theRefTime;
  NFmiMetTime starttime = reftime;
  NFmiMetTime endtime = reftime;
  starttime.ChangeByHours(dt1);
  endtime.ChangeByHours(dt2);

  NFmiTimeList datatimes;
  for (theQ.ResetTime(); theQ.NextTime();)
  {
    const NFmiMetTime& t = theQ.ValidTime();
    if (t.IsLessThan(starttime))
      continue;
    if (endtime.IsLessThan(t))
      continue;
    if (!IsWantedTime(t, dt, utc, local_hour, utc_hour, utc_minute))
      continue;

    datatimes.Add(new NFmiMetTime(t));
  }

  if (datatimes.NumberOfItems() == 0)
    throw std::runtime_error(
        "Using the selected time options results in no timesteps being extracted");

  NFmiTimeDescriptor tdesc(origintime, datatimes);
  return tdesc;
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert local time to UTC time using current TZ
 *
 * \param theLocalTime The local time
 * \return The UTC time
 */
// ----------------------------------------------------------------------

NFmiTime toUtcTime(const NFmiTime& theLocalTime)
{
  ::tm tlocal{};
  tlocal.tm_sec = theLocalTime.GetSec();
  tlocal.tm_min = theLocalTime.GetMin();
  tlocal.tm_hour = theLocalTime.GetHour();
  tlocal.tm_mday = theLocalTime.GetDay();
  tlocal.tm_mon = theLocalTime.GetMonth() - 1;
  tlocal.tm_year = theLocalTime.GetYear() - 1900;
  tlocal.tm_wday = -1;
  tlocal.tm_yday = -1;
  tlocal.tm_isdst = -1;

  ::time_t tsec = mktime(&tlocal);

  struct ::tm tutcBuffer{};  // gmtime_r is thread safe
  ::tm* tutc = ::gmtime_r(&tsec, &tutcBuffer);

  NFmiTime out(tutc->tm_year + 1900,
               tutc->tm_mon + 1,
               tutc->tm_mday,
               tutc->tm_hour,
               tutc->tm_min,
               tutc->tm_sec);

  return out;
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the given string into a date
 */
// ----------------------------------------------------------------------

NFmiTime ParseDate(const string& theTime)
{
  if (theTime.size() != 12)
    throw runtime_error("The time stamp '" + theTime + "' must be of the form YYYYMMDDHHMI");
  try
  {
    auto year = NFmiStringTools::Convert<short>(theTime.substr(0, 4));
    auto month = NFmiStringTools::Convert<short>(theTime.substr(4, 2));
    auto day = NFmiStringTools::Convert<short>(theTime.substr(6, 2));
    auto hour = NFmiStringTools::Convert<short>(theTime.substr(8, 2));
    auto minute = NFmiStringTools::Convert<short>(theTime.substr(10, 2));
    return NFmiTime(year, month, day, hour, minute);
  }
  catch (...)
  {
    throw runtime_error("The time stamp '" + theTime + "' must be of the form YYYYMMDDHHMI");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Count all values and the missing values of the current parameter for each time
 */
// ----------------------------------------------------------------------

void CountMissingValues(NFmiFastQueryInfo& theQ,
                        vector<long>& missing_count,
                        vector<long>& total_count)
{
  for (theQ.ResetLocation(); theQ.NextLocation();)
    for (theQ.ResetLevel(); theQ.NextLevel();)
    {
      std::size_t i = 0;
      for (theQ.ResetTime(); theQ.NextTime(); ++i)
      {
        ++total_count[i];
        if (theQ.FloatValue() == kFloatMissing)
          ++missing_count[i];
      }
    }
}

// ----------------------------------------------------------------------
/*!
 * \brief Find time steps with too much missing data
 */
// ----------------------------------------------------------------------

set<NFmiMetTime> FindBadTimes(NFmiFastQueryInfo& theQ, double theLimit, const string& theParam)
{
  // the parameter to be checked, if any
  FmiParameterName paramnum = kFmiBadParameter;
  if (!theParam.empty())
  {
    paramnum = parse_param(theParam);
    if (!theQ.Param(paramnum))
      throw runtime_error("Parameter '" + theParam + "' is not available in the querydata");
  }

  unsigned long ntimes = theQ.SizeTimes();
  vector<long> missing_count(ntimes, 0);
  vector<long> total_count(ntimes, 0);

  // Calculate totals and missing values for all times
  // (This PLZT loop order is optimal for cache efficiency)

  for (theQ.ResetParam(); theQ.NextParam(false);)
  {
    if (paramnum == kFmiBadParameter || paramnum == theQ.Param().GetParamIdent())
    {
      CountMissingValues(theQ, missing_count, total_count);
    }
  }

  // Now determine which times are OK to keep

  set<NFmiMetTime> times;

  std::size_t i = 0;
  for (theQ.ResetTime(); theQ.NextTime(); ++i)
  {
    if (100.0 * missing_count[i] / total_count[i] >= theLimit)
      times.insert(theQ.ValidTime());
  }
  return times;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create a new time descriptor by removing the given time steps
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor MakeTimeDescriptor(NFmiFastQueryInfo& theQ, const set<NFmiMetTime>& theBadTimes)
{
  NFmiMetTime origintime = theQ.OriginTime();
  NFmiTimeList datatimes;
  for (theQ.ResetTime(); theQ.NextTime();)
  {
    if (theBadTimes.find(theQ.ValidTime()) == theBadTimes.end())
      datatimes.Add(new NFmiMetTime(theQ.ValidTime()));
  }
  return NFmiTimeDescriptor(origintime, datatimes);
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove bad times from the querydata and write the result
 */
// ----------------------------------------------------------------------

void RemoveBadTimes(NFmiQueryData& theQD,
                    NFmiFastQueryInfo& theQ,
                    double theVersion,
                    double theMissingLimit,
                    const string& theParam,
                    const string& theOutFile)
{
  set<NFmiMetTime> badtimes = FindBadTimes(theQ, theMissingLimit, theParam);
  if (badtimes.empty())
  {
    theQD.Write(theOutFile);  // just copy as is to output
    return;
  }

  // Filter out the bad timesteps
  NFmiTimeDescriptor tdesc(MakeTimeDescriptor(theQ, badtimes));
  NFmiFastQueryInfo tmpinfo(
      theQ.ParamDescriptor(), tdesc, theQ.HPlaceDescriptor(), theQ.VPlaceDescriptor(), theVersion);
  NFmiQueryData* data2 = NFmiQueryDataUtil::CreateEmptyData(tmpinfo);
  if (data2 == nullptr)
    throw runtime_error("Could not allocate memory for result data");
  NFmiFastQueryInfo dstinfo(data2);

  for (dstinfo.ResetParam(), theQ.ResetParam(); dstinfo.NextParam() && theQ.NextParam();)
    for (dstinfo.ResetLevel(), theQ.ResetLevel(); dstinfo.NextLevel() && theQ.NextLevel();)
      for (dstinfo.ResetTime(); dstinfo.NextTime();)
      {
        theQ.Time(dstinfo.ValidTime());
        for (dstinfo.ResetLocation(), theQ.ResetLocation();
             dstinfo.NextLocation() && theQ.NextLocation();)
          dstinfo.FloatValue(theQ.FloatValue());
      }

  data2->Write(theOutFile);
}

// ----------------------------------------------------------------------
/*!
 * \brief Copy the point data values of the current parameter, level and time
 */
// ----------------------------------------------------------------------

void CopyLocationValues(NFmiFastQueryInfo& theSrc, NFmiFastQueryInfo& theDst, bool samePoints)
{
  // copy point data quickly if all stations are kept
  if (samePoints)
  {
    for (theDst.ResetLocation(), theSrc.ResetLocation();
         theDst.NextLocation() && theSrc.NextLocation();)
      theDst.FloatValue(theSrc.FloatValue());
  }
  else
  {
    // slower version if stations are kept & removed
    for (theDst.ResetLocation(); theDst.NextLocation();)
    {
      theSrc.Location(theDst.Location()->GetIdent());
      theDst.FloatValue(theSrc.FloatValue());
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Copy point data to output info
 */
// ----------------------------------------------------------------------

void CopyNonGridData(NFmiFastQueryInfo& theSrc, NFmiFastQueryInfo& theDst, bool samePoints)
{
  for (theDst.ResetParam(); theDst.NextParam();)
  {
    if (theSrc.Param(theDst.Param()))
    {
      for (theDst.ResetLevel(); theDst.NextLevel();)
      {
        if (!theSrc.Level(*theDst.Level()))
          throw runtime_error("Level not available in querydata");

        for (theDst.ResetTime(); theDst.NextTime();)
        {
          if (!theSrc.Time(theDst.ValidTime()))
            throw runtime_error("Time not available in querydata");

          CopyLocationValues(theSrc, theDst, samePoints);
        }
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Copy the current analysis parameter from the first time to all times for the current level
 */
// ----------------------------------------------------------------------

void CopyAnalysisLevel(NFmiFastQueryInfo& theSrc, NFmiFastQueryInfo& theDst, bool same_stations)
{
  // copy point data
  if (same_stations)
  {
    for (theDst.ResetLocation(), theSrc.ResetLocation();
         theDst.NextLocation() && theSrc.NextLocation();)
    {
      // We assume first time is origin time
      theSrc.FirstTime();
      for (theDst.ResetTime(); theDst.NextTime();)
        theDst.FloatValue(theSrc.FloatValue());
    }
  }
  else
  {
    for (theDst.ResetLocation(); theDst.NextLocation();)
    {
      if (theSrc.Location(theDst.Location()->GetIdent()))
      {
        // We assume first time is origin time
        theSrc.FirstTime();
        for (theDst.ResetTime(); theDst.NextTime();)
        {
          theDst.FloatValue(theSrc.FloatValue());
        }
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Copy given parameters from the first time to all subsequent times
 *
 * Mainly used for copying topography and similar information
 */
// ----------------------------------------------------------------------

void CopyAnalysisParameters(NFmiFastQueryInfo& theSrc,
                            NFmiFastQueryInfo& theDst,
                            const vector<string>& theParams,
                            bool same_stations)
{
  for (const auto& paramname : theParams)
  {
    FmiParameterName paramnum = parse_param(paramname);
    if (!theSrc.Param(paramnum) || !theDst.Param(paramnum))
      throw runtime_error("Error copy parameter " + paramname +
                          " from origin time, parameter not available");

    for (theDst.ResetLevel(); theDst.NextLevel();)
    {
      if (!theSrc.Level(*theDst.Level()))
        throw runtime_error("Level not available in querydata");

      CopyAnalysisLevel(theSrc, theDst, same_stations);
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Command line options
 */
// ----------------------------------------------------------------------

struct Options
{
  string infile;                      // the querydata to read
  string outfile = "-";               // the querydata to write
  string geometry;                    // the geometry to extract
  string bounds;                      // the latlon bounding box to extract
  string steps;                       // the stepsizes for the geometry
  string proj;                        // the projection to interpolate to
  string new_producer_name;           // new optional producer name
  int new_producer_id = -1;           // new optional producer number
  vector<string> parameters;          // the parameters to extract
  vector<string> delparameters;       // the parameters to remove
  vector<string> newparameters;       // the parameters to be added
  vector<string> analysisparameters;  // the parameters to be extracted from origin time
  vector<string> renamedparameters;   // the parameter rename commands
  set<int> stations;                  // the stations to extract
  set<int> nostations;                // the stations to remove
  list<int> levels;                   // the levels to extract

  bool preserve_version = false;

  list<int> times;  // the times to extract
  bool utc = false;

  NFmiTime reftime;         // reference time in UTC time
  bool usereftime = false;  // option -z or -Z not used yet

  int local_hour = -1;            // the local hour to extract
  int utc_hour = -1;              // the UTC hour to extract
  int utc_minute = -1;            // the UTC minute to extract
  double missing_limit = -1;      // allowed percentage of missing values
  std::string missing_parameter;  // parameter to be checked (default = all)

  bool multifile = false;  // -R enables multifile input

  vector<NFmiTime> crops;  // -S option
};

// ----------------------------------------------------------------------
/*!
 * \brief Parse the input and output file arguments
 */
// ----------------------------------------------------------------------

void parse_file_arguments(NFmiCmdLine& cmdline, Options& options)
{
  if (cmdline.NumberofParameters() > 2)
    throw runtime_error("Two command line arguments are expected, not " +
                        lexical_cast<string>(cmdline.NumberofParameters()));

  if (cmdline.NumberofParameters() >= 1)
    options.infile = cmdline.Parameter(1);
  else
    options.infile = "-";

  if (cmdline.NumberofParameters() >= 2)
    options.outfile = cmdline.Parameter(2);

  if (options.infile.empty())
    throw runtime_error("Input querydata filename cannot be empty");
  if (options.outfile.empty())
    throw runtime_error("Output querydata filename cannot be empty");
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the area, station, level and parameter options
 */
// ----------------------------------------------------------------------

void parse_area_and_param_options(NFmiCmdLine& cmdline, Options& options)
{
  if (cmdline.isOption('g'))
    options.geometry = cmdline.OptionValue('g');

  if (cmdline.isOption('G'))
    options.bounds = cmdline.OptionValue('G');

  if (cmdline.isOption('d'))
    options.steps = cmdline.OptionValue('d');

  if (cmdline.isOption('P'))
    options.proj = cmdline.OptionValue('P');

  if (cmdline.isOption('p'))
    options.parameters = NFmiStringTools::Split(cmdline.OptionValue('p'));

  if (cmdline.isOption('r'))
    options.delparameters = NFmiStringTools::Split(cmdline.OptionValue('r'));

  if (cmdline.isOption('a'))
    options.newparameters = NFmiStringTools::Split(cmdline.OptionValue('a'));

  if (cmdline.isOption('A'))
    options.analysisparameters = NFmiStringTools::Split(cmdline.OptionValue('A'));

  if (cmdline.isOption('n'))
  {
    options.renamedparameters = NFmiStringTools::Split(cmdline.OptionValue('n'));
    if (options.renamedparameters.size() % 3 != 0)
      throw runtime_error("Option -n argument list must have 3 elements for each rename task");
  }

  if (cmdline.isOption('l'))
    options.levels = NFmiStringTools::Split<list<int> >(cmdline.OptionValue('l'));

  if (cmdline.isOption('w'))
    options.stations = parse_numberlist(cmdline.OptionValue('w'));

  if (cmdline.isOption('W'))
    options.nostations = parse_numberlist(cmdline.OptionValue('W'));
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the missing value limit option -m
 */
// ----------------------------------------------------------------------

void parse_missing_option(NFmiCmdLine& cmdline, Options& options)
{
  if (cmdline.isOption('m'))
  {
    vector<string> parts = NFmiStringTools::Split(cmdline.OptionValue('m'));
    if (parts.size() == 1)
      options.missing_limit = NFmiStringTools::Convert<double>(parts[0]);
    else if (parts.size() == 2)
    {
      options.missing_parameter = parts[0];
      options.missing_limit = NFmiStringTools::Convert<double>(parts[1]);
    }
    else
      throw runtime_error("Option -m argument has too many parts");

    if (options.missing_limit <= 0 || options.missing_limit > 100)
      throw runtime_error("Option -m argument must be in the range 0-100, excluding 0");

    // This optimizes for speed in case user input is stupid
    if (options.missing_limit == 100)
      options.missing_limit = -1;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the time selection options
 */
// ----------------------------------------------------------------------

void parse_time_options(NFmiCmdLine& cmdline, Options& options)
{
  if (cmdline.isOption('t') && cmdline.isOption('T'))
    throw runtime_error("Cannot use -t and -T simultaneously");

  if (cmdline.isOption('z') && cmdline.isOption('Z'))
    throw runtime_error("Cannot use -z and -Z simultaneously");

  if (cmdline.isOption('t'))
  {
    options.utc = false;
    options.times = NFmiStringTools::Split<list<int> >(cmdline.OptionValue('t'));
    if (options.times.empty() || options.times.size() > 3)
      throw runtime_error("Option -t argument must have 1-3 values");
  }

  if (cmdline.isOption('T'))
  {
    options.utc = true;
    options.times = NFmiStringTools::Split<list<int> >(cmdline.OptionValue('T'));
    if (options.times.empty() || options.times.size() > 3)
      throw runtime_error("Option -t argument must have 1-3 values");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the hour and minute selection options
 */
// ----------------------------------------------------------------------

void parse_hour_options(NFmiCmdLine& cmdline, Options& options)
{
  if (cmdline.isOption('i'))
  {
    options.local_hour = NFmiStringTools::Convert<int>(cmdline.OptionValue('i'));
    if (options.local_hour < 0 || options.local_hour > 23)
      throw runtime_error("Option -i argument must be in the range 0-23");
  }

  if (cmdline.isOption('I'))
  {
    options.utc_hour = NFmiStringTools::Convert<int>(cmdline.OptionValue('I'));
    if (options.utc_hour < 0 || options.utc_hour > 23)
      throw runtime_error("Option -I argument must be in the range 0-23");
  }

  if (cmdline.isOption('M'))
  {
    options.utc_minute = NFmiStringTools::Convert<int>(cmdline.OptionValue('M'));
    if (options.utc_minute < 0 || options.utc_minute > 59)
      throw runtime_error("Option -M argument must be in the range 0-59");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the reference time and time selection options
 */
// ----------------------------------------------------------------------

void parse_reftime_options(NFmiCmdLine& cmdline, Options& options)
{
  if (cmdline.isOption('Z'))
  {
    options.reftime = ParseDate(cmdline.OptionValue('Z'));
    options.usereftime = true;
  }

  if (cmdline.isOption('z'))
  {
    NFmiTime tmp = ParseDate(cmdline.OptionValue('z'));
    options.reftime = toUtcTime(tmp);
    options.usereftime = true;
  }

  if (cmdline.isOption('S'))
  {
    vector<string> times = NFmiStringTools::Split(cmdline.OptionValue('S'), ",");
    for (auto& time : times)
      options.crops.push_back(ParseDate(time));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Check for incompatible options
 */
// ----------------------------------------------------------------------

void check_option_conflicts(NFmiCmdLine& cmdline, const Options& options)
{
  if (!options.parameters.empty() && !options.delparameters.empty())
    throw runtime_error("Options -p and -r are mutually exclusive");

  if (cmdline.isOption('S') && (cmdline.isOption('t') || cmdline.isOption('T') ||
                                cmdline.isOption('i') || cmdline.isOption('I')))
    throw runtime_error("Option -S cannot be used with the other time options");

  if (options.utc_hour >= 0 && options.local_hour >= 0)
    throw runtime_error("Cannot use options -i and -I simultaneously");

  if (!options.bounds.empty() && !options.geometry.empty())
    throw runtime_error("Cannot use -g and -G options simultaneously");

  if (!options.proj.empty() && (!options.bounds.empty() || !options.geometry.empty()))
    throw runtime_error("Cannot use -P and -g or -G options simultaneously");
}

// ----------------------------------------------------------------------
/*!
 * \brief Modify the producer if requested
 */
// ----------------------------------------------------------------------

void set_producer(NFmiFastQueryInfo& theSrc, NFmiFastQueryInfo& theDst, const Options& options)
{
  if (!options.new_producer_name.empty() || options.new_producer_id > 0)
  {
    theSrc.FirstParam();  // first parameter as base for new settings
    NFmiProducer producer(*theSrc.Producer());
    if (!options.new_producer_name.empty())
      producer.SetName(options.new_producer_name);
    if (options.new_producer_id > 0)
      producer.SetIdent(options.new_producer_id);
    theDst.SetProducer(producer);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove timesteps with too much missing data
 */
// ----------------------------------------------------------------------

void remove_bad_times(std::shared_ptr<NFmiQueryData>& data,
                      NFmiFastQueryInfo& dstinfo,
                      const NFmiParamDescriptor& pdesc,
                      const NFmiHPlaceDescriptor& hdesc,
                      const NFmiVPlaceDescriptor& vdesc,
                      double version,
                      const Options& options)
{
  set<NFmiMetTime> badtimes =
      FindBadTimes(dstinfo, options.missing_limit, options.missing_parameter);
  if (!badtimes.empty())
  {
    // Filter out the bad timesteps
    NFmiTimeDescriptor tdesc2(MakeTimeDescriptor(dstinfo, badtimes));
    NFmiFastQueryInfo info2(pdesc, tdesc2, hdesc, vdesc, version);
    NFmiQueryData* data2 = NFmiQueryDataUtil::CreateEmptyData(info2);
    if (data2 == nullptr)
      throw runtime_error("Could not allocate memory for result data");
    NFmiFastQueryInfo dstinfo2(data2);

    for (dstinfo2.ResetParam(), dstinfo.ResetParam(); dstinfo2.NextParam() && dstinfo.NextParam();)
      for (dstinfo2.ResetLevel(), dstinfo.ResetLevel();
           dstinfo2.NextLevel() && dstinfo.NextLevel();)
        for (dstinfo2.ResetTime(); dstinfo2.NextTime();)
        {
          dstinfo.Time(dstinfo2.ValidTime());
          for (dstinfo2.ResetLocation(), dstinfo.ResetLocation();
               dstinfo2.NextLocation() && dstinfo.NextLocation();)
            dstinfo2.FloatValue(dstinfo.FloatValue());
        }

    data.reset(data2);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform parameter replacements
 */
// ----------------------------------------------------------------------

void rename_params(NFmiQueryData& theData, const vector<string>& opt_renamedparameters)
{
  // Manipulate querydata directly
  NFmiQueryInfo* realinfo = theData.Info();

  for (std::size_t i = 0; i < opt_renamedparameters.size(); i += 3)
  {
    FmiParameterName oldparam = parse_param(opt_renamedparameters[i]);
    FmiParameterName newparam = parse_param(opt_renamedparameters[i + 1]);
    const auto& newname = opt_renamedparameters[i + 2];

    if (!realinfo->Param(oldparam))
      throw runtime_error("No parameter " + opt_renamedparameters[i] + " to rename in the data");

    NFmiDataIdent newident(realinfo->Param());  // default values for attributes from old parameter
    newident.GetParam()->SetIdent(newparam);
    newident.GetParam()->SetName(newname);
    realinfo->Param() = newident;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief The main work subroutine for the main program
 *
 * The main program traps all exceptions throw in here.
 */
// ----------------------------------------------------------------------

int run(int argc, const char* argv[])
{
  Options options;

  // Read command line arguments

  NFmiCmdLine cmdline(argc, argv, "hVg!G!P!p!r!a!A!l!t!T!d!w!W!i!I!z!Z!S!m!M!Rn!N!D!");
  if (cmdline.Status().IsError())
    throw runtime_error(cmdline.Status().ErrorLog().CharPtr());

  // help option must be checked before checking the number
  // of command line arguments

  if (cmdline.isOption('h'))
  {
    usage();
    return 0;
  }

  if (cmdline.isOption('V'))
    options.preserve_version = !options.preserve_version;

  // extract command line parameters

  parse_file_arguments(cmdline, options);

  // extract command line options

  parse_area_and_param_options(cmdline, options);
  parse_missing_option(cmdline, options);
  parse_time_options(cmdline, options);
  parse_hour_options(cmdline, options);
  parse_reftime_options(cmdline, options);

  if (cmdline.isOption('R'))
    options.multifile = !options.multifile;

  if (cmdline.isOption('N'))
    options.new_producer_name = cmdline.OptionValue('N');

  if (cmdline.isOption('D'))
    options.new_producer_id = NFmiStringTools::Convert<int>(cmdline.OptionValue('D'));

  check_option_conflicts(cmdline, options);

  // read the querydata

  NFmiQueryData* qd = nullptr;
  NFmiFastQueryInfo* srcinfo = nullptr;

  if (options.multifile)
  {
    srcinfo = new NFmiMultiQueryInfo(options.infile);
  }
  else
  {
    qd = new NFmiQueryData(options.infile);
    srcinfo = new NFmiFastQueryInfo(qd);
  }

  // Cannot extract stations from grid data

  if ((!options.stations.empty() || !options.nostations.empty()) && srcinfo->IsGrid())
    throw runtime_error("Cannot extract stations from gridded data");

  // Establish the querydata version to be produced

  double version = DefaultFmiInfoVersion;
  if (options.preserve_version)
    version = srcinfo->InfoVersion();

  // Special optimization for fast removal of missing timesteps only. This is
  // mainly useful to remove invalid timesteps produced by model post processing
  // which must not get into production. Cannot handle multifiles quickly yet.

  if (cmdline.NumberofOptions() == 1 && options.missing_limit > 0 && !options.multifile)
  {
    RemoveBadTimes(
        *qd, *srcinfo, version, options.missing_limit, options.missing_parameter, options.outfile);
    return 0;
  }

  // Non-trivial cropping follows

  // create new descriptors for the new data

  int x1 = 0;
  int y1 = 0;
  int dx = 0;
  int dy = 0;
  NFmiHPlaceDescriptor hdesc(MakeHPlaceDescriptor(*srcinfo,
                                                  options.stations,
                                                  options.nostations,
                                                  options.geometry,
                                                  options.bounds,
                                                  options.steps,
                                                  options.proj,
                                                  x1,
                                                  y1,
                                                  dx,
                                                  dy));
  NFmiVPlaceDescriptor vdesc(MakeVPlaceDescriptor(*srcinfo, options.levels));
  NFmiParamDescriptor pdesc(MakeParamDescriptor(*srcinfo,
                                                options.parameters,
                                                options.delparameters,
                                                options.newparameters,
                                                options.analysisparameters));
  NFmiTimeDescriptor tdesc(MakeTimeDescriptor(*srcinfo,
                                              options.crops,
                                              options.times,
                                              options.utc,
                                              options.local_hour,
                                              options.utc_hour,
                                              options.utc_minute,
                                              options.reftime,
                                              options.usereftime));

  // now create new data based on the new descriptors

  NFmiFastQueryInfo info(pdesc, tdesc, hdesc, vdesc, version);

  std::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(info));
  if (data == nullptr)
    throw runtime_error("Could not allocate memory for result data");

  NFmiFastQueryInfo dstinfo(data.get());

  // Modify the producer as if requested
  set_producer(*srcinfo, dstinfo, options);

  // finally fill the new data with values
  //
  // currently FillGridDataFullMT does not handle multifiles

  bool same_stations = (options.stations.empty() && options.nostations.empty());

  if ((!dstinfo.Grid()) || options.multifile)
    CopyNonGridData(*srcinfo, dstinfo, same_stations || options.multifile);
  else
  {
    int max_threads = 1;
    NFmiQueryDataUtil::FillGridDataFullMT(
        qd, data.get(), gMissingIndex, gMissingIndex, max_threads);
  }

  // Copy parameter values from origin time if necessary

  if (!options.analysisparameters.empty())
    CopyAnalysisParameters(*srcinfo, dstinfo, options.analysisparameters, same_stations);

  // Remove timesteps with too much missing data

  if (options.missing_limit > 0)
    remove_bad_times(data, dstinfo, pdesc, hdesc, vdesc, version, options);

  // Perform parameter replacements

  if (!options.renamedparameters.empty())
    rename_params(*data, options.renamedparameters);

  data->Write(options.outfile);

  delete srcinfo;
  delete qd;

  return 0;
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief The main program
 *
 * The main program is only an error trapping driver for run
 */
// ----------------------------------------------------------------------

int main(int argc, const char* argv[])
{
  try
  {
    return run(argc, argv);
  }
  catch (const std::exception& e)
  {
    cerr << "Error: Caught an exception:"
         << "\n"
         << "--> " << e.what() << "\n";
    return 1;
  }
}

// ======================================================================
