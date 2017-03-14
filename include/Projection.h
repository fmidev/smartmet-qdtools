// ======================================================================
/*!
 * \file
 * \brief Interface of the Projection class
 */
// ======================================================================
/*!
 * \class FMI::RadContour::Projection
 *
 * The responsibility of this class is to provide projection services
 * for NFmiPath objects onto the given image.
 *
 * In detail, this class checks the given image dimensions, modifies
 * the internally stored projection accordingly, and then projects
 * the given path so that the area specified in the projection
 * matches exactly the boundaries of the image.
 *
 * The alternative way to implement this would have been to require
 * the user to explicitly create the projection with knowledge of
 * the image size to be used. This is not always convenient in
 * scripted environments due to the execution order of things.
 * Hence this solution, which will automatically check the image
 * dimensions at the precise moment when it is needed - right
 * before rendering a path to the image.
 *
 * Note that it is not possible to create a projection using the
 * center and scale only without knowing the width and height also.
 * Hence we use an approach which stores the projection specification
 * in name only, and then we construct the actual projection on demand
 * when the width and height are known for certain.
 */
// ----------------------------------------------------------------------

#ifndef FMI_RADCONTOUR_PROJECTION_H
#define FMI_RADCONTOUR_PROJECTION_H

#include <newbase/NFmiArea.h>
#include <boost/shared_ptr.hpp>
#include <string>

namespace Imagine
{
class NFmiImage;
class NFmiPath;
}

namespace FMI
{
namespace RadContour
{
struct ProjectionPimple;

//! Define a projection
class Projection
{
 public:
  ~Projection(void);
  Projection(void);
  Projection(const Projection& theProjection);
  Projection& operator=(const Projection& theProjection);

  void type(const std::string& theType);
  void centralLatitude(float theLatitude);
  void centralLongitude(float theLongitude);
  void trueLatitude(float theLongitude);

  void bottomLeft(float theLon, float theLat);
  void topRight(float theLon, float theLat);

  void center(float theLon, float theLat);
  void scale(float theScale);

  boost::shared_ptr<NFmiArea> area(unsigned int theWidth, unsigned int theHeight) const;
  Imagine::NFmiPath project(const Imagine::NFmiPath& thePath,
                            const Imagine::NFmiImage& theImage) const;

 private:
  boost::shared_ptr<ProjectionPimple> itsPimple;

};  // class Projection
}  // namespace RadContour
}  // namespace FMI

#endif  // FMI_RADCONTOUR_PROJECTION_H

// ======================================================================
