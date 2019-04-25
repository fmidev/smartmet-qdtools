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
  ProjectionPimple()
      : itsType(""),
        itsCentralLatitude(kFloatMissing),
        itsCentralLongitude(kFloatMissing),
        itsTrueLatitude(kFloatMissing),
        itsBottomLeft(NFmiPoint(kFloatMissing, kFloatMissing)),
        itsTopRight(NFmiPoint(kFloatMissing, kFloatMissing)),
        itsCenter(NFmiPoint(kFloatMissing, kFloatMissing)),
        itsScale(kFloatMissing)
  {
  }

  std::string itsType;
  float itsCentralLatitude;
  float itsCentralLongitude;
  float itsTrueLatitude;
  NFmiPoint itsBottomLeft;
  NFmiPoint itsTopRight;
  NFmiPoint itsCenter;
  float itsScale;
};

// ----------------------------------------------------------------------
/*!
 * The destructor does nothing special.
 */
// ----------------------------------------------------------------------

Projection::~Projection() {}
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

void Projection::type(const std::string& theType) { itsPimple->itsType = theType; }
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

void Projection::centralLatitude(float theLatitude) { itsPimple->itsCentralLatitude = theLatitude; }
// ----------------------------------------------------------------------
/*!
 * Set the true latitude of the projection.
 *
 * \param theLatitude The latitude
 */
// ----------------------------------------------------------------------

void Projection::trueLatitude(float theLatitude) { itsPimple->itsTrueLatitude = theLatitude; }
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

void Projection::scale(float theScale) { itsPimple->itsScale = theScale; }
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

  std::string sphere = "FMI";
  std::string proj4;

  if (itsPimple->itsType == "latlon")
    proj4 = "FMI";

  else if (itsPimple->itsType == "ykj")
  {
    proj4 =
        "+proj=tmerc +lat_0=0 +lon_0=27 +k=1 +x_0=3500000 +y_0=0 +ellps=intl +units=m +wktext "
        "+towgs84=-96.0617,-82.4278,-121.7535,4.80107,0.34543,-1.37646,1.4964 +no_defs";
    sphere = "+proj=longlat +ellps=intl +no_defs";
  }

  else if (itsPimple->itsType == "stereographic")
  {
    proj4 = fmt::format(
        "+proj=stere +lat_0={} +lat_ts={} +lon_0={} +k=1 +x_0=0 +y_0=0 +R={:.0f} "
        "+units=m +wktext +towgs84=0,0,0 +no_defs",
        itsPimple->itsCentralLatitude,
        itsPimple->itsTrueLatitude,
        itsPimple->itsCentralLongitude,
        kRearth);
  }
  else if (itsPimple->itsType == "equidist")
  {
    proj4 = fmt::format(
        "+proj=aeqd +lat_0={} +lon_0={} +x_0=0 +y_0=0 +R={:.0f} +units=m +wktext "
        "+towgs84=0,0,0 +no_defs",
        itsPimple->itsCentralLatitude,
        itsPimple->itsCentralLongitude,
        kRearth);
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

  return proj;
}
}  // namespace RadContour
}  // namespace FMI

// ======================================================================
