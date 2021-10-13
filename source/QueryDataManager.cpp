// ======================================================================
/*!
 * \file
 * \brief Implementation of the QueryDataManager class
 */
// ======================================================================
#ifdef _MSC_VER
#pragma warning(disable : 4786)  // poistaa n kpl VC++ kääntäjän varoitusta (liian pitkä nimi >255
                                 // merkkiä joka johtuu 'puretuista' STL-template nimistä)
#endif

#include "QueryDataManager.h"

#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiQueryData.h>

#include <sstream>
#include <stdexcept>

namespace
{
// ----------------------------------------------------------------------
/*!
 * \brief Tests whether the data for the set location is OK
 *
 * The data is considered OK, if any time and parameter combination
 * has a valid value in the data. The purpose is to detect
 * stations which are not operational so that the user may
 * get observations from some second nearest station.
 */
// ----------------------------------------------------------------------

bool locationvalid(NFmiFastQueryInfo& theQD)
{
  const bool ignoresubparameters = false;

  theQD.FirstLevel();

  theQD.ResetTime();
  while (theQD.NextTime())
  {
    theQD.ResetParam();
    while (theQD.NextParam(ignoresubparameters))
    {
      if (theQD.FloatValue() != kFloatMissing) return true;
    }
  }
  return false;
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 *
 * Default constructor does nothing special
 */
// ----------------------------------------------------------------------

QueryDataManager::QueryDataManager() : itsSearchPath(), itsData(), itsCurrentData()
{
  itsCurrentData = itsData.end();
}

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 *
 * The destructor explicitly destroys all internal NFmiStreamQueryData
 * objects.
 */
// ----------------------------------------------------------------------

QueryDataManager::~QueryDataManager()
{
  for (storage_type::iterator it = itsData.begin(); it != itsData.end(); ++it)
  {
    delete it->get<1>();  // querydata
    delete it->get<2>();  // fastqueryinfo
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the data search path
 *
 * The search path is a :-separated Unix-style search path for
 * querydata files. The search path will be passed to the
 * newbase FileComplete function when needed.
 *
 * If a search path has already been set, the new paths will be
 * appended to the end of the existing search path.
 *
 * \param theSearchPath The search path
 */
// ----------------------------------------------------------------------

void QueryDataManager::searchpath(const std::string& theSearchPath)
{
  if (itsSearchPath.empty())
    itsSearchPath = theSearchPath;
  else
    itsSearchPath += ':' + theSearchPath;
}

// ----------------------------------------------------------------------
/*!
 * \brief Add a querydata file to the set of files
 *
 * \param theFile The name of the file
 */
// ----------------------------------------------------------------------

void QueryDataManager::addfile(const std::string& theFile)
{
  itsData.push_back(value_type(theFile, 0));
}

// ----------------------------------------------------------------------
/*!
 * \brief Add querydata files to the set of files
 *
 * \param theFiles The list of file names
 */
// ----------------------------------------------------------------------

void QueryDataManager::addfiles(const std::vector<std::string>& theFiles)
{
  for (std::vector<std::string>::const_iterator it = theFiles.begin(); it != theFiles.end(); ++it)
  {
    addfile(*it);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the location has been set
 *
 * \return True, if a location has been set
 */
// ----------------------------------------------------------------------

bool QueryDataManager::isset(void) const { return (itsCurrentData != itsData.end()); }
// ----------------------------------------------------------------------
/*!
 * \brief Return the current query info
 *
 * The current querydata is set by calling set with a WMO-number
 * or with a longitude-latitude pair.
 *
 * The method will throw if there is no active query data.
 *
 * \return The query info
 */
// ----------------------------------------------------------------------

NFmiFastQueryInfo& QueryDataManager::info(void) const
{
  if (isset()) return *itsCurrentData->get<2>();

  throw std::runtime_error("Trying to access querydata before setting a location");
}

// ----------------------------------------------------------------------
/*!
 * \brief Set location based on a WMO-number
 *
 * This will throw if the WMO-number does not exist in any data file.
 *
 * \param theWmoNumber The WMO-number of the station
 */
// ----------------------------------------------------------------------

void QueryDataManager::setstation(int theWmoNumber)
{
  // Set "no data" condition until we've found a station
  itsCurrentData = itsData.end();

  for (storage_type::iterator it = itsData.begin(); it != itsData.end(); ++it)
  {
    if (!it->get<1>())
    {
      std::string filename = NFmiFileSystem::FileComplete(it->get<0>(), itsSearchPath);
      NFmiQueryData* qd = new NFmiQueryData(filename);
      it->get<1>() = qd;
      it->get<2>() = new NFmiFastQueryInfo(qd);
    }

    if (it->get<2>()->Location(theWmoNumber))
    {
      itsCurrentData = it;
      return;
    }
  }
  std::ostringstream os;
  os << "Station number " << theWmoNumber << " is not available";
  throw std::runtime_error(os.str());
}

// ----------------------------------------------------------------------
/*!
 * \brief Set location based on geographic coordinates
 *
 * This will throw if the coordinates are not available in any data file.
 *
 * \param theLonLat The coordinate
 * \param theMaxDistance The maximum distance
 */
// ----------------------------------------------------------------------

void QueryDataManager::setpoint(const NFmiPoint& theLonLat, double theMaxDistance)
{
  // Set "no data" condition until we've found the coordinate
  itsCurrentData = itsData.end();

  double smallest_distance = -1;

  for (storage_type::iterator it = itsData.begin(); it != itsData.end(); ++it)
  {
    if (!it->get<1>())
    {
      std::string filename = NFmiFileSystem::FileComplete(it->get<0>(), itsSearchPath);
      NFmiQueryData* qd = new NFmiQueryData(filename);
      it->get<1>() = qd;
      it->get<2>() = new NFmiFastQueryInfo(qd);
    }

    NFmiFastQueryInfo& qi = *(it->get<2>());

    if (qi.NearestLocation(theLonLat, theMaxDistance))
    {
      itsCurrentData = it;
      return;
    }

    qi.NearestPoint(theLonLat);
    double distance = qi.Location()->Distance(theLonLat);
    if (smallest_distance < 0)
      smallest_distance = distance;
    else
      smallest_distance = std::min(smallest_distance, distance);
  }
  std::ostringstream os;
  os << "Coordinate (" << theLonLat.X() << ',' << theLonLat.Y() << ") is too far ("
     << smallest_distance / 1000 << " km) from the data";
  throw std::runtime_error(os.str());
}

// ----------------------------------------------------------------------
/*!
 * \brief Return set of all stations
 *
 * Returns a set containing all available WMO-numbers
 *
 * \return The set of stations
 */
// ----------------------------------------------------------------------

std::set<int> QueryDataManager::stations()
{
  std::set<int> ret;

  for (storage_type::iterator it = itsData.begin(); it != itsData.end(); ++it)
  {
    if (!it->get<1>())
    {
      std::string filename = NFmiFileSystem::FileComplete(it->get<0>(), itsSearchPath);
      NFmiQueryData* qd = new NFmiQueryData(filename);
      it->get<1>() = qd;
      it->get<2>() = new NFmiFastQueryInfo(qd);
    }

    NFmiFastQueryInfo& qi = *(it->get<2>());

    qi.ResetLocation();
    while (qi.NextLocation())
    {
      int wmo = qi.Location()->GetIdent();
      ret.insert(wmo);
    }
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Find nearest stations
 *
 * Returns a list of WMO-numbers which are within the given maximum
 * distance from the given coordinate. The maximum number of returned
 * stations can be set.
 *
 * \param theLonLat The coordinate
 * \param theMaxNumber The maximum number of stations desired, or -1
 * \param theMaxDistance The maximum allowed distance, or -1
 * \param theCheckingFlag True, if station data must not be missing
 * \return A map of distances to WMO-numbers
 */
// ----------------------------------------------------------------------

std::map<double, int> QueryDataManager::nearest(const NFmiPoint& theLonLat,
                                                int theMaxNumber,
                                                double theMaxDistance,
                                                bool theCheckingFlag)
{
  typedef std::map<double, int> ReturnType;
  ReturnType ret;

  for (storage_type::iterator it = itsData.begin(); it != itsData.end(); ++it)
  {
    if (!it->get<1>())
    {
      std::string filename = NFmiFileSystem::FileComplete(it->get<0>(), itsSearchPath);
      NFmiQueryData* qd = new NFmiQueryData(filename);
      it->get<1>() = qd;
      it->get<2>() = new NFmiFastQueryInfo(qd);
    }

    NFmiFastQueryInfo& qi = *(it->get<2>());

    // Won't find nearest points from grids
    if (qi.IsGrid()) continue;

    qi.ResetLocation();
    while (qi.NextLocation())
    {
      int wmo = qi.Location()->GetIdent();
      double dist = qi.Location()->Distance(theLonLat);
      if (dist <= theMaxDistance)
        if (!theCheckingFlag || locationvalid(qi))
          ret.insert(std::map<double, int>::value_type(dist, wmo));
    }
  }

  if (theMaxNumber > 0 && ret.size() > static_cast<unsigned long>(theMaxNumber))
  {
    ReturnType::iterator it = ret.begin();
    // For some reason advance() won't compile with g++ v3.2
    // std::advance(ret,theMaxNumber);
    for (int i = 0; i < theMaxNumber; i++)
      ++it;
    ret.erase(it, ret.end());
  }

  return ret;
}

// ======================================================================
