// ======================================================================
/*!
 * \file
 * \brief Interface of the QueryDataManager class
 */
// ======================================================================
/*!
 * \class QueryDataManager
 *
 * The responsibility of the QueryDataManager is to manage multiple
 * querydata sources simultaneously, and to return the queryinfo
 * containing the desired station number or coordinate.
 *
 */
// ======================================================================

#ifndef QUERYDATAMANAGER_H
#define QUERYDATAMANAGER_H

#include <boost/tuple/tuple.hpp>
#include <newbase/NFmiFastQueryInfo.h>

#include <map>
#include <set>
#include <string>
#include <vector>

class QueryDataManager
{
 public:
  ~QueryDataManager();
  QueryDataManager();

  void multimode() { itsMultiMode = true; }
  std::set<int> stations();

  void searchpath(const std::string &theSearchPath);
  void addfile(const std::string &theFile);
  void addfiles(const std::vector<std::string> &theFiles);

  void setstation(int theWmoNumber);
  void setpoint(const NFmiPoint &theLonLat, double theMaxDistance);

  bool isset() const;
  NFmiFastQueryInfo &info() const;

  std::map<double, int> nearest(const NFmiPoint &theLonLat,
                                int theMaxNumber,
                                double theMaxDistance,
                                bool theCheckingFlag);

 private:
  QueryDataManager(const QueryDataManager &theManager);
  QueryDataManager &operator=(const QueryDataManager &theManager);

  std::string itsSearchPath;
  bool itsMultiMode;

  using value_type = boost::tuple<std::string, NFmiQueryData *, NFmiFastQueryInfo *>;

  using storage_type = std::vector<value_type>;
  storage_type itsData;
  storage_type::const_iterator itsCurrentData;

  NFmiFastQueryInfo &require(storage_type::iterator it);

};  // class QueryDataManager

#endif  // QUERYDATAMANAGER_H

// ======================================================================
