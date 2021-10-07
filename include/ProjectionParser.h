// ======================================================================
/*!
 * \file
 * \brief Interface of class ProjectionParser
 */
// ======================================================================
/*!
 * \class FMI::RadContour::ProjectionParser
 *
 * The responsibility of this class is to read a Projection specification
 * from the given input stream and store it into the given ProjectionStore.
 *
 * If the specification is incomplete or incorrect, a std::runtime_error
 * is thrown.
 *
 * A projection specification contains a subset of the following tokens
 * \code
 * [name]
 * {
 *   type [name]
 *   centrallatitude [lat]
 *   centrallongitude [lon]
 *   truelatitude [lat]
 *   bottomleft [lon] [lat]
 *   topright [lon] [lat]
 *   center [lon] [lat]
 *   scale [scale]
 * }
 * \endcode
 *
 * The recognized projection types are
 *   - latlon
 *   - stereographic
 *   - ykj
 *   - equidist
 *   - mercator
 *   - gnomonic
 *
 * All projections require either the bottomleft and topright commands
 * or the center and scale commands. latlon, ykj and mercator require
 * nothing else. The others require atleast central longitude and latitude,
 * gnomonic and stereographic also the true latitude.
 */
// ======================================================================

#pragma once

#include <iosfwd>

namespace FMI
{
namespace RadContour
{
class ProjectionStore;

//! Parse a projection
class ProjectionParser
{
 public:
  static void parse(std::istream& is, ProjectionStore& theStore, const std::string& theEllipsoid);

 private:
  ~ProjectionParser() = delete;
  ProjectionParser() = delete;

};  // class ProjectionParser
}  // namespace RadContour
}  // namespace FMI

// ======================================================================
