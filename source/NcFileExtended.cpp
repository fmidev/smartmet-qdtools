// GDAL-only implementation of NcFileExtended
#include "NcFileExtended.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <macgyver/Exception.h>
#include <macgyver/NumericCast.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <algorithm>
#include <cmath>
#include <gdal.h>
#include <gdal_priv.h>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std::literals;

// ======================================================================
// Local helpers
// ======================================================================

namespace
{

// Parse GDAL's "{v1,v2,...}" or "v" format into a vector of doubles
std::vector<double> parse_gdal_array(const std::string& s)
{
  std::string val = s;
  if (val.size() >= 2 && val.front() == '{')
    val = val.substr(1, val.size() - 2);
  std::vector<double> result;
  std::istringstream ss(val);
  std::string token;
  while (std::getline(ss, token, ','))
  {
    boost::algorithm::trim(token);
    if (!token.empty())
    {
      try
      {
        result.push_back(std::stod(token));
      }
      catch (...)
      {
      }
    }
  }
  return result;
}

// Parse a brace-list like "{time,lev_soil}" or "time" into a vector of strings
std::vector<std::string> parse_brace_list(const std::string& s)
{
  std::string val = s;
  if (val.size() >= 2 && val.front() == '{')
    val = val.substr(1, val.size() - 2);
  std::vector<std::string> result;
  boost::algorithm::split(result, val, boost::algorithm::is_any_of(","));
  for (auto& item : result)
    boost::algorithm::trim(item);
  result.erase(std::remove_if(result.begin(), result.end(), [](const std::string& s) {
    return s.empty();
  }), result.end());
  return result;
}

bool IsMissingValue(float value, float ncMissingValue)
{
  const float extraMissingValueLimit = 9.99e034f;
  return value == ncMissingValue || value >= extraMissingValueLimit;
}

// Compare version strings like "1.6" vs "1.5"
int compare_versions(const std::string& v1, const std::string& v2)
{
  std::size_t v1pos = 0, v2pos = 0;
  while (v1pos < v1.size())
  {
    std::size_t v1pe = v1.find('.', v1pos);
    std::size_t v2pe = v2.find('.', v2pos);
    if (v1pe == std::string::npos)
      v1pe = v1.size();
    if (v2pe == std::string::npos)
      v2pe = v2.size();
    int p1 = 0, p2 = 0;
    try
    {
      p1 = Fmi::stoi(v1.substr(v1pos, v1pe - v1pos));
    }
    catch (...)
    {
      throw Fmi::Exception(BCP, "Unable to convert " + v1 + " to integer version parts");
    }
    try
    {
      p2 = Fmi::stoi(v2.substr(v2pos, v2pe - v2pos));
    }
    catch (...)
    {
      throw Fmi::Exception(BCP, "Unable to convert " + v2 + " to integer version parts");
    }
    if (p1 != p2)
      return p1 - p2;
    v1pos = v1pe + 1;
    v2pos = v2pe + 1;
  }
  if (v2pos < v2.size())
    return -1;
  return 0;
}

}  // anonymous namespace

// ======================================================================
// Free functions
// ======================================================================

NFmiMetTime nctools::tomettime(const Fmi::DateTime& t)
{
  try
  {
    return NFmiMetTime(static_cast<short>(t.date().year()),
                       static_cast<short>(t.date().month()),
                       static_cast<short>(t.date().day()),
                       static_cast<short>(t.time_of_day().hours()),
                       static_cast<short>(t.time_of_day().minutes()),
                       static_cast<short>(t.time_of_day().seconds()),
                       1);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

unsigned long nctools::get_units_in_seconds(std::string unit_str)
{
  try
  {
    if (unit_str == "day" || unit_str == "days" || unit_str == "d")
      return 86400;
    if (unit_str == "hour" || unit_str == "hours" || unit_str == "h")
      return 3600;
    if (unit_str == "minute" || unit_str == "minutes" || unit_str == "min" ||
        unit_str == "mins")
      return 60;
    if (unit_str == "second" || unit_str == "seconds" || unit_str == "sec" ||
        unit_str == "secs" || unit_str == "s")
      return 1;
    throw Fmi::Exception(BCP, "Invalid time unit used: " + unit_str);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void nctools::parse_time_units_string(const std::string& units,
                                      Fmi::DateTime* origintime,
                                      long* timeunit)
{
  try
  {
    std::vector<std::string> parts;
    boost::algorithm::split(parts, units, boost::algorithm::is_any_of(" "));

    if (parts.size() < 3 || parts.size() > 5)
      throw Fmi::Exception(BCP, "Invalid time units string: '" + units + "'");

    std::string unit = boost::algorithm::to_lower_copy(parts[0]);

    if (unit == "second" || unit == "seconds" || unit == "sec" || unit == "secs" || unit == "s")
      *timeunit = 1;
    else if (unit == "minute" || unit == "minutes" || unit == "min" || unit == "mins")
      *timeunit = 60;
    else if (unit == "hour" || unit == "hours" || unit == "hr" || unit == "h")
      *timeunit = 60 * 60;
    else if (unit == "day" || unit == "days" || unit == "d")
      *timeunit = 24 * 60 * 60;
    else
      throw Fmi::Exception(BCP, "Unknown unit in time axis: '" + unit + "'");

    if (boost::algorithm::to_lower_copy(parts[1]) != "since")
      throw Fmi::Exception(BCP, "Invalid time units string: '" + units + "'");

    std::string datestr = parts[2];
    std::string timestr = (parts.size() >= 4 ? parts[3] : "");

    if (timestr.empty() && datestr.find('T') == std::string::npos)
      timestr = "00:00:00";

    const std::string datetime_str =
        timestr.empty() ? datestr : (datestr + " " + timestr);
    *origintime = Fmi::TimeParser::parse(datetime_str);

    if (parts.size() == 5 && boost::iequals(parts[4], "UTC") == false)
      *origintime += Fmi::date_time::duration_from_string(parts[4]);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ======================================================================
// NcFileExtended implementation
// ======================================================================

nctools::NcFileExtended::NcFileExtended(std::string apath, long atimeshift)
    : path(apath), timeshift(atimeshift)
{
  gdal_dataset = GDALOpen(path.c_str(), GA_ReadOnly);
}

nctools::NcFileExtended::~NcFileExtended()
{
  if (gdal_dataset)
  {
    GDALClose(gdal_dataset);
    gdal_dataset = nullptr;
  }
}

// ======================================================================
// GDAL helpers
// ======================================================================

std::string nctools::NcFileExtended::open_var_ds_name(const std::string& varname) const
{
  if (!gdal_dataset)
    return {};

  // Try to find exact subdataset name from GDAL SUBDATASETS metadata
  char** subdatasets = GDALGetMetadata(gdal_dataset, "SUBDATASETS");
  if (subdatasets)
  {
    for (int i = 0; subdatasets[i]; i++)
    {
      const std::string item(subdatasets[i]);
      if (item.find("_NAME=") == std::string::npos)
        continue;
      const auto eq = item.find('=');
      if (eq == std::string::npos)
        continue;
      const std::string value = item.substr(eq + 1);
      // Check if it ends with :varname or /varname or //varname
      const std::string suffix1 = ":" + varname;
      const std::string suffix2 = "//" + varname;
      const std::string suffix3 = "/" + varname;
      if ((value.size() >= suffix1.size() &&
           value.substr(value.size() - suffix1.size()) == suffix1) ||
          (value.size() >= suffix2.size() &&
           value.substr(value.size() - suffix2.size()) == suffix2) ||
          (value.size() >= suffix3.size() &&
           value.substr(value.size() - suffix3.size()) == suffix3))
        return value;
    }
  }

  // Fallback: construct subdataset name based on file type
  // For HDF5 files (subdatasets start with HDF5:), use HDF5://varname format
  if (subdatasets)
  {
    for (int i = 0; subdatasets[i]; i++)
    {
      const std::string item(subdatasets[i]);
      if (item.find("_NAME=HDF5:") != std::string::npos)
        return "HDF5:\"" + path + "\"://" + varname;
    }
  }
  return "NETCDF:\"" + path + "\":" + varname;
}

GDALDatasetH nctools::NcFileExtended::open_var_dataset(const std::string& varname) const
{
  const std::string ds_name = open_var_ds_name(varname);
  if (ds_name.empty())
    return nullptr;
  // HONOUR_VALID_RANGE=NO: do not let GDAL mask values outside valid_range as NoData.
  // The caller is responsible for any valid_range filtering.
  const char* open_options[] = {"HONOUR_VALID_RANGE=NO", nullptr};
  return GDALOpenEx(ds_name.c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY, nullptr, open_options, nullptr);
}

void nctools::NcFileExtended::load_first_var_meta() const
{
  if (!first_var_name_.empty())
    return;  // already loaded

  const auto varnames = get_gdal_variable_names();
  if (varnames.empty())
  {
    // No subdatasets: single-variable file. Load metadata directly from top-level dataset.
    if (!gdal_dataset)
      return;
    char** meta = GDALGetMetadata(gdal_dataset, nullptr);
    if (!meta)
      return;
    // Determine variable name from metadata keys (e.g. "cnc_PM10#units" → "cnc_PM10")
    std::string vname;
    for (int i = 0; meta[i]; i++)
    {
      const std::string item(meta[i]);
      const auto hash = item.find('#');
      if (hash == std::string::npos)
        continue;
      const std::string cname = item.substr(0, hash);
      // Skip NC_GLOBAL, NETCDF_DIM_, and coordinate variable names
      if (cname.substr(0, 9) == "NC_GLOBAL" || cname.substr(0, 11) == "NETCDF_DIM_")
        continue;
      // The first non-coordinate, non-global key is likely the data variable
      vname = cname;
      break;
    }
    first_var_name_ = vname.empty() ? "unknown" : vname;
    for (int i = 0; meta[i]; i++)
    {
      const std::string item(meta[i]);
      const auto eq = item.find('=');
      if (eq == std::string::npos)
        continue;
      first_var_meta_[item.substr(0, eq)] = item.substr(eq + 1);
    }
    _xsize = static_cast<unsigned long>(GDALGetRasterXSize(gdal_dataset));
    _ysize = static_cast<unsigned long>(GDALGetRasterYSize(gdal_dataset));
    return;
  }

  // Helper: load metadata from a GDAL dataset into first_var_meta_ and cache spatial size
  auto load_from_ds = [&](const std::string& vname, GDALDatasetH ds)
  {
    first_var_name_ = vname;
    char** meta = GDALGetMetadata(ds, nullptr);
    for (int i = 0; meta && meta[i]; i++)
    {
      const std::string item(meta[i]);
      const auto eq = item.find('=');
      if (eq == std::string::npos)
        continue;
      first_var_meta_[item.substr(0, eq)] = item.substr(eq + 1);
    }
    _xsize = static_cast<unsigned long>(GDALGetRasterXSize(ds));
    _ysize = static_cast<unsigned long>(GDALGetRasterYSize(ds));
    GDALClose(ds);
  };

  // Pass 1: prefer variables with >= 3 dims in SUBDATASET_DESC (skip 2D coord/bounds vars)
  // Coordinate variables (lon, lat, longitude, latitude) and bounds variables (_bnds) are
  // typically 2D in SUBDATASET_DESC even on curvilinear grids. Real data variables have
  // at least 3 dims (time × y × x or lev × y × x).
  for (const auto& vname : varnames)
  {
    const std::string desc = get_subdataset_desc(vname);
    if (desc.empty())
      continue;
    const auto dims = parse_desc_dims(desc);
    if (dims.size() < 3)
      continue;  // 2D: likely coordinate or bounds variable
    GDALDatasetH ds = open_var_dataset(vname);
    if (!ds)
      continue;
    load_from_ds(vname, ds);
    return;
  }

  // Pass 2: accept 2D variables with both dimensions > 1 (e.g. static 2D data)
  for (const auto& vname : varnames)
  {
    GDALDatasetH ds = open_var_dataset(vname);
    if (!ds)
      continue;
    const int xsz = GDALGetRasterXSize(ds);
    const int ysz = GDALGetRasterYSize(ds);
    if (xsz <= 1 || ysz <= 1)
    {
      GDALClose(ds);
      continue;
    }
    load_from_ds(vname, ds);
    return;
  }

  // Final fallback: use the first variable
  first_var_name_ = varnames.front();
  GDALDatasetH ds = open_var_dataset(first_var_name_);
  if (!ds)
    return;
  load_from_ds(first_var_name_, ds);
}

// Merge metadata from a subdataset that contains the specified dimension.
// Called when the user specifies a z-dim (e.g. --zdim lev) that is not present
// in the representative first variable loaded by load_first_var_meta().
void nctools::NcFileExtended::merge_dim_meta(const std::string& dimname) const
{
  if (dimname.empty())
    return;
  const std::string def_key = "NETCDF_DIM_" + dimname + "_DEF";
  if (!get_first_var_meta(def_key).empty())
    return;  // already have it

  const auto varnames = get_gdal_variable_names();
  for (const auto& vname : varnames)
  {
    GDALDatasetH ds = open_var_dataset(vname);
    if (!ds)
      continue;
    char** meta = GDALGetMetadata(ds, nullptr);
    bool has_dim = false;
    for (int i = 0; meta && meta[i]; i++)
    {
      const std::string item(meta[i]);
      if (item.find(def_key + "=") == 0 || item.find(def_key + " =") == 0 ||
          item.substr(0, def_key.size()) == def_key)
      {
        has_dim = true;
        break;
      }
    }
    if (has_dim)
    {
      // Merge all metadata entries not already present (coordinate attrs + NETCDF_DIM_)
      for (int i = 0; meta && meta[i]; i++)
      {
        const std::string item(meta[i]);
        const auto eq = item.find('=');
        if (eq == std::string::npos)
          continue;
        const std::string k = item.substr(0, eq);
        if (first_var_meta_.find(k) == first_var_meta_.end())
          first_var_meta_[k] = item.substr(eq + 1);
      }
      GDALClose(ds);
      return;
    }
    GDALClose(ds);
  }
}

std::string nctools::NcFileExtended::get_first_var_meta(const std::string& key) const
{
  const auto it = first_var_meta_.find(key);
  if (it != first_var_meta_.end())
    return it->second;
  return {};
}

std::string nctools::NcFileExtended::get_coord_attr(const std::string& coordname,
                                                     const std::string& attrname) const
{
  return get_first_var_meta(coordname + "#" + attrname);
}

std::vector<double> nctools::NcFileExtended::get_dim_values_double(
    const std::string& dimname) const
{
  const std::string key = "NETCDF_DIM_" + dimname + "_VALUES";
  const std::string raw = get_first_var_meta(key);
  if (raw.empty())
    return {};
  return parse_gdal_array(raw);
}

unsigned long nctools::NcFileExtended::compute_dim_size(const std::string& dimname) const
{
  // Try _DEF first: "{count,nctype}"
  const std::string def_key = "NETCDF_DIM_" + dimname + "_DEF";
  const std::string def_raw = get_first_var_meta(def_key);
  if (!def_raw.empty())
  {
    const auto vals = parse_gdal_array(def_raw);
    if (!vals.empty())
      return static_cast<unsigned long>(vals[0]);
  }
  // Fall back to counting VALUES entries
  const auto values = get_dim_values_double(dimname);
  if (!values.empty())
    return static_cast<unsigned long>(values.size());
  // HDF5 fallback: open the coordinate variable dataset and get its total raster size
  GDALDatasetH ds = open_var_dataset(dimname);
  if (ds)
  {
    const unsigned long sz = static_cast<unsigned long>(GDALGetRasterXSize(ds)) *
                             static_cast<unsigned long>(GDALGetRasterYSize(ds));
    GDALClose(ds);
    if (sz > 0)
      return sz;
  }
  return 0;
}

std::string nctools::NcFileExtended::get_subdataset_desc(const std::string& varname) const
{
  if (!gdal_dataset)
    return {};
  char** meta = GDALGetMetadata(gdal_dataset, "SUBDATASETS");
  if (!meta)
    return {};

  // Iterate; NAME entries have _NAME=, DESC entries have _DESC=
  // They come in pairs: SUBDATASET_N_NAME=... then SUBDATASET_N_DESC=...
  int matching_n = -1;
  for (int i = 0; meta[i]; i++)
  {
    const std::string item(meta[i]);
    const auto eq = item.find('=');
    if (eq == std::string::npos)
      continue;
    const std::string key = item.substr(0, eq);
    const std::string value = item.substr(eq + 1);
    if (key.find("_NAME") == std::string::npos)
      continue;
    // Check if value ends with :varname or //varname
    const std::string s1 = ":" + varname;
    const std::string s2 = "//" + varname;
    if ((value.size() >= s1.size() &&
         value.substr(value.size() - s1.size()) == s1) ||
        (value.size() >= s2.size() &&
         value.substr(value.size() - s2.size()) == s2))
    {
      // Extract N from SUBDATASET_N_NAME
      const auto name_pos = key.find("SUBDATASET_");
      const auto name_end = key.rfind('_');
      if (name_pos != std::string::npos && name_end > name_pos + 11)
      {
        try
        {
          matching_n = std::stoi(key.substr(name_pos + 11, name_end - name_pos - 11));
        }
        catch (...)
        {
        }
      }
      break;
    }
  }

  if (matching_n < 0)
    return {};

  const std::string desc_key = "SUBDATASET_" + std::to_string(matching_n) + "_DESC";
  for (int i = 0; meta[i]; i++)
  {
    const std::string item(meta[i]);
    const auto eq = item.find('=');
    if (eq == std::string::npos)
      continue;
    const std::string key = item.substr(0, eq);
    if (key == desc_key)
      return item.substr(eq + 1);
  }
  return {};
}

std::vector<long> nctools::NcFileExtended::parse_desc_dims(const std::string& desc) const
{
  // Format: "[d1xd2x...xdn] varname (type)" OR "[d1xd2] varname (type)"
  const auto lb = desc.find('[');
  const auto rb = desc.find(']');
  if (lb == std::string::npos || rb == std::string::npos || rb <= lb)
    return {};
  const std::string dims_str = desc.substr(lb + 1, rb - lb - 1);
  std::vector<std::string> parts;
  boost::algorithm::split(parts, dims_str, boost::algorithm::is_any_of("x"));
  std::vector<long> result;
  for (const auto& p : parts)
  {
    std::string t = p;
    boost::algorithm::trim(t);
    if (t.empty())
      continue;
    try
    {
      result.push_back(std::stol(t));
    }
    catch (...)
    {
      return {};
    }
  }
  return result;
}

double nctools::NcFileExtended::axis_scale_from_units(const std::string& units) const
{
  if (units == "100  km")
    return 100.0 * 1000.0;
  if (units == "km")
    return 1000.0;
  if (units == "m" || units == "Meter")
    return 1.0;
  return 1.0;
}

// ======================================================================
// get_gdal_variable_names
// ======================================================================

std::vector<std::string> nctools::NcFileExtended::get_gdal_variable_names() const
{
  try
  {
    if (!gdal_dataset)
      return {};

    char** subdatasets = GDALGetMetadata(gdal_dataset, "SUBDATASETS");
    if (!subdatasets)
    {
      // No subdatasets: single-variable file.
      // Find the variable name from the top-level metadata keys (e.g. "cnc_PM10#units").
      char** meta = GDALGetMetadata(gdal_dataset, nullptr);
      for (int i = 0; meta && meta[i]; i++)
      {
        const std::string item(meta[i]);
        const auto hash = item.find('#');
        if (hash == std::string::npos)
          continue;
        const std::string cname = item.substr(0, hash);
        if (cname.substr(0, 9) == "NC_GLOBAL" || cname.substr(0, 11) == "NETCDF_DIM_")
          continue;
        // Check it's not a coordinate variable (lat, lon, time, level names)
        const std::string lower = boost::algorithm::to_lower_copy(cname);
        if (lower == "lat" || lower == "latitude" || lower == "lon" || lower == "longitude" ||
            lower == "time" || lower == "level" || lower == "lev" || lower == "height" ||
            lower == "depth" || lower == "z")
          continue;
        return {cname};
      }
      return {};
    }

    std::vector<std::string> varnames;
    for (int i = 0; subdatasets[i] != nullptr; ++i)
    {
      const std::string item(subdatasets[i]);
      if (item.find("_NAME=") == std::string::npos)
        continue;
      const std::size_t colon_pos = item.rfind(':');
      if (colon_pos == std::string::npos)
        continue;
      std::string varname = item.substr(colon_pos + 1);
      if (varname.size() >= 2 && varname.front() == '"' && varname.back() == '"')
        varname = varname.substr(1, varname.size() - 2);
      if (varname.size() >= 2 && varname[0] == '/' && varname[1] == '/')
        varname = varname.substr(2);
      if (!varname.empty())
        varnames.push_back(varname);
    }
    return varnames;
  }
  catch (...)
  {
    return {};
  }
}

// ======================================================================
// initAxis
// ======================================================================

void nctools::NcFileExtended::initAxis(const std::optional<std::string>& xname,
                                       const std::optional<std::string>& yname,
                                       const std::optional<std::string>& zname,
                                       const std::optional<std::string>& tname)
{
  try
  {
    if (wrf)
    {
      // WRF mode: axis names come from options; just cache spatial sizes
      load_first_var_meta();

      if (options.xdim)
        x_name = *options.xdim;
      if (options.ydim)
        y_name = *options.ydim;
      if (options.zdim && !options.zdim->empty())
        z_name = *options.zdim;
      if (options.tdim)
        t_name = *options.tdim;

      // For WRF, xsize/ysize come from the first data variable raster
      // _xsize and _ysize were set in load_first_var_meta
      if (_xsize == 0 || _ysize == 0)
      {
        // Try opening first variable's dataset directly
        const auto varnames = get_gdal_variable_names();
        if (!varnames.empty())
        {
          GDALDatasetH ds = open_var_dataset(varnames.front());
          if (ds)
          {
            _xsize = static_cast<unsigned long>(GDALGetRasterXSize(ds));
            _ysize = static_cast<unsigned long>(GDALGetRasterYSize(ds));
            GDALClose(ds);
          }
        }
      }
      return;
    }

    load_first_var_meta();

    if (_xsize == 0)
      throw Fmi::Exception(BCP, "Cannot determine spatial dimensions from file: " + path);

    // Parse NETCDF_DIM_EXTRA to find extra dimension names
    const std::string extra_raw = get_first_var_meta("NETCDF_DIM_EXTRA");
    const std::vector<std::string> extra_dims = parse_brace_list(extra_raw);

    // Build a map: coordname → {attrname → value} from metadata
    std::map<std::string, std::map<std::string, std::string>> coord_attrs;
    for (const auto& [key, val] : first_var_meta_)
    {
      const auto hash = key.find('#');
      if (hash == std::string::npos)
        continue;
      const std::string cname = key.substr(0, hash);
      const std::string aname = key.substr(hash + 1);
      // Skip NETCDF_DIM_* entries (they use # after the prefix too)
      if (cname.substr(0, 11) == "NETCDF_DIM_")
        continue;
      coord_attrs[cname][aname] = val;
    }

    // Helper: find a coordinate variable name matching axis or units criteria
    auto find_by_axis_or_units =
        [&](const std::set<std::string>& axis_vals,
            const std::set<std::string>& units_vals,
            const std::vector<std::string>& name_fallbacks,
            const std::vector<std::string>* restrict_to = nullptr) -> std::string
    {
      // Check #axis attribute
      for (const auto& [coord, attrs] : coord_attrs)
      {
        if (restrict_to)
        {
          bool found = std::find(restrict_to->begin(), restrict_to->end(), coord) !=
                       restrict_to->end();
          if (!found)
            continue;
        }
        const auto it = attrs.find("axis");
        if (it != attrs.end() && axis_vals.count(it->second))
          return coord;
      }
      // Check #units attribute
      for (const auto& [coord, attrs] : coord_attrs)
      {
        if (restrict_to)
        {
          bool found = std::find(restrict_to->begin(), restrict_to->end(), coord) !=
                       restrict_to->end();
          if (!found)
            continue;
        }
        const auto it = attrs.find("units");
        if (it != attrs.end() && units_vals.count(it->second))
          return coord;
      }
      // Name-based fallback: check coord_attrs first, then try direct open.
      // Only apply restrict_to filter when it is non-empty; an empty restrict_to
      // means no NETCDF_DIM_EXTRA info is available (e.g. HDF5 files), so we
      // should still try the well-known names.
      for (const auto& name : name_fallbacks)
      {
        if (restrict_to && !restrict_to->empty())
        {
          bool found = std::find(restrict_to->begin(), restrict_to->end(), name) !=
                       restrict_to->end();
          if (!found)
            continue;
        }
        if (coord_attrs.count(name))
          return name;
        // For HDF5 or files without coord variable metadata: try opening the variable
        GDALDatasetH tds = open_var_dataset(name);
        if (tds)
        {
          // Merge this coord variable's metadata (dataset + band 1) into first_var_meta_
          // so that get_coord_attr("lon", "units") etc. work for HDF5 files
          const auto load_meta = [&](const char* const* meta, const std::string& prefix)
          {
            for (int j = 0; meta && meta[j]; j++)
            {
              const std::string mitem(meta[j]);
              const auto eq = mitem.find('=');
              if (eq == std::string::npos)
                continue;
              const std::string k = prefix + mitem.substr(0, eq);
              if (first_var_meta_.find(k) == first_var_meta_.end())
                first_var_meta_[k] = mitem.substr(eq + 1);
            }
          };
          load_meta(GDALGetMetadata(tds, nullptr), name + "#");
          GDALRasterBandH b1 = GDALGetRasterBand(tds, 1);
          if (b1)
            load_meta(GDALGetMetadata(b1, nullptr), name + "#");
          GDALClose(tds);
          return name;
        }
      }
      return {};
    };

    // Detect x_name
    if (xname && !xname->empty())
      x_name = *xname;  // preserve case as provided
    else if (!xname)
      x_name = find_by_axis_or_units(
          {"X"},
          {"degrees_east",
           "degree_east",
           "degree_E",
           "degrees_E",
           "degreeE",
           "degreesE",
           "100  km",
           "m",
           "km",
           "Meter"},
          {"lon", "longitude", "x", "xc"});
    // xname == "" → x_name stays ""

    // Detect y_name
    if (yname && !yname->empty())
      y_name = *yname;  // preserve case as provided
    else if (!yname)
      y_name = find_by_axis_or_units(
          {"Y"},
          {"degrees_north",
           "degree_north",
           "degree_N",
           "degrees_N",
           "degreeN",
           "degreesN",
           "100  km",
           "m",
           "km",
           "Meter"},
          {"lat", "latitude", "y", "yc"});

    // Detect t_name (only from extra_dims)
    if (tname && !tname->empty())
      t_name = *tname;  // preserve case as provided
    else if (!tname)
      t_name = find_by_axis_or_units(
          {"T"},
          {},
          {"time", "t"},
          &extra_dims);
    // tname == "" → t_name stays "" (static data)

    // If t_name not found via axis/units, check for "since" in units
    if (t_name.empty() && !tname)
    {
      for (const auto& dim : extra_dims)
      {
        const auto it = coord_attrs.find(dim);
        if (it == coord_attrs.end())
          continue;
        const auto uit = it->second.find("units");
        if (uit != it->second.end() && uit->second.find("since") != std::string::npos)
        {
          t_name = dim;
          break;
        }
      }
    }

    // Detect z_name (only if user requests it or it's auto-detected from extra_dims)
    if (zname && !zname->empty())
      z_name = *zname;  // preserve case as provided
    else if (!zname)
    {
      // Auto-detect: look for Z axis in extra_dims
      z_name = find_by_axis_or_units(
          {"Z"}, {}, {"lev", "level", "z", "zc"}, &extra_dims);
    }
    // zname == "" (default) → z_name stays ""

    // Ensure coordinate variable metadata (especially 'units') is in first_var_meta_
    // for x_name and y_name. When a coord var is found via axis= attribute in another
    // variable, its own metadata (like units=km) may not be loaded yet.
    const auto ensure_coord_meta = [&](const std::string& cname)
    {
      if (cname.empty())
        return;
      // Already have units for this coord variable?
      if (!get_first_var_meta(cname + "#units").empty())
        return;
      GDALDatasetH tds = open_var_dataset(cname);
      if (!tds)
        return;
      // Merge dataset + band 1 metadata with prefix cname+"#"
      const auto load_m = [&](const char* const* meta, const std::string& prefix)
      {
        for (int j = 0; meta && meta[j]; j++)
        {
          const std::string mitem(meta[j]);
          const auto eq = mitem.find('=');
          if (eq == std::string::npos)
            continue;
          const std::string k = prefix + mitem.substr(0, eq);
          if (first_var_meta_.find(k) == first_var_meta_.end())
            first_var_meta_[k] = mitem.substr(eq + 1);
        }
      };
      load_m(GDALGetMetadata(tds, nullptr), cname + "#");
      GDALRasterBandH b1 = GDALGetRasterBand(tds, 1);
      if (b1)
        load_m(GDALGetMetadata(b1, nullptr), cname + "#");
      GDALClose(tds);
    };
    ensure_coord_meta(x_name);
    ensure_coord_meta(y_name);

    // Validate
    if (x_name.empty())
      throw Fmi::Exception(BCP, "X-axis not found in file " + path);
    if (y_name.empty())
      throw Fmi::Exception(BCP, "Y-axis not found in file " + path);
    if (z_name.empty() && zname && !zname->empty())
      throw Fmi::Exception(BCP, "Z-axis '" + *zname + "' not found in file " + path);
    if (t_name.empty() && tname && !tname->empty())
      throw Fmi::Exception(BCP, "T-axis '" + *tname + "' not found in file " + path);

    // If user specified a z-dim or t-dim, ensure we have metadata from a variable with those dims
    if (!z_name.empty())
      merge_dim_meta(z_name);
    if (!t_name.empty())
      merge_dim_meta(t_name);

    // Cache dimension sizes
    _zsize = z_name.empty() ? 1UL : compute_dim_size(z_name);
    _tsize = t_name.empty() ? 0UL : compute_dim_size(t_name);
    // _xsize and _ysize were set in load_first_var_meta

    if (options.debug)
    {
      std::cerr << "debug: axes x=" << x_name << " y=" << y_name << " z=" << z_name
                << " t=" << t_name << "\n";
      std::cerr << "debug: sizes xsize=" << _xsize << " ysize=" << _ysize
                << " zsize=" << _zsize << " tsize=" << _tsize << "\n";
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ======================================================================
// Axis info queries
// ======================================================================

std::string nctools::NcFileExtended::x_units() const
{
  const std::string u = get_coord_attr(x_name, "units");
  if (!u.empty())
    return u;
  // Fallback: infer from standard_name (HDF5 files often lack explicit units)
  const std::string sn = get_coord_attr(x_name, "standard_name");
  if (sn == "longitude")
    return "degrees_east";
  if (sn == "projection_x_coordinate")
    return "m";
  return {};
}

std::string nctools::NcFileExtended::y_units() const
{
  const std::string u = get_coord_attr(y_name, "units");
  if (!u.empty())
    return u;
  const std::string sn = get_coord_attr(y_name, "standard_name");
  if (sn == "latitude")
    return "degrees_north";
  if (sn == "projection_y_coordinate")
    return "m";
  return {};
}

std::string nctools::NcFileExtended::z_units() const
{
  if (z_name.empty())
    return {};
  return get_coord_attr(z_name, "units");
}

bool nctools::NcFileExtended::is_dim(const std::string& name) const
{
  // A name is a dimension (coordinate variable) if it is NOT in the GDAL SUBDATASETS.
  // GDAL only lists data variables in SUBDATASETS, not 1D coord vars.
  const auto varnames = get_gdal_variable_names();
  const std::string lower = boost::algorithm::to_lower_copy(name);
  for (const auto& vn : varnames)
    if (boost::algorithm::to_lower_copy(vn) == lower)
      return false;
  return true;
}

bool nctools::NcFileExtended::axis_match(const std::string& varname) const
{
  try
  {
    const std::string desc = get_subdataset_desc(varname);
    if (desc.empty())
    {
      // No subdataset desc: single-variable file.
      // Accept if varname matches the loaded first variable name.
      load_first_var_meta();
      return varname == first_var_name_;
    }

    const auto dims = parse_desc_dims(desc);
    if (dims.size() < 2)
      return false;

    // Spatial dims are last two: dims[-2] = ysize, dims[-1] = xsize
    const long actual_xsz = dims.back();
    const long actual_ysz = dims[dims.size() - 2];

    if (actual_xsz != static_cast<long>(_xsize) ||
        actual_ysz != static_cast<long>(_ysize))
      return false;

    // Expected number of extra dims
    const int n_extra_expected = (t_name.empty() ? 0 : 1) + (z_name.empty() ? 0 : 1);
    const int n_extra_actual = static_cast<int>(dims.size()) - 2;

    if (n_extra_actual != n_extra_expected)
      return false;

    // Check that extra dims product matches expected tsize * zsize
    if (n_extra_actual > 0)
    {
      const long expected_product = (t_name.empty() ? 1L : static_cast<long>(_tsize)) *
                                    (z_name.empty() ? 1L : static_cast<long>(_zsize));
      long actual_product = 1;
      for (int i = 0; i < n_extra_actual; i++)
        actual_product *= dims[static_cast<std::size_t>(i)];
      if (actual_product != expected_product)
        return false;
    }

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ======================================================================
// get_standard_name
// ======================================================================

std::string nctools::NcFileExtended::get_standard_name(const std::string& varname) const
{
  try
  {
    // First check cached first-variable metadata
    const std::string cached = get_first_var_meta(varname + "#standard_name");
    if (!cached.empty())
      return cached;

    // Open the variable's subdataset and read standard_name
    GDALDatasetH ds = open_var_dataset(varname);
    if (!ds)
      return varname;

    const std::string key = varname + "#standard_name";
    const char* sn = GDALGetMetadataItem(ds, key.c_str(), nullptr);
    std::string result = sn ? sn : varname;
    GDALClose(ds);
    return result;
  }
  catch (...)
  {
    return varname;
  }
}

// ======================================================================
// grid_mapping
// ======================================================================

std::string nctools::NcFileExtended::grid_mapping()
{
  try
  {
    if (projectionName != nullptr)
      return *projectionName;

    load_first_var_meta();

    // Look for any variable's #grid_mapping attribute in the first-var metadata
    std::string projection_var_name;
    for (const auto& [key, val] : first_var_meta_)
    {
      const auto hash = key.find('#');
      if (hash == std::string::npos)
        continue;
      const std::string attrname = key.substr(hash + 1);
      if (attrname == "grid_mapping")
      {
        projection_var_name = val;
        break;
      }
    }

    if (!projection_var_name.empty())
    {
      // Read grid_mapping_name and projection origin from the projection variable's attributes
      const std::string gm_name =
          get_first_var_meta(projection_var_name + "#grid_mapping_name");
      if (!gm_name.empty())
        projectionName = std::make_shared<std::string>(gm_name);

      const std::string lon_str =
          get_first_var_meta(projection_var_name + "#longitude_of_projection_origin");
      if (!lon_str.empty())
      {
        try
        {
          longitudeOfProjectionOrigin = std::stod(lon_str);
        }
        catch (...)
        {
        }
      }

      const std::string lat_str =
          get_first_var_meta(projection_var_name + "#latitude_of_projection_origin");
      if (!lat_str.empty())
      {
        try
        {
          latitudeOfProjectionOrigin = std::stod(lat_str);
        }
        catch (...)
        {
        }
      }
    }

    if (projectionName == nullptr)
      projectionName = std::make_shared<std::string>(LATITUDE_LONGITUDE);

    return *projectionName;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool nctools::NcFileExtended::isStereographic()
{
  return (grid_mapping() == POLAR_STEREOGRAPHIC);
}

// ======================================================================
// find_axis_bounds_gdal / find_lonlat_bounds / find_bounds
// ======================================================================

void nctools::NcFileExtended::find_axis_bounds_gdal(const std::string& coordname,
                                                     unsigned long n,
                                                     double& x1,
                                                     double& x2,
                                                     const char* label,
                                                     bool& isdescending,
                                                     bool* is_irregular)
{
  try
  {
    GDALDatasetH ds = open_var_dataset(coordname);
    if (!ds)
      throw Fmi::Exception(BCP,
                           std::string("Cannot open coordinate variable '") + coordname + "'");

    const int xsz = GDALGetRasterXSize(ds);
    const int ysz = GDALGetRasterYSize(ds);
    const int nb = GDALGetRasterCount(ds);
    const int total = xsz * ysz * nb;

    std::vector<double> values(total);
    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    GDALRasterIO(
        band, GF_Read, 0, 0, xsz, ysz, values.data(), xsz, ysz, GDT_Float64, 0, 0);
    GDALClose(ds);

    if (values.empty())
      return;

    isdescending = false;
    if (values.size() >= 2 && values[1] < values[0])
      isdescending = true;

    // Verify monotonicity
    for (std::size_t i = 1; i < values.size(); i++)
    {
      if (!isdescending && values[i] <= values[i - 1])
        throw Fmi::Exception(BCP,
                             std::string(label) + "-axis is not monotonously increasing");
      if (isdescending && values[i] >= values[i - 1])
        throw Fmi::Exception(BCP,
                             std::string(label) + "-axis is not monotonously decreasing");
    }

    if (!isdescending)
    {
      x1 = values.front();
      x2 = values.back();
    }
    else
    {
      x2 = values.front();
      x1 = values.back();
    }

    // Verify step size is even
    if (n <= 2)
      return;
    const double step = (x2 - x1) / (n - 1);
    // Check strict regularity (matches GDAL's internal ~1e-4 threshold).
    // This flag is used to detect when GDAL resamples an irregular lat axis to ascending order.
    if (is_irregular)
    {
      *is_irregular = false;
      for (std::size_t i = 1; i < values.size(); i++)
      {
        const double s = isdescending ? (values[i - 1] - values[i]) : (values[i] - values[i - 1]);
        if (std::abs(s - step) > 1e-4 * std::abs(step))
        {
          *is_irregular = true;
          break;
        }
      }
    }
    for (std::size_t i = 1; i < values.size(); i++)
    {
      const double s = isdescending ? (values[i - 1] - values[i]) : (values[i] - values[i - 1]);
      if (std::abs(s - step) > options.tolerance * step)
        throw Fmi::Exception(BCP,
                             std::string(label) + "-axis is not regular with tolerance " +
                                 std::to_string(options.tolerance));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void nctools::NcFileExtended::find_lonlat_bounds(double& lon1,
                                                  double& lat1,
                                                  double& lon2,
                                                  double& lat2)
{
  try
  {
    load_first_var_meta();

    // Find lat/lon variable names from first-var metadata
    std::string lat_var, lon_var;
    for (const auto& [key, val] : first_var_meta_)
    {
      const auto hash = key.find('#');
      if (hash == std::string::npos)
        continue;
      const std::string cname = key.substr(0, hash);
      const std::string aname = key.substr(hash + 1);
      if (aname == "standard_name")
      {
        if (val == "latitude")
          lat_var = cname;
        else if (val == "longitude")
          lon_var = cname;
      }
    }
    if (lat_var.empty())
      lat_var = "latitude";
    if (lon_var.empty())
      lon_var = "longitude";

    // Read corner values of the 2D lat/lon arrays.
    // GDAL returns rasters north-to-south (row 0 = northernmost).
    // For a stereographic grid:
    //   Bottom-left corner (projected SW) = pixel (0, ysz-1)
    //   Top-right corner   (projected NE) = pixel (xsz-1, 0)
    auto read_corners = [&](const std::string& varname, double& c1, double& c2)
    {
      GDALDatasetH ds = open_var_dataset(varname);
      if (!ds)
        return;
      const int xsz = GDALGetRasterXSize(ds);
      const int ysz = GDALGetRasterYSize(ds);
      GDALRasterBandH band = GDALGetRasterBand(ds, 1);
      if (band && xsz > 0 && ysz > 0)
      {
        double bl = 0, tr = 0;
        GDALRasterIO(band, GF_Read, 0, ysz - 1, 1, 1, &bl, 1, 1, GDT_Float64, 0, 0);
        GDALRasterIO(band, GF_Read, xsz - 1, 0, 1, 1, &tr, 1, 1, GDT_Float64, 0, 0);
        c1 = bl;
        c2 = tr;
      }
      GDALClose(ds);
    };

    read_corners(lat_var, lat1, lat2);
    read_corners(lon_var, lon1, lon2);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void nctools::NcFileExtended::find_bounds()
{
  try
  {
    if (isStereographic())
    {
      find_lonlat_bounds(_xmin, _ymin, _xmax, _ymax);
    }
    else
    {
      find_axis_bounds_gdal(x_name, _xsize, _xmin, _xmax, "x", _xinverted);
      // Only detect lat irregularity for geographic (degree) y-axes.
      // GDAL resamples irregular lat axes to ascending order only for geographic grids.
      // For projected axes (km, m), GDAL does not resample.
      const std::string yunits = y_units();
      const bool is_lat_axis = (yunits.find("degree") != std::string::npos);
      find_axis_bounds_gdal(y_name, _ysize, _ymin, _ymax, "y", _yinverted,
                            is_lat_axis ? &_y_lat_irregular : nullptr);
    }

    // Z bounds (if z axis exists)
    if (!z_name.empty())
    {
      const auto zvals = get_dim_values_double(z_name);
      if (!zvals.empty())
      {
        _zmin = *std::min_element(zvals.begin(), zvals.end());
        _zmax = *std::max_element(zvals.begin(), zvals.end());
      }
    }

    minmaxfound = true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool nctools::NcFileExtended::xinverted()
{
  if (!minmaxfound)
    find_bounds();
  return _xinverted;
}

bool nctools::NcFileExtended::yinverted()
{
  if (!minmaxfound)
    find_bounds();
  return _yinverted;
}

double nctools::NcFileExtended::xmin()
{
  if (!minmaxfound)
    find_bounds();
  return _xmin;
}

double nctools::NcFileExtended::xmax()
{
  if (!minmaxfound)
    find_bounds();
  return _xmax;
}

double nctools::NcFileExtended::ymin()
{
  if (!minmaxfound)
    find_bounds();
  return _ymin;
}

double nctools::NcFileExtended::ymax()
{
  if (!minmaxfound)
    find_bounds();
  return _ymax;
}

double nctools::NcFileExtended::zmin()
{
  if (!minmaxfound)
    find_bounds();
  return _zmin;
}

double nctools::NcFileExtended::zmax()
{
  if (!minmaxfound)
    find_bounds();
  return _zmax;
}

// ======================================================================
// Axis scaling
// ======================================================================

double nctools::NcFileExtended::x_scale()
{
  if (!x_scale_set_)
  {
    xscale_ = axis_scale_from_units(x_units());
    x_scale_set_ = true;
  }
  return xscale_;
}

double nctools::NcFileExtended::y_scale()
{
  if (!y_scale_set_)
  {
    yscale_ = axis_scale_from_units(y_units());
    y_scale_set_ = true;
  }
  return yscale_;
}

double nctools::NcFileExtended::z_scale()
{
  if (!z_scale_set_)
  {
    zscale_ = axis_scale_from_units(z_units());
    z_scale_set_ = true;
  }
  return zscale_;
}

// ======================================================================
// get_z_values
// ======================================================================

std::vector<long> nctools::NcFileExtended::get_z_values() const
{
  try
  {
    if (z_name.empty())
      return {};
    const auto dvals = get_dim_values_double(z_name);
    std::vector<long> result;
    result.reserve(dvals.size());
    for (double v : dvals)
      result.push_back(static_cast<long>(v));
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ======================================================================
// get_missingvalue_for_var
// ======================================================================

float nctools::NcFileExtended::get_missingvalue_for_var(const std::string& varname) const
{
  try
  {
    // Check first-var meta first (fast path for first variable)
    std::string fv = get_first_var_meta(varname + "#_FillValue");
    if (fv.empty())
      fv = get_first_var_meta(varname + "#fill_value");
    if (!fv.empty())
    {
      try
      {
        return std::stof(fv);
      }
      catch (...)
      {
      }
    }

    // Open the variable's subdataset
    GDALDatasetH ds = open_var_dataset(varname);
    if (!ds)
      return std::numeric_limits<float>::quiet_NaN();

    std::string key1 = varname + "#_FillValue";
    const char* fv1 = GDALGetMetadataItem(ds, key1.c_str(), nullptr);
    std::string key2 = varname + "#fill_value";
    const char* fv2 = GDALGetMetadataItem(ds, key2.c_str(), nullptr);
    GDALClose(ds);

    const char* fv_raw = fv1 ? fv1 : fv2;
    if (fv_raw)
    {
      try
      {
        return std::stof(fv_raw);
      }
      catch (...)
      {
      }
    }
    return std::numeric_limits<float>::quiet_NaN();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ======================================================================
// timeList
// ======================================================================

NFmiTimeList nctools::NcFileExtended::timeList(std::string varName, std::string unitAttrName)
{
  try
  {
    if (timelist != nullptr)
      return *timelist;

    auto tlist = std::make_shared<NFmiTimeList>();

    load_first_var_meta();

    if (wrf)
    {
      // WRF: read Times character variable via GDAL
      GDALDatasetH times_ds = open_var_dataset("Times");
      if (times_ds)
      {
        const int ncols = GDALGetRasterXSize(times_ds);  // character width (e.g. 19)
        const int nrows = GDALGetRasterYSize(times_ds);  // time count
        std::vector<char> ncvals(static_cast<std::size_t>(ncols) * nrows);
        GDALRasterBandH band = GDALGetRasterBand(times_ds, 1);
        GDALRasterIO(
            band, GF_Read, 0, 0, ncols, nrows, ncvals.data(), ncols, nrows, GDT_Byte, 0, 0);
        GDALClose(times_ds);
        for (int row = 0; row < nrows; row++)
        {
          std::string stamp;
          for (int col = 0; col < ncols; col++)
          {
            char ch = ncvals[static_cast<std::size_t>(row) * ncols + col];
            if (ch == '_')
              ch = 'T';
            stamp += ch;
          }
          auto t = Fmi::TimeParser::parse(stamp);
          tlist->Add(new NFmiMetTime(tomettime(t)));
        }
      }
      timelist = tlist;
      return *timelist;
    }

    // Get time units string: try coord_attr first, then look up "time" variable's units
    std::string t_var = t_name.empty() ? "time" : t_name;
    std::string units_str = get_coord_attr(t_var, "units");
    if (units_str.empty())
    {
      // For isStereographic path with custom varName
      if (varName != "time")
      {
        units_str = get_coord_attr(varName, unitAttrName);
        if (units_str.empty())
          throw Fmi::Exception(BCP,
                               "Could not find attribute " + unitAttrName +
                                   " for variable " + varName);
        t_var = varName;
      }
      else
        throw Fmi::Exception(BCP, "Time axis has no defined units in file: " + path);
    }

    Fmi::DateTime origintime;
    long timeunit = 0;
    parse_time_units_string(units_str, &origintime, &timeunit);

    if (t_name.empty())
    {
      // Static data (--tdim ''): use origin time
      tlist->Add(new NFmiMetTime(tomettime(origintime)));
    }
    else
    {
      // Read time values
      std::vector<double> raw_values = get_dim_values_double(t_name);

      // For isStereographic with custom varName: use NETCDF_DIM_t_VALUES may not work,
      // fall back to reading the variable
      if (raw_values.empty() && varName != "time")
        raw_values = get_dim_values_double(varName);

      // If still empty, open the time variable directly
      if (raw_values.empty())
      {
        GDALDatasetH tds = open_var_dataset(t_var);
        if (tds)
        {
          const int xsz = GDALGetRasterXSize(tds);
          const int ysz = GDALGetRasterYSize(tds);
          std::vector<double> tvals(static_cast<std::size_t>(xsz) * ysz);
          GDALRasterBandH band = GDALGetRasterBand(tds, 1);
          GDALRasterIO(band, GF_Read, 0, 0, xsz, ysz, tvals.data(), xsz, ysz,
                       GDT_Float64, 0, 0);
          GDALClose(tds);
          raw_values = tvals;
        }
      }

      for (double val_d : raw_values)
      {
        const long timeoffset = static_cast<long>(val_d);
        Fmi::DateTime validtime = origintime + Fmi::Minutes(timeshift);

        if (timeunit == 1)
          validtime += Fmi::Seconds(timeoffset);
        else if (timeunit == 60)
          validtime += Fmi::Minutes(timeoffset);
        else if (timeunit == 60 * 60)
          validtime += Fmi::Hours(timeoffset);
        else if (timeunit == 24 * 60 * 60)
          validtime += Fmi::Hours(24 * timeoffset);
        else
          validtime += Fmi::Seconds(timeoffset * timeunit);

        tlist->Add(new NFmiMetTime(tomettime(validtime)));
      }
    }

    timelist = tlist;
    return *timelist;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ======================================================================
// copy_values (public - iterates all variables)
// ======================================================================

void nctools::NcFileExtended::copy_values(const Options& opts,
                                           NFmiFastQueryInfo& info,
                                           const ParamConversions& paramconvs,
                                           bool useAutoGeneratedIds)
{
  try
  {
    const std::vector<std::string> varnames = get_gdal_variable_names();
    if (varnames.empty())
      return;

    for (const auto& varname : varnames)
    {
      if (!wrf && !axis_match(varname))
      {
        if (opts.verbose)
          std::cerr << "debug: variable " << varname << " skipped (dimension mismatch)\n";
        continue;
      }

      const std::string name = get_standard_name(varname);
      ParamInfo pinfo = nctools::parse_parameter(name, paramconvs, useAutoGeneratedIds);
      if (pinfo.id == kFmiBadParameter)
        continue;

      if (info.Param(pinfo.id))
      {
        if (pinfo.isregular)
          copy_values_var(opts, varname, info);
        else
          copy_values_wind(info, pinfo, &opts);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ======================================================================
// copy_values_var (regular scalar variable)
// ======================================================================

void nctools::NcFileExtended::copy_values_var(const Options& opts,
                                               const std::string& varname,
                                               NFmiFastQueryInfo& info)
{
  try
  {
    if (opts.debug)
      std::cerr << "\ndebug: starting copy for variable " << varname << std::endl;

    GDALDatasetH ds = open_var_dataset(varname);
    if (!ds)
      throw Fmi::Exception(BCP, "Cannot open variable '" + varname + "'");

    // Read metadata: for NetCDF/GDAL, variable attributes appear as "varname#attr" in dataset
    // metadata; for HDF5 (and some NetCDF via GDAL), they appear without prefix in band metadata.
    GDALRasterBandH band1 = GDALGetRasterBand(ds, 1);
    auto get_meta = [&](const std::string& attr) -> std::string
    {
      const std::string key = varname + "#" + attr;
      const char* v = GDALGetMetadataItem(ds, key.c_str(), nullptr);
      if (v)
        return v;
      // HDF5 fallback: check band 1 metadata without prefix
      if (band1)
      {
        const char* bv = GDALGetMetadataItem(band1, attr.c_str(), nullptr);
        if (bv)
          return bv;
      }
      return {};
    };

    const std::string units = get_meta("units");
    const bool ignoreUnitChange = is_name_in_list(opts.ignoreUnitChangeParams, varname);

    if (opts.verbose && !units.empty())
    {
      if (!ignoreUnitChange && units == "K")
        std::cout << "Note: " << varname << " units converted from K to C\n";
      else if (!ignoreUnitChange && units == "Pa")
        std::cout << "Note: " << varname << " units converted from pa to hPa\n";
      else if (!ignoreUnitChange && units == "fraction")
        std::cout << "Note: " << varname << " units converted from fraction to percentage\n";
    }

    // Missing value
    float missingvalue = std::numeric_limits<float>::quiet_NaN();
    {
      const std::string fv_str = get_meta("_FillValue");
      if (!fv_str.empty())
      {
        try
        {
          missingvalue = std::stof(fv_str);
        }
        catch (...)
        {
        }
      }
      if (std::isnan(missingvalue))
      {
        const std::string fv2_str = get_meta("fill_value");
        if (!fv2_str.empty())
        {
          try
          {
            missingvalue = std::stof(fv2_str);
          }
          catch (...)
          {
          }
        }
      }
      // Fallback: use GDAL band NoData value (covers cases where _FillValue is not in metadata)
      if (std::isnan(missingvalue) && band1)
      {
        int hasNoData = 0;
        const double nodata = GDALGetRasterNoDataValue(band1, &hasNoData);
        if (hasNoData)
          missingvalue = static_cast<float>(nodata);
      }
    }

    // scale_factor and add_offset
    float scale = 1.0f;
    float offset = 0.0f;
    {
      const std::string sf_str = get_meta("scale_factor");
      if (!sf_str.empty())
      {
        try
        {
          scale = std::stof(sf_str);
        }
        catch (...)
        {
        }
      }
      const std::string ao_str = get_meta("add_offset");
      if (!ao_str.empty())
      {
        try
        {
          offset = std::stof(ao_str);
        }
        catch (...)
        {
        }
      }
    }

    // Unit conversions
    if (!ignoreUnitChange)
    {
      if (units == "K")
        offset -= 273.15f;
      else if (units == "Pa")
        scale *= 0.01f;
      else if (units == "fraction")
        scale *= 100.0f;
    }

    // Read all bands into a flat array
    const int nx = GDALGetRasterXSize(ds);
    const int ny = GDALGetRasterYSize(ds);
    const int nbands = GDALGetRasterCount(ds);

    // Determine y-axis orientation.
    // Standard GDAL (regular lat): row 0 = northernmost → read from last row first.
    // GDAL resampled irregular lat to ascending order: row 0 = southernmost → read from first row.
    // _y_lat_irregular is set in find_bounds() by checking strict step regularity.
    const bool gdal_southup = _y_lat_irregular;

    std::vector<float> vals(static_cast<std::size_t>(nx) * ny * nbands);
    for (int b = 1; b <= nbands; b++)
    {
      GDALRasterBandH band = GDALGetRasterBand(ds, b);
      GDALRasterIO(band,
                   GF_Read,
                   0,
                   0,
                   nx,
                   ny,
                   vals.data() + static_cast<std::size_t>(b - 1) * nx * ny,
                   nx,
                   ny,
                   GDT_Float32,
                   0,
                   0);
    }
    GDALClose(ds);

    // Debug: print first value at row 0 and last row for comparison

    const std::size_t x_size = _xsize;
    const std::size_t y_size = _ysize;
    int sourcetimeindex = 0;
    int targettimeindex = 0;
    unsigned long zstart = 0;

    for (info.ResetTime(); info.NextTime(); ++targettimeindex)
    {
      unsigned long level = 0;
      const NFmiTime targettime = info.Time();
      auto tmp_tlist = timeList();
      const NFmiTime* sourcetimeptr = tmp_tlist.Time(sourcetimeindex);
      if (sourcetimeptr == nullptr)
        break;
      const NFmiTime sourcetime = *sourcetimeptr;

      if (sourcetime == targettime)
      {
        for (info.ResetLevel(); info.NextLevel(); ++level)
        {
          unsigned long xcounter = (xinverted() ? x_size - 1 : 0);
          // NFmiQueryInfo iterates y=0=southernmost.
          // Standard GDAL (gt[5]<0): row 0 = northernmost → start at last row, decrement.
          // South-up GDAL (gt[5]>0, e.g. after resampling irregular lat): row 0 = southernmost
          // → start at first row, increment.
          unsigned long ystart = gdal_southup ? zstart : zstart + (y_size - 1) * x_size;

          for (info.ResetLocation(); info.NextLocation();)
          {
            const float value_raw = vals.at(ystart + xcounter);
            if (!IsMissingValue(value_raw, missingvalue))
            {
              const float value = scale * value_raw + offset;
              info.FloatValue(value);
            }
            if (xcounter == (xinverted() ? 0UL : x_size - 1))
            {
              // Move to next row (northward)
              if (gdal_southup)
                ystart += x_size;  // south-up: row 0=south, ascending toward north
              else
                ystart -= x_size;  // north-up: row 0=north, descending toward south
              xcounter = (xinverted() ? x_size - 1 : 0);
            }
            else
            {
              (xinverted() ? xcounter-- : xcounter++);
            }
          }
          zstart += x_size * y_size;
        }
        sourcetimeindex++;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ======================================================================
// copy_values_wind (U/V or speed/direction components)
// ======================================================================

void nctools::NcFileExtended::copy_values_wind(NFmiFastQueryInfo& info,
                                                const ParamInfo& pinfo,
                                                const Options* opts)
{
  try
  {
    const float pi = 3.14159265358979326f;

    // Read X component
    GDALDatasetH xds = open_var_dataset(pinfo.x_component);
    if (!xds)
      return;
    GDALDatasetH yds = open_var_dataset(pinfo.y_component);
    if (!yds)
    {
      GDALClose(xds);
      return;
    }

    auto get_float_meta = [](GDALDatasetH ds, const std::string& varname,
                              const std::string& attr) -> float
    {
      const std::string key = varname + "#" + attr;
      const char* v = GDALGetMetadataItem(ds, key.c_str(), nullptr);
      if (!v)
        return std::numeric_limits<float>::quiet_NaN();
      try
      {
        return std::stof(v);
      }
      catch (...)
      {
        return std::numeric_limits<float>::quiet_NaN();
      }
    };

    const float xmissingvalue = get_float_meta(xds, pinfo.x_component, "_FillValue");
    float xscale = get_float_meta(xds, pinfo.x_component, "scale_factor");
    if (std::isnan(xscale))
      xscale = 1.0f;
    float xoffset = get_float_meta(xds, pinfo.x_component, "add_offset");
    if (std::isnan(xoffset))
      xoffset = 0.0f;

    const float ymissingvalue = get_float_meta(yds, pinfo.y_component, "_FillValue");
    float yscale = get_float_meta(yds, pinfo.y_component, "scale_factor");
    if (std::isnan(yscale))
      yscale = 1.0f;
    float yoffset = get_float_meta(yds, pinfo.y_component, "add_offset");
    if (std::isnan(yoffset))
      yoffset = 0.0f;

    const int xnx = GDALGetRasterXSize(xds);
    const int xny = GDALGetRasterYSize(xds);
    const int xnb = GDALGetRasterCount(xds);
    std::vector<float> xvals(static_cast<std::size_t>(xnx) * xny * xnb);
    for (int b = 1; b <= xnb; b++)
    {
      GDALRasterBandH band = GDALGetRasterBand(xds, b);
      GDALRasterIO(band,
                   GF_Read,
                   0,
                   0,
                   xnx,
                   xny,
                   xvals.data() + static_cast<std::size_t>(b - 1) * xnx * xny,
                   xnx,
                   xny,
                   GDT_Float32,
                   0,
                   0);
    }
    GDALClose(xds);

    const int ynx = GDALGetRasterXSize(yds);
    const int yny = GDALGetRasterYSize(yds);
    const int ynb = GDALGetRasterCount(yds);
    std::vector<float> yvals(static_cast<std::size_t>(ynx) * yny * ynb);
    for (int b = 1; b <= ynb; b++)
    {
      GDALRasterBandH band = GDALGetRasterBand(yds, b);
      GDALRasterIO(band,
                   GF_Read,
                   0,
                   0,
                   ynx,
                   yny,
                   yvals.data() + static_cast<std::size_t>(b - 1) * ynx * yny,
                   ynx,
                   yny,
                   GDT_Float32,
                   0,
                   0);
    }
    GDALClose(yds);

    const auto src_tlist = timeList();
    const std::size_t nsrctimes =
        static_cast<std::size_t>(src_tlist.NumberOfItems());
    const std::size_t n_per_time =
        (nsrctimes > 0 && xvals.size() >= nsrctimes) ? xvals.size() / nsrctimes
                                                      : xvals.size();

    if (opts && opts->debug)
    {
      std::cerr << "debug: x-component has " << xvals.size() << " elements, " << nsrctimes
                << " time steps, " << n_per_time << " per step\n";
    }

    int sourcetimeindex = 0;
    int targettimeindex = 0;
    for (info.ResetTime(); info.NextTime(); ++targettimeindex)
    {
      const NFmiTime targettime = info.Time();
      const NFmiTime* sourcetimeptr = src_tlist.Time(sourcetimeindex);

      if (sourcetimeptr == nullptr)
        break;
      const NFmiTime sourcetime = *sourcetimeptr;

      if (sourcetime == targettime)
      {
        // GDAL returns north-to-south (row 0 = north); NFmiQueryInfo y=0 = south.
        // Use the same y-flip traversal as copy_values_var.
        if (xinverted())
          throw Fmi::Exception(BCP, "Inverted x-axis not implemented in wind copy");
        const std::size_t nx = static_cast<std::size_t>(_xsize);
        const std::size_t ny = static_cast<std::size_t>(_ysize);
        const std::size_t t_offset = static_cast<std::size_t>(sourcetimeindex) * n_per_time;
        std::size_t zstart = 0;
        for (info.ResetLevel(); info.NextLevel();)
        {
          std::size_t ystart = t_offset + zstart + (ny - 1) * nx;
          std::size_t xcounter = 0;
          for (info.ResetLocation(); info.NextLocation();)
          {
            const std::size_t idx = ystart + xcounter;
            const float xv = xvals.at(idx);
            const float yv = yvals.at(idx);
            if (xv != xmissingvalue && yv != ymissingvalue)
            {
              const float x = xscale * xv + xoffset;
              const float y = yscale * yv + yoffset;
              if (pinfo.isspeed)
                info.FloatValue(std::sqrt(x * x + y * y));
              else
                info.FloatValue(180.0f * std::atan2(x, y) / pi);
            }
            if (xcounter == nx - 1)
            {
              ystart -= nx;
              xcounter = 0;
            }
            else
            {
              xcounter++;
            }
          }
          zstart += nx * ny;
        }
        sourcetimeindex++;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ======================================================================
// joinable
// ======================================================================

bool nctools::NcFileExtended::joinable(NcFileExtended& ncfile,
                                        std::vector<std::string>* failreasons)
{
  try
  {
    bool ok = true;
    if (failreasons != nullptr)
      failreasons->clear();

    auto check = [&](bool cond, const std::string& reason)
    {
      if (!cond)
      {
        ok = false;
        if (failreasons)
          failreasons->push_back(reason);
      }
    };

    check(grid_mapping() == ncfile.grid_mapping(), "projection is different");
    check(xsize() == ncfile.xsize(), "x-axis dimension is different");
    check(ysize() == ncfile.ysize(), "y-axis dimension is different");
    check(zsize() == ncfile.zsize(), "z-axis dimension is different");
    check(longitudeOfProjectionOrigin == ncfile.longitudeOfProjectionOrigin,
          "origin(longitude) is different");
    check(latitudeOfProjectionOrigin == ncfile.latitudeOfProjectionOrigin,
          "origin(latitude) is different");
    check(xinverted() == ncfile.xinverted(), "x-axis inversion is different");
    check(yinverted() == ncfile.yinverted(), "y-axis inversion is different");
    check(x_scale() == ncfile.x_scale(), "x-axis units are different");
    check(y_scale() == ncfile.y_scale(), "y-axis units are different");
    check(isStereographic() == ncfile.isStereographic(),
          "both files are not stereographic");

    return ok;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ======================================================================
// require_conventions
// ======================================================================

void nctools::NcFileExtended::require_conventions(const std::string* reference)
{
  try
  {
    if (reference == nullptr || reference->empty())
      return;

    load_first_var_meta();
    // GDAL exposes Conventions as "NC_GLOBAL#Conventions" for NetCDF or "Conventions" for HDF5
    std::string ref = get_first_var_meta("NC_GLOBAL#Conventions");
    if (ref.empty())
      ref = get_first_var_meta("Conventions");
    if (ref.empty())
      throw Fmi::Exception(BCP, "The NetCDF file is missing the Conventions attribute");

    // Accept COARDS as subset of all CF conventions
    if (ref == "COARDS")
      return;

    std::string refsub = ref;
    std::string cmp = *reference;
    if (refsub.size() > 3 && refsub.substr(0, 3) == "CF-")
      refsub = ref.substr(3);
    if (cmp.size() > 3 && cmp.substr(0, 3) == "CF-")
      cmp = reference->substr(3);

    if (compare_versions(refsub, cmp) < 0)
      throw Fmi::Exception(BCP,
                           "The file must conform to " + *reference + ", not to " + ref);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ======================================================================
// printInfo
// ======================================================================

void nctools::NcFileExtended::printInfo() const
{
  try
  {
    load_first_var_meta();

    const auto varnames = get_gdal_variable_names();

    std::cout << path << " information:\n";

    // Print NETCDF_DIM info from first-var metadata
    std::cout << "    Extra dimensions (from first variable):\n";
    const std::string extra_raw = get_first_var_meta("NETCDF_DIM_EXTRA");
    if (!extra_raw.empty())
    {
      const auto dims = parse_brace_list(extra_raw);
      for (const auto& dim : dims)
      {
        const auto vals = get_dim_values_double(dim);
        std::cout << "\t" << dim << "(" << vals.size() << ")";
        if (!vals.empty())
        {
          std::cout << " [";
          for (std::size_t i = 0; i < std::min(vals.size(), std::size_t(4)); i++)
          {
            if (i > 0)
              std::cout << ",";
            std::cout << vals[i];
          }
          if (vals.size() > 4)
            std::cout << ",...";
          std::cout << "]";
        }
        std::cout << "\n";
      }
    }

    // Spatial dims
    std::cout << "\tx(" << _xsize << ") y(" << _ysize << ")\n";

    // Parameter list from GDAL SUBDATASETS
    std::cout << "    Variables (GDAL SUBDATASETS):\n";
    if (!gdal_dataset)
      return;
    char** subdatasets = GDALGetMetadata(gdal_dataset, "SUBDATASETS");
    for (int i = 0; subdatasets && subdatasets[i]; i++)
    {
      const std::string item(subdatasets[i]);
      if (item.find("_DESC=") == std::string::npos)
        continue;
      const auto eq = item.find('=');
      std::cout << "\t" << item.substr(eq + 1) << "\n";
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ======================================================================
// is_name_in_list (defined here as it was in the original)
// ======================================================================

bool nctools::is_name_in_list(const std::list<std::string>& nameList, const std::string name)
{
  try
  {
    if (!nameList.empty())
    {
      auto it = std::find(nameList.begin(), nameList.end(), name);
      if (it != nameList.end())
        return true;
    }
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
