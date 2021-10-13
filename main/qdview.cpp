// ======================================================================
/*!
 * \file
 * \brief Implementation of qdview - a querydata locations display program
 */
// ======================================================================

#include <boost/lexical_cast.hpp>
#include <imagine/NFmiGshhsTools.h>
#include <imagine/NFmiImage.h>
#include <imagine/NFmiPath.h>
#include <newbase/NFmiArea.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiQueryData.h>
#include <string>

#ifdef WGS84
#include <newbase/NFmiAreaTools.h>
#else
#include <newbase/NFmiLatLonArea.h>
#endif

using namespace std;
using namespace boost;

// ----------------------------------------------------------------------
/*!
 * \brief Update bounding box based on given point
 *
 * \param thePoint The point to add to the bounding box
 * \param theMinLon The variable in which to store the minimum longitude
 * \param theMinLat The variable in which to store the minimum latitude
 * \param theMaxLon The variable in which to store the maximum longitude
 * \param theMaxLat The variable in which to store the maximum latitude
 */
// ----------------------------------------------------------------------

void UpdateBBox(const NFmiPoint& thePoint,
                double& theMinLon,
                double& theMinLat,
                double& theMaxLon,
                double& theMaxLat)
{
  theMinLon = std::min(theMinLon, thePoint.X());
  theMinLat = std::min(theMinLat, thePoint.Y());
  theMaxLon = std::max(theMaxLon, thePoint.X());
  theMaxLat = std::max(theMaxLat, thePoint.Y());
}

// ----------------------------------------------------------------------
/*!
 * \brief Find geographic bounding box for given area
 *
 * The bounding box is found by traversing the edges of the area
 * and converting the coordinates to geographic ones for extrema
 * calculations.
 *
 * \param theArea The area
 * \param theMinLon The variable in which to store the minimum longitude
 * \param theMinLat The variable in which to store the minimum latitude
 * \param theMaxLon The variable in which to store the maximum longitude
 * \param theMaxLat The variable in which to store the maximum latitude
 */
// ----------------------------------------------------------------------

void FindBBox(const NFmiArea& theArea,
              double& theMinLon,
              double& theMinLat,
              double& theMaxLon,
              double& theMaxLat)
{
  // Good initial values are obtained from the corners

  theMinLon = theArea.TopLeftLatLon().X();
  theMinLat = theArea.TopLeftLatLon().Y();
  theMaxLon = theMinLon;
  theMaxLat = theMinLat;

  const unsigned int divisions = 100;

  // Go through the top edge

  for (unsigned int i = 0; i <= divisions; i++)
  {
    NFmiPoint xy(theArea.Left() + theArea.Width() * i / divisions, theArea.Top());
    NFmiPoint latlon(theArea.ToLatLon(xy));
    UpdateBBox(latlon, theMinLon, theMinLat, theMaxLon, theMaxLat);
  }

  // Go through the bottom edge

  for (unsigned int i = 0; i <= divisions; i++)
  {
    NFmiPoint xy(theArea.Left() + theArea.Width() * i / divisions, theArea.Bottom());
    NFmiPoint latlon(theArea.ToLatLon(xy));
    UpdateBBox(latlon, theMinLon, theMinLat, theMaxLon, theMaxLat);
  }

  // Go through the left edge

  for (unsigned int i = 0; i <= divisions; i++)
  {
    NFmiPoint xy(theArea.Left(), theArea.Bottom() + theArea.Height() * i / divisions);
    NFmiPoint latlon(theArea.ToLatLon(xy));
    UpdateBBox(latlon, theMinLon, theMinLat, theMaxLon, theMaxLat);
  }

  // Go through the top edge

  for (unsigned int i = 0; i <= divisions; i++)
  {
    NFmiPoint xy(theArea.Left(), theArea.Bottom() + theArea.Height() * i / divisions);
    NFmiPoint latlon(theArea.ToLatLon(xy));
    UpdateBBox(latlon, theMinLon, theMinLat, theMaxLon, theMaxLat);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Print usage information
 */
// ----------------------------------------------------------------------

void usage()
{
  cout << "Usage: qdview [options] [querydata] [pngfile]" << endl
       << endl
       << "qdview draws the grid in a querydata or the latlon bounding box" << endl
       << "for point data." << endl
       << endl
       << "Options:" << endl
       << endl
       << "\t-x [width]\tThe width of the image (default 400)" << endl
       << "\t-y [height]\tThe height of the image" << endl
       << "\t-m [degrees]\tThe default margin for map data" << endl
       << "\t-r [char]\tThe resolution of GSHHS data to be used (default crude)" << endl
       << "\t-s [pixels]\tThe width of the square dot" << endl
       << "\t-S [pixels]\tThe width of the square dot, but only for valid points" << endl
       << "\t-p [param]\tThe parameter to use for determining the valid points (default=all)"
       << endl
       << endl
       << "If no width or height is specified, the height is calculated from" << endl
       << "the default width. If only one of width and height are specified," << endl
       << "the other is calculated from the data projection information so" << endl
       << "that the aspect ratio is preserved. If both width and height are" << endl
       << "given, the aspect ratio may be distorted" << endl
       << endl
       << "Available GSHHS resolutions are:" << endl
       << endl
       << "\tcrude resolution (25km)" << endl
       << "\tlow resolution (5 km)" << endl
       << "\tintermediate resolution (1km)" << endl
       << "\thigh resolution (0.2km)" << endl
       << "\tfull resolution" << endl
       << endl
       << "Example: qdview -x 600 -r l /data/pal/querydata/pal/skandinavia/pinta pal.png" << endl
       << endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Minimum function for 4 arguments
 *
 * \return The minimum of the 4 arguments
 */
// ----------------------------------------------------------------------

template <typename T>
const T& min(const T& arg1, const T& arg2, const T& arg3, const T& arg4)
{
  return min(min(arg1, arg2), min(arg3, arg4));
}

// ----------------------------------------------------------------------
/*!
 * \brief Maximum function for 4 arguments
 *
 * \return The maximum of the 4 arguments
 */
// ----------------------------------------------------------------------

template <typename T>
const T& max(const T& arg1, const T& arg2, const T& arg3, const T& arg4)
{
  return max(min(arg1, arg2), max(arg3, arg4));
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate a bounding latlon area for point data
 *
 * \param q The query info
 * \return Latlon area containing all points in the data
 */
// ----------------------------------------------------------------------

NFmiArea* create_bbox(NFmiFastQueryInfo& q)
{
  q.NextLocation();
  if (!q.NextLocation()) throw runtime_error("Querydata contains no points!");
  double minlon = q.LatLon().X();
  double maxlon = q.LatLon().X();
  double minlat = q.LatLon().Y();
  double maxlat = q.LatLon().Y();
  while (q.NextLocation())
  {
    if (q.LatLon().X() != kFloatMissing && q.LatLon().Y() != kFloatMissing)
    {
      minlon = min(minlon, q.LatLon().X());
      maxlon = max(maxlon, q.LatLon().X());
      minlat = min(minlat, q.LatLon().Y());
      maxlat = max(maxlat, q.LatLon().Y());
    }
  }

#ifdef WGS84  
  return NFmiAreaTools::CreateLegacyLatLonArea(NFmiPoint(minlon, minlat),
                                               NFmiPoint(maxlon, maxlat));
#else
  return new NFmiLatLonArea(NFmiPoint(minlon, minlat), NFmiPoint(maxlon, maxlat));
#endif  
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate width or height if necessary
 *
 * \param theArea The projection
 * \param theWidth The width
 * \param theHeight The height
 */
// ----------------------------------------------------------------------

void check_size(const NFmiArea* theArea, int& theWidth, int& theHeight)
{
  const int default_width = 400;

  const NFmiPoint bl = theArea->LatLonToWorldXY(theArea->BottomLeftLatLon());
  const NFmiPoint tr = theArea->LatLonToWorldXY(theArea->TopRightLatLon());

  if (theWidth < 0 && theHeight < 0) theWidth = default_width;

  if (theWidth <= 0 && theHeight > 0)
    theWidth = static_cast<int>((tr.X() - bl.X()) / (tr.Y() - bl.Y()) * theHeight);
  else if (theHeight <= 0 && theWidth > 0)
    theHeight = static_cast<int>((tr.Y() - bl.Y()) / (tr.X() - bl.X()) * theWidth);
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw GSHHS map data on the image
 *
 * \param theImage The image to draw onto
 * \param theProj The projection
 * \param theResolution The resolution of the GSHHS data
 */
// ----------------------------------------------------------------------

void draw_map(Imagine::NFmiImage& theImage, NFmiArea* theProj, char theResolution)
{
  const string gshhsfile = string("gshhs_") + theResolution + ".b";

  double minlon, minlat, maxlon, maxlat;

  FindBBox(*theProj, minlon, minlat, maxlon, maxlat);

  Imagine::NFmiPath path =
      Imagine::NFmiGshhsTools::ReadPath(gshhsfile, minlon, minlat, maxlon, maxlat);

  path.Project(theProj);

  path.Fill(theImage,
            Imagine::NFmiColorTools::MakeColor(0, 255, 0),
            Imagine::NFmiColorTools::kFmiColorCopy);

  path.Stroke(theImage,
              Imagine::NFmiColorTools::MakeColor(0, 0, 0),
              Imagine::NFmiColorTools::kFmiColorCopy);
}

// ----------------------------------------------------------------------
/*!
 * \brief Draw querydata locations onto the given image
 *
 * \param theImage The image to draw into
 * \param dotsize The size of the grid dots
 * \param validpoints True if only points with valid data should be drawn
 * \param param The parameter to use for checking valid points (or check all if bad param)
 * \param theProj The projection
 * \param theQ The querydata
 */
// ----------------------------------------------------------------------

void draw_grid(Imagine::NFmiImage& theImage,
               int dotsize,
               bool validpoints,
               FmiParameterName param,
               const NFmiArea* theProj,
               NFmiFastQueryInfo& theQ)
{
  if (dotsize <= 0) return;

  Imagine::NFmiImage red(dotsize, dotsize);
  red.Erase(Imagine::NFmiColorTools::MakeColor(255, 0, 0));

  if (param != kFmiBadParameter)
    if (!theQ.Param(param)) throw runtime_error("The data does not have the requested parameter");

  theQ.ResetLocation();
  while (theQ.NextLocation())
  {
    const NFmiPoint p = theQ.LatLon();
    const NFmiPoint xy = theProj->ToXY(p);

    const int x = static_cast<int>(round(xy.X()));
    const int y = static_cast<int>(round(xy.Y()));

    // Establish whether to render or not
    bool ok = true;
    if (validpoints)
    {
      ok = false;
      for (theQ.ResetLevel(); theQ.NextLevel();)
      {
        if (param == kFmiBadParameter)
        {
          for (theQ.ResetParam(); theQ.NextParam();)
          {
            for (theQ.ResetTime(); theQ.NextTime();)
            {
              if (theQ.FloatValue() != kFloatMissing)
              {
                ok = true;
                goto pointok;
              }
            }
          }
        }
        else
        {
          for (theQ.ResetTime(); theQ.NextTime();)
          {
            if (theQ.FloatValue() != kFloatMissing)
            {
              ok = true;
              goto pointok;
            }
          }
        }
      }
    pointok:;
    }

    if (ok)
    {
      theImage.Composite(
          red, Imagine::NFmiColorTools::kFmiColorCopy, Imagine::kFmiAlignCenter, x, y);
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief The main work subroutine for the main program
 *
 * The main program traps all exceptions throw in here.
 */
// ----------------------------------------------------------------------

int domain(int argc, const char* argv[])
{
  bool validpoints = false;
  int dotsize = 3;
  int margin = 0;
  int width = -1;
  int height = -1;
  char resolution = 'c';
  FmiParameterName param = kFmiBadParameter;

  NFmiCmdLine cmdline(argc, argv, "hr!x!y!m!s!S!p!");

  if (cmdline.Status().IsError()) throw runtime_error(cmdline.Status().ErrorLog().CharPtr());

  if (cmdline.NumberofParameters() != 2)
    throw runtime_error("Exactly two parameter is expected on the command line");

  const string filename = cmdline.Parameter(1);
  const string imagefile = cmdline.Parameter(2);

  if (filename.empty()) throw runtime_error("Empty filename argument");

  if (imagefile.empty()) throw runtime_error("Empty image filename argument");

  if (cmdline.isOption('m'))
  {
    margin = lexical_cast<int>(cmdline.OptionValue('m'));
    if (margin < 0) throw runtime_error("Argument for option -m must be nonnegative");
  }

  if (cmdline.isOption('s'))
  {
    validpoints = false;
    dotsize = lexical_cast<int>(cmdline.OptionValue('s'));
    if (dotsize < 0) throw runtime_error("Argument for option -s must be nonnegative");
  }
  if (cmdline.isOption('S'))
  {
    validpoints = true;
    dotsize = lexical_cast<int>(cmdline.OptionValue('S'));
    if (dotsize < 0) throw runtime_error("Argument for option -S must be nonnegative");
  }
  if (cmdline.isOption('s') && cmdline.isOption('S'))
    throw runtime_error("Options -s and -S are mutually exclusive");

  if (cmdline.isOption('r'))
  {
    const string tmp = cmdline.OptionValue('r');
    if (tmp.empty()) throw runtime_error("Invalid resolution " + tmp + " for option -r");
    resolution = tmp[0];
  }

  if (cmdline.isOption('x'))
  {
    width = lexical_cast<int>(cmdline.OptionValue('x'));
    if (width <= 0) throw runtime_error("Argument for option -x must be positive");
  }

  if (cmdline.isOption('y'))
  {
    height = lexical_cast<int>(cmdline.OptionValue('y'));
    if (height <= 0) throw runtime_error("Argument for option -y must be positive");
  }

  if (cmdline.isOption('p'))
  {
    NFmiEnumConverter converter;
    param = FmiParameterName(converter.ToEnum(cmdline.OptionValue('p')));
    if (param == kFmiBadParameter)
      throw runtime_error(string("Unknown parameter: ") + cmdline.OptionValue('p'));
  }

  if (cmdline.isOption('h'))
  {
    usage();
    return 0;
  }

  NFmiQueryData qd(filename);
  NFmiFastQueryInfo q(&qd);

  NFmiArea* area = 0;
  if (q.Area() != 0)
    area = q.Area()->Clone();
  else
    area = create_bbox(q);

  NFmiArea* drawarea = 0;
  if (margin == 0)
    drawarea = area;
  else
  {
    NFmiPoint bl = area->BottomLeftLatLon();
    NFmiPoint tr = area->TopRightLatLon();
    bl.X(max(-180.0, bl.X() - margin));
    bl.Y(max(-90.0, bl.Y() - margin));
    tr.X(min(180.0, tr.X() + margin));
    tr.Y(min(90.0, tr.Y() + margin));
    drawarea = area->NewArea(bl, tr);
  }

  // Check and recalculate width & height if necessary

  check_size(drawarea, width, height);

  // Start drawing the image
  Imagine::NFmiImage image(width, height);
  image.Erase(Imagine::NFmiColorTools::MakeColor(255, 255, 255));

  // Rendering projection

  NFmiArea* proj = drawarea->Clone();
  proj->SetXYArea(NFmiRect(NFmiPoint(0, 0), NFmiPoint(width, height)));

  // Draw map data

  draw_map(image, proj, resolution);

  // Draw gridpoints

  draw_grid(image, dotsize, validpoints, param, proj, q);

// Save image and exit
#ifdef IMAGINE_IGNORE_FORMATS
  throw std::runtime_error("Png support not in use, remove IMAGINE_IGNORE_FORMATS define.");
#else
  image.WritePng(imagefile);
#endif

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief The main program
 *
 * The main program is only an error trapping driver for domain
 */
// ----------------------------------------------------------------------

int main(int argc, const char* argv[])
{
  try
  {
    return domain(argc, argv);
  }
  catch (const std::exception& e)
  {
    cerr << "Error: Caught an exception:" << endl << "--> " << e.what() << endl;
    usage();
    return 1;
  }
}
