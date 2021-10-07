// ======================================================================
/*!
 * \file
 * \brief Implementation of the Projection class
 */
// ======================================================================

#include "Projection.h"

#include <fmt/format.h>
#include <imagine/NFmiImage.h>
#include <imagine/NFmiPath.h>
#include <newbase/NFmiGlobals.h>
#include <newbase/NFmiPoint.h>
#include <stdexcept>

#ifdef WGS84
#include <newbase/NFmiAreaTools.h>
#else
#include <newbase/NFmiEquidistArea.h>
#include <newbase/NFmiGlobals.h>
#include <newbase/NFmiGnomonicArea.h>
#include <newbase/NFmiLatLonArea.h>
#include <newbase/NFmiMercatorArea.h>
#include <newbase/NFmiPoint.h>
#include <newbase/NFmiStereographicArea.h>
#include <newbase/NFmiYKJArea.h>
#endif

using namespace Imagine;

namespace FMI
{
namespace RadContour
{
// ----------------------------------------------------------------------
/*!
 * The implementation hiding pimple for class Projection
 */
// ----------------------------------------------------------------------

//! The implementation hiding pimple for class Projection
struct ProjectionPimple
{
  std::string itsType;
  std::string itsEllipsoid;
  float itsCentralLatitude = kFloatMissing;
  float itsCentralLongitude = kFloatMissing;
  float itsTrueLatitude = kFloatMissing;
  NFmiPoint itsBottomLeft{kFloatMissing, kFloatMissing};
  NFmiPoint itsTopRight{kFloatMissing, kFloatMissing};
  NFmiPoint itsCenter{kFloatMissing, kFloatMissing};
  float itsScale = kFloatMissing;
};

// ----------------------------------------------------------------------
/*!
 * The destructor does nothing special.
 */
// ----------------------------------------------------------------------

Projection::~Projection() = default;
// ----------------------------------------------------------------------
/*!
 * The void constructor merely initializes the implementation pimple
 */
// ----------------------------------------------------------------------

Projection::Projection() : itsPimple(new ProjectionPimple()) {}
// ----------------------------------------------------------------------
/*!
 * The copy constructor
 *
 * \param theProjection The object to copy
 */
// ----------------------------------------------------------------------

Projection::Projection(const Projection& theProjection)
    : itsPimple(new ProjectionPimple(*theProjection.itsPimple))
{
}

// ----------------------------------------------------------------------
/*!
 * Assignment operator
 *
 * \param theProjection The projection to copy
 * \return This
 */
// ----------------------------------------------------------------------

Projection& Projection::operator=(const Projection& theProjection)
{
  if (this != &theProjection)
  {
    itsPimple.reset(new ProjectionPimple(*theProjection.itsPimple));
  }
  return *this;
}

// ----------------------------------------------------------------------
/*!
 * Set the type of the projection.
 *
 * \param theType The name of the projection
 */
// ----------------------------------------------------------------------

void Projection::type(const std::string& theType)
{
  itsPimple->itsType = theType;
}
// ----------------------------------------------------------------------
/*!
 * Set the central longitude of the projection.
 *
 * \param theLongitude The longitude
 */
// ----------------------------------------------------------------------

void Projection::centralLongitude(float theLongitude)
{
  itsPimple->itsCentralLongitude = theLongitude;
}

// ----------------------------------------------------------------------
/*!
 * Set the central latitude of the projection.
 *
 * \param theLatitude The latitude
 */
// ----------------------------------------------------------------------

void Projection::centralLatitude(float theLatitude)
{
  itsPimple->itsCentralLatitude = theLatitude;
}
// ----------------------------------------------------------------------
/*!
 * Set the true latitude of the projection.
 *
 * \param theLatitude The latitude
 */
// ----------------------------------------------------------------------

void Projection::trueLatitude(float theLatitude)
{
  itsPimple->itsTrueLatitude = theLatitude;
}
// ----------------------------------------------------------------------
/*!
 * Set the bottom left coordinates of the projection
 *
 * \param theLon The longitude
 * \param theLat The latitude
 */
// ----------------------------------------------------------------------

void Projection::bottomLeft(float theLon, float theLat)
{
  itsPimple->itsBottomLeft = NFmiPoint(theLon, theLat);
}

// ----------------------------------------------------------------------
/*!
 * Set the top right coordinates of the projection
 *
 * \param theLon The longitude
 * \param theLat The latitude
 */
// ----------------------------------------------------------------------

void Projection::topRight(float theLon, float theLat)
{
  itsPimple->itsTopRight = NFmiPoint(theLon, theLat);
}

// ----------------------------------------------------------------------
/*!
 * Set the center coordinates of the projection
 *
 * \param theLon The longitude
 * \param theLat The latitude
 */
// ----------------------------------------------------------------------

void Projection::center(float theLon, float theLat)
{
  itsPimple->itsCenter = NFmiPoint(theLon, theLat);
}

// ----------------------------------------------------------------------
/*!
 * Set the scale of the projection (when center is also specified).
 *
 * \param theScale the scale
 */
// ----------------------------------------------------------------------

void Projection::scale(float theScale)
{
  itsPimple->itsScale = theScale;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the ellipsoid
 */
// ----------------------------------------------------------------------

void Projection::ellipsoid(const std::string& theEllipsoid)
{
  itsPimple->itsEllipsoid = theEllipsoid;
}

// ----------------------------------------------------------------------
/*!
 * The projection service provided by the class.
 *
 * This method projects the given path using the internal projection
 * settings into to the bounds of the given NFmiImage.
 *
 * If the image is empty, a std::runtime_error exception is thrown.
 *
 * \param thePath The path to project
 * \param theImage The image whose bounds define the pixel projection
 * \return The projected path
 */
// ----------------------------------------------------------------------

NFmiPath Projection::project(const NFmiPath& thePath, const NFmiImage& theImage) const
{
  // First check validity

  if (theImage.Width() <= 0 || theImage.Height() <= 0)
    throw std::runtime_error("Cannot project onto an empty image");

  // Create the required projection

  boost::shared_ptr<NFmiArea> proj = area(theImage.Width(), theImage.Height());

  // Then perform the projection

  NFmiPath out = thePath;
  out.Project(proj.get());
  return out;
}

// ----------------------------------------------------------------------
/*!
 * Return the newbase projection
 *
 * \param theWidth The width
 * \param theHeight The height
 * \return The projection
 */
// ----------------------------------------------------------------------

boost::shared_ptr<NFmiArea> Projection::area(unsigned int theWidth, unsigned int theHeight) const
{
  // Create the required projection

  boost::shared_ptr<NFmiArea> proj;

  // Special handling for the center

  bool has_center =
      (itsPimple->itsCenter.X() != kFloatMissing && itsPimple->itsCenter.Y() != kFloatMissing);

  NFmiPoint bottomleft = has_center ? itsPimple->itsCenter : itsPimple->itsBottomLeft;
  NFmiPoint topright = has_center ? itsPimple->itsCenter : itsPimple->itsTopRight;

  NFmiPoint topleftxy = NFmiPoint(0, 0);
  NFmiPoint bottomrightxy = NFmiPoint(theWidth, theHeight);

#ifdef WGS84
  std::string sphere = itsPimple->itsEllipsoid;

  if (sphere.empty())
    throw std::runtime_error("Reference ellipsoid not set");

  std::string proj4;

  if (itsPimple->itsType == "latlon")
    proj4 = sphere;

  else if (itsPimple->itsType == "ykj")
  {
    proj4 =
        "+proj=tmerc +lat_0=0 +lon_0=27 +k=1 +x_0=3500000 +y_0=0 +ellps=intl +units=m +wktext "
        "+towgs84=-96.0617,-82.4278,-121.7535,4.80107,0.34543,-1.37646,1.4964 +no_defs";
    sphere = "+proj=longlat +ellps=intl +no_defs";
  }

  else if (itsPimple->itsType == "stereographic")
  {
    if (sphere == "FMI")
      proj4 = fmt::format(
          "+proj=stere +lat_0={} +lat_ts={} +lon_0={} +k=1 +x_0=0 +y_0=0 +R={:.0f} "
          "+units=m +wktext +towgs84=0,0,0 +no_defs",
          itsPimple->itsCentralLatitude,
          itsPimple->itsTrueLatitude,
          itsPimple->itsCentralLongitude,
          kRearth);
    proj4 = fmt::format(
        "+proj=stere +lat_0={} +lat_ts={} +lon_0={} +k=1 +x_0=0 +y_0=0 +ellps={} "
        "+units=m +wktext +towgs84=0,0,0 +no_defs",
        itsPimple->itsCentralLatitude,
        itsPimple->itsTrueLatitude,
        itsPimple->itsCentralLongitude,
        sphere);
  }
  else if (itsPimple->itsType == "equidist")
  {
    if (sphere == "FMI")
      proj4 = fmt::format(
          "+proj=aeqd +lat_0={} +lon_0={} +x_0=0 +y_0=0 +R={:.0f} +units=m +wktext "
          "+towgs84=0,0,0 +no_defs",
          itsPimple->itsCentralLatitude,
          itsPimple->itsCentralLongitude,
          kRearth);
    else
      proj4 = fmt::format(
          "+proj=aeqd +lat_0={} +lon_0={} +x_0=0 +y_0=0 +ellps={} +units=m +wktext "
          "+towgs84=0,0,0 +no_defs",
          itsPimple->itsCentralLatitude,
          itsPimple->itsCentralLongitude,
          sphere);
  }
  else
  {
    std::string msg = "Unrecognized projection type ";
    msg += itsPimple->itsType;
    msg += " in Projection::project()";
    throw std::runtime_error(msg);
  }

  if (!has_center)
    proj.reset(NFmiArea::CreateFromCorners(proj4, sphere, bottomleft, topright));
  else
  {
    // 2 is for a handling legacy coding mistake
    auto pscale = 2 * 1000 * itsPimple->itsScale;
    proj.reset(NFmiArea::CreateFromCenter(
        proj4, sphere, itsPimple->itsCenter, pscale * theWidth, pscale * theHeight));
  }

  proj->SetXYArea(NFmiRect(topleftxy, bottomrightxy));

#else
  if (itsPimple->itsType == "latlon")
    proj.reset(new NFmiLatLonArea(bottomleft, topright, topleftxy, bottomrightxy));

  else if (itsPimple->itsType == "ykj")
    proj.reset(new NFmiYKJArea(bottomleft, topright, topleftxy, bottomrightxy));

  else if (itsPimple->itsType == "mercator")
    proj.reset(new NFmiMercatorArea(bottomleft, topright, topleftxy, bottomrightxy));

  else if (itsPimple->itsType == "stereographic")
    proj.reset(new NFmiStereographicArea(bottomleft,
                                         topright,
                                         itsPimple->itsCentralLongitude,
                                         topleftxy,
                                         bottomrightxy,
                                         itsPimple->itsCentralLatitude,
                                         itsPimple->itsTrueLatitude));
  else if (itsPimple->itsType == "gnomonic")
    proj.reset(new NFmiGnomonicArea(bottomleft,
                                    topright,
                                    itsPimple->itsCentralLongitude,
                                    topleftxy,
                                    bottomrightxy,
                                    itsPimple->itsCentralLatitude,
                                    itsPimple->itsTrueLatitude));

  else if (itsPimple->itsType == "equidist")
    proj.reset(new NFmiEquidistArea(bottomleft,
                                    topright,
                                    itsPimple->itsCentralLongitude,
                                    topleftxy,
                                    bottomrightxy,
                                    itsPimple->itsCentralLatitude));
  else
  {
    std::string msg = "Unrecognized projection type ";
    msg += itsPimple->itsType;
    msg += " in Projection::project()";
    throw std::runtime_error(msg);
  }

  // Recalculate topleft and bottom right if center was set
  if (has_center)
  {
    const float pscale = 1000 * itsPimple->itsScale;
    const NFmiPoint c = proj->LatLonToWorldXY(itsPimple->itsCenter);
    const NFmiPoint bl(c.X() - pscale * theWidth, c.Y() - pscale * theHeight);
    const NFmiPoint tr(c.X() + pscale * theWidth, c.Y() + pscale * theHeight);

    const NFmiPoint BL = proj->WorldXYToLatLon(bl);
    const NFmiPoint TR = proj->WorldXYToLatLon(tr);

    proj.reset(proj->NewArea(BL, TR));
  }

#endif

  return proj;
}
}  // namespace RadContour
}  // namespace FMI

// ======================================================================
