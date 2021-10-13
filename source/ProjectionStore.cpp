// ======================================================================
/*!
 * \file
 * \brief Implementation of class ProjectionStore
 */
// ======================================================================

#include "ProjectionStore.h"
#include "Projection.h"
#include <imagine/NFmiPath.h>
#include <newbase/NFmiArea.h>
#include <map>
#include <stdexcept>

namespace FMI
{
namespace RadContour
{
typedef std::map<std::string, Projection> NamedProjections;

// ----------------------------------------------------------------------
/*!
 * Implementation hiding pimple for class ProjectionStore
 */
// ----------------------------------------------------------------------

//! Implementation hiding pimple for class ProjectionStore
struct ProjectionStorePimple
{
  ProjectionStorePimple(void) : itsStore() {}
  NamedProjections itsStore;
};

// ----------------------------------------------------------------------
/*!
 * The destructor does nothing special
 */
// ----------------------------------------------------------------------

ProjectionStore::~ProjectionStore(void) {}
// ----------------------------------------------------------------------
/*!
 * The void constructor merely initializes the implementation pimple
 */
// ----------------------------------------------------------------------

ProjectionStore::ProjectionStore(void) : itsPimple(new ProjectionStorePimple()) {}
// ----------------------------------------------------------------------
/*!
 * Add a new named Projection.
 *
 * The method throws std::runtime_error if the name has already been
 * used.
 *
 * \param theName The name to assign to the projection
 * \param theProjection The projection to name
 */
// ----------------------------------------------------------------------

void ProjectionStore::add(const std::string& theName, const Projection& theProjection)
{
  std::pair<NamedProjections::iterator, bool> result;
  result = itsPimple->itsStore.insert(make_pair(theName, theProjection));

  if (!result.second)
  {
    std::string msg = "Trying to add projection specification named ";
    msg += theName;
    msg += " twice";
    throw std::runtime_error(msg);
  }
}

// ----------------------------------------------------------------------
/*!
 * Return the unique names stored in the object. Note that we explicitly
 * return a set to indicate that the names are unique. Iterating over
 * the set will perhaps be slightly slower than over a list or a vector,
 * but it is insignificant compared to the interface guarantee of unique
 * names.
 *
 * \return The stored names.
 */
// ----------------------------------------------------------------------

std::set<std::string> ProjectionStore::names(void) const
{
  std::set<std::string> result;

  for (NamedProjections::const_iterator it = itsPimple->itsStore.begin();
       it != itsPimple->itsStore.end();
       ++it)
    result.insert(it->first);

  return result;
}

// ----------------------------------------------------------------------
/*!
 * Return the desired projection. If the projection does not exist,
 * a std::runtime_error is thrown.
 *
 * \param theName The name assigned to the projection
 * \return The projection
 */
// ----------------------------------------------------------------------

const Projection& ProjectionStore::projection(const std::string& theName) const
{
  NamedProjections::const_iterator pos = itsPimple->itsStore.find(theName);
  if (pos == itsPimple->itsStore.end())
  {
    std::string msg = "Accessing Projection ";
    msg += theName;
    msg += " from ProjectionStore when its not there";
    throw std::runtime_error(msg);
  }
  return pos->second;
}
}  // namespace RadContour
}  // namespace FMI

// ======================================================================
