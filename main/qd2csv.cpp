// ======================================================================
/*!
 * \file
 * \brief Implementation of qd2csv program
 */
// ======================================================================
/*!
 * \page qd2csv qd2csv
 *
 * A program to print point QD data in CSV format
 *
 * Usage:
 * \code
 * qd2csv <querydata>
 * \endcode
 *
 * The times are printed in UTC time.
 */
// ======================================================================

#include <iostream>
#include <sstream>
#include <stdexcept>

#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiStreamQueryData.h>

using namespace std;

// ----------------------------------------------------------------------
/*!
 * \brief Test if the parameter & level combination has valid values
 */
// ----------------------------------------------------------------------

bool goodcolumn(NFmiFastQueryInfo& theQ)
{
  for (theQ.ResetLocation(); theQ.NextLocation();)
    for (theQ.ResetTime(); theQ.NextTime();)
      if (theQ.FloatValue() != kFloatMissing) return true;
  return false;
}

// ----------------------------------------------------------------------
/*!
 * \brief Build variable name from parameter and level
 */
// ----------------------------------------------------------------------

const string makename(const NFmiFastQueryInfo& theQ)
{
  NFmiEnumConverter converter;

  ostringstream name;
  name << converter.ToString(theQ.Param().GetParamIdent());

  const NFmiLevel& level = *theQ.Level();
  switch (level.LevelTypeId())
  {
    case kFmiGroundSurface:
      break;
    case kFmiPressureLevel:
      name << "_P" << level.LevelValue();
      break;
    case kFmiMeanSeaLevel:
      name << "_M" << level.LevelValue();
      break;
    case kFmiAltitude:
      name << "_A" << level.LevelValue();
      break;
    case kFmiHeight:
      name << "_H" << level.LevelValue();
      break;
    case kFmiHybridLevel:
      name << "_L" << level.LevelValue();
      break;
    case kFmi:
    case kFmiAnyLevelType:
      break;
  }
  return name.str();
}

// ----------------------------------------------------------------------
/*!
 * \brief Main algorithm
 */
// ----------------------------------------------------------------------

int domain(int argc, const char* argv[])
{
  if (argc == 1)
  {
    cout << "Usage: qd2csv <querydata>" << endl;
    return 0;
  }
  else if (argc != 2)
    throw runtime_error("Expecting one querydata argument");

  // Read the querydata

  const string filename = argv[1];

  NFmiStreamQueryData qd;
  if (!qd.SafeReadLatestData(filename))
    throw runtime_error("Failed to read querydata from '" + filename + "'");

  NFmiFastQueryInfo* q = qd.QueryInfoIter();

  // Print the data columns. We print one station at a time,
  // all levels and parameters in a single row

  // We omit columns with missing values only

  vector<bool> okvariables;

  // First the headers

  NFmiEnumConverter converter;
  cout << "\"id\",\"date\"";
  for (q->ResetLevel(); q->NextLevel();)
    for (q->ResetParam(); q->NextParam();)
    {
      bool flag = goodcolumn(*q);
      okvariables.push_back(flag);

      if (flag)
      {
        string name = makename(*q);
        cout << ",\"" << name << '"';
      }
    }
  cout << endl;

  // Then the data columns

  for (q->ResetLocation(); q->NextLocation();)
  {
    for (q->ResetTime(); q->NextTime();)
    {
      NFmiString timestr = q->ValidTime().ToStr(kYYYYMMDDHH);

      int column = 0;
      int printedcolumns = 0;
      for (q->ResetLevel(); q->NextLevel();)
        for (q->ResetParam(); q->NextParam();)
        {
          if (okvariables[column])
          {
            if (++printedcolumns == 1)
              cout << q->Location()->GetIdent() << ',' << timestr.CharPtr();
            cout << ',';
            float value = q->FloatValue();
            if (value == kFloatMissing)
              cout << "NA";
            else
              cout << value;
          }
          ++column;
        }
      if (printedcolumns > 0) cout << endl;
    }
  }

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program
 */
// ----------------------------------------------------------------------

int main(int argc, const char* argv[])
{
  try
  {
    return domain(argc, argv);
  }
  catch (exception& e)
  {
    cout << "Error: " << e.what() << endl;
    return 1;
  }
  catch (...)
  {
    cout << "Error: Caught an unknown exception" << endl;
    return 1;
  }
}
