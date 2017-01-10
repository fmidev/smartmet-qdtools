// ======================================================================
/*!
 * \file
 * \brief Interface of the ProjectionStore class
 */
// ======================================================================
/*!
 * \class FMI::RadContour::ProjectionStore
 *
 * The responsibility of this class is to provide named projection services.
 *
 */
// ----------------------------------------------------------------------

#ifndef FMI_RADCONTOUR_PROJECTIONSTORE_H
#define FMI_RADCONTOUR_PROJECTIONSTORE_H

#include <boost/shared_ptr.hpp>
#include <set>
#include <string>

namespace FMI
{
namespace RadContour
{
struct ProjectionStorePimple;
class Projection;

//! Store named projections
class ProjectionStore
{
 public:
  ~ProjectionStore(void);
  ProjectionStore(void);

  void add(const std::string& theName, const Projection& theProjection);

  std::set<std::string> names(void) const;

  const Projection& projection(const std::string& theName) const;

 private:
  ProjectionStore(const ProjectionStore& theStore);
  ProjectionStore& operator=(const ProjectionStore& theStore);

  boost::shared_ptr<ProjectionStorePimple> itsPimple;

};  // class ProjectionStore
}  // namespace RadContour
}  // namespace FMI

#endif  // FMI_RADCONTOUR_PROJECTIONSTORE_H

// ======================================================================
