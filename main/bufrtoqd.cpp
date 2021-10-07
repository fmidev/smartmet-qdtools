// ======================================================================
/*!
 * \brief BUFR point observations conversion to querydata
 *
 * Much of the code comes from the bufr_decoder utility included
 * in the Environmental Canada BUFR Library
 */
// ======================================================================

// libECBUFR license:

/***
Copyright Her Majesty The Queen in Right of Canada, Environment Canada, 2009.
Copyright Sa Majeste la Reine du Chef du Canada, Environnement Canada, 2009.

This file is part of libECBUFR.

    libECBUFR is free software: you can redistribute it and/or modify
    it under the terms of the Lesser GNU General Public License,
    version 3, as published by the Free Software Foundation.

    libECBUFR is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    Lesser GNU General Public License for more details.

    You should have received a copy of the Lesser GNU General Public
    License along with libECBUFR.  If not, see <http://www.gnu.org/licenses/>.
***/

#ifdef _MSC_VER
#pragma warning(disable : 4505 4512 4996)  // Disables many useless or 3rd-party-code generated
                                           // warnings that e.g. MSVC++ 2012 generates
#endif

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include <fmt/format.h>
#include <macgyver/CsvReader.h>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiGlobals.h>
#include <newbase/NFmiHPlaceDescriptor.h>
#include <newbase/NFmiLocationBag.h>
#include <newbase/NFmiMetTime.h>
#include <newbase/NFmiParamDescriptor.h>
#include <newbase/NFmiProducerName.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiStation.h>
#include <newbase/NFmiTimeDescriptor.h>
#include <newbase/NFmiTimeList.h>
#include <newbase/NFmiVPlaceDescriptor.h>
#include <smarttools/NFmiAviationStationInfoSystem.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#ifdef UNIX
#include <sys/ioctl.h>
#endif

extern "C"
{
#include <bufr_api.h>
#include <bufr_local.h>
#include <bufr_value.h>
}

namespace fs = boost::filesystem;
struct ParNameInfo
{
  ParNameInfo() {}
  std::string bufrId;
  std::string bufrName;
  std::string shortName;
  FmiParameterName parId{kFmiBadParameter};
};

using NameMap = std::map<std::string, ParNameInfo>;
// typedef std::map<std::string, FmiParameterName> NameMap;

const std::string REMAP_IDENT("ident");
const std::string REMAP_PHASE("phaseofflight");
const std::string REMAP_ALTITUDE("altitude");

const int REMAP_PRIMARY_IDENT = 1006;

using MessageReMap = std::map<std::string, std::list<int>>;

// ----------------------------------------------------------------------
/*!
 * \brief Global since initialization is heavy
 */
// ----------------------------------------------------------------------

NFmiEnumConverter converter;

// ----------------------------------------------------------------------
/*!
 * \brief BUFR data categories
 */
// ----------------------------------------------------------------------

enum BufrDataCategory
{
  kBufrLandSurface = 0,
  kBufrSeaSurface = 1,
  kBufrSounding = 2,  // other than satellite
  kBufrSatSounding = 3,
  kBufrUpperAirLevel = 4,  // other than satellite
  kBufrSatUpperAirLevel = 5,
  kBufrRadar = 6,
  kBufrSynoptic = 7,
  kBufrPhysical = 8,
  kBufrDispersal = 9,
  kBufrRadiological = 10,
  kBufrTables = 11,
  kBufrSatSurface = 12,
  kBufrRadiances = 21,
  kBufrOceanographic = 31,
  kBufrImage = 101
};

// ----------------------------------------------------------------------
/*!
 * \brief BUFR data category names for error messages etc
 */
// ----------------------------------------------------------------------

std::string data_category_name(BufrDataCategory category)
{
  switch (category)
  {
    case kBufrLandSurface:
      return "land surface";
    case kBufrSeaSurface:
      return "sea surface";
    case kBufrSounding:
      return "sounding";
    case kBufrSatSounding:
      return "satellite sounding";
    case kBufrUpperAirLevel:
      return "upper air level";
    case kBufrSatUpperAirLevel:
      return "upper air level with satellite";
    case kBufrRadar:
      return "radar";
    case kBufrSynoptic:
      return "synoptic";
    case kBufrPhysical:
      return "physical";
    case kBufrDispersal:
      return "dispersal";
    case kBufrRadiological:
      return "radiological";
    case kBufrTables:
      return "tables";
    case kBufrSatSurface:
      return "satellite surface";
    case kBufrRadiances:
      return "radiances";
    case kBufrOceanographic:
      return "oceanographic";
    case kBufrImage:
      return "image";
    default:
      return "";
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert name to BUFR data category
 */
// ----------------------------------------------------------------------

BufrDataCategory data_category(const std::string &name)
{
  // Inverse of data_category_name
  if (name == "land surface")
    return kBufrLandSurface;
  if (name == "sea surface")
    return kBufrSeaSurface;
  if (name == "sounding")
    return kBufrSounding;
  if (name == "satellite sounding")
    return kBufrSatSounding;
  if (name == "upper air level")
    return kBufrUpperAirLevel;
  if (name == "upper air level with satellite")
    return kBufrSatUpperAirLevel;
  if (name == "radar")
    return kBufrRadar;
  if (name == "synoptic")
    return kBufrSynoptic;
  if (name == "physical")
    return kBufrPhysical;
  if (name == "dispersal")
    return kBufrDispersal;
  if (name == "radiological")
    return kBufrRadiological;
  if (name == "tables")
    return kBufrTables;
  if (name == "satellite surface")
    return kBufrSatSurface;
  if (name == "radiances")
    return kBufrRadiances;
  if (name == "oceanographic")
    return kBufrOceanographic;
  if (name == "image")
    return kBufrImage;

  // Aliases

  if (name == "land")
    return kBufrLandSurface;
  if (name == "sea")
    return kBufrSeaSurface;

  throw std::runtime_error("Unknown BUFR data category '" + name + "'");
}

// ----------------------------------------------------------------------
/*!
 * \brief Container for command line options
 */
// ----------------------------------------------------------------------

struct Options
{
  bool verbose = false;  // -v --verbose
  bool debug = false;    //    --debug
  // Code 8042 = extended vertical sounding significance. Insignificant levels
  // will also be output if this option is used. This may significantly increase
  // the size of the sounding (from ~100 to ~5000 levels)
  bool insignificant = false;                                      //    --insignificant
  bool subsets = false;                                            //    --subsets
  bool usebufrname = false;                                        // --usebufrname
  std::string category;                                            // -C --category
  std::string conffile = "/usr/share/smartmet/formats/bufr.conf";  // -c --config
  std::string stationsfile = "/usr/share/smartmet/stations.csv";   // -s --stations
  std::string infile = ".";                                        // -i --infile
  std::string outfile = "-";                                       // -o --outfile
  std::string localtableB;                                         // -B --localtableB
  std::string localtableD;                                         // -D --localtableD
  std::string producername = "UNKNOWN";                            // --producername
  long producernumber = 0;                                         // --producernumber
  bool autoproducer = false;                                       // -a --autoproducer
  int messagenumber = 0;                                           // -m --message
  int roundtohours = 0;                                            // --roundtohours
  bool requireident = false;                                       // -I --ident
  std::string remapdef;                                            // -r --remap
  MessageReMap messageremap;                                       // -r --remap
  int minobservations = 3;                                         // -N --minobservations
  int maxdurationhours = 2;                                        // -M --maxdurationhours
  bool totalcloudoctas = false;                                    // -t --totalcloudoctas
  bool forcepressurechangesign = false;                            // -f --forcepressurechangesign
};

Options options;

// ----------------------------------------------------------------------
/*!
 * \brief Parse amdar or sounding bufr remapping option
 */
// ----------------------------------------------------------------------

void parse_option_remap(const std::string &remapdef)
{
  // Parse remapping option for bufr codes
  //
  // All given bufr codes are changed to first/primary code before loading the messages.
  // Thus data can be loaded from multiple bufr codes to the same qd parameter, without
  // defining multiple mappings in configuration (which would currenty result into duplicate
  // qd parameters in parambag). Mapping 'ident' is used to list the codes to extract aircraft
  // reg. nr or other identification to group/collect amdar messages
  //
  // name1,code1,code2,code3;name2,code1,...
  //
  // Name ("ident", "phaseofflight" or "flaltitude"/"altitude") is used only to override default
  // remapping for aircraft identification, phase of flight and altitude. For other mappings the
  // name is currently meaningless (the given codes are remapped regardless of whether they are
  // mapped to qd -parameter or not)

  std::string remapstr = Fmi::trim_copy(remapdef);

  if (!remapstr.empty())
  {
    if (options.debug)
      fprintf(stderr, "remap = %s\n", remapstr.c_str());

    remapstr += ";";

    MessageReMap::iterator rit;
    std::string token;
    std::string name;
    size_t pos0 = 0;
    size_t pos;

    while ((pos = remapstr.find(';', pos0)) != std::string::npos)
    {
      std::string remap = remapstr.substr(pos0, pos - pos0) + ",";

      if (options.debug)
        fprintf(stderr, "  remap = %s\n", remap.c_str());

      pos0 = pos + 1;
      size_t pos1 = 0;
      size_t pos2;

      while ((pos2 = remap.find(',', pos1)) != std::string::npos)
      {
        token = Fmi::trim_copy(remap.substr(pos1, pos2 - pos1));

        if (pos1 == 0)
        {
          if (token.empty())
            name = Fmi::to_string(pos0);
          else
          {
            name = Fmi::ascii_tolower_copy(token);

            if (name == "flaltitude")
              name = REMAP_ALTITUDE;
          }

          auto remapping = options.messageremap.insert(std::make_pair(name, std::list<int>()));

          if (!remapping.second)
            throw std::runtime_error("Duplicate mapping name '" + name + "' for option --remap");

          rit = remapping.first;
        }
        else if (token.empty())
          throw std::runtime_error("Empty code for mapping '" + name + "' for option --remap");
        else
        {
          try
          {
            rit->second.push_back(Fmi::stoi(token));
          }
          catch (...)
          {
            throw std::runtime_error("Invalid code '" + token + "' for mapping '" + name +
                                     "' for option --remap");
          }
        }

        if (options.debug)
          fprintf(stderr, "    token = %s\n", token.c_str());

        pos1 = pos2 + 1;
      }
    }
  }

  // Defaults for aircraft identification, phase of flight and altitude

  if (options.messageremap.find(REMAP_IDENT) == options.messageremap.end())
  {
    std::list<int> ident{REMAP_PRIMARY_IDENT, 1008};
    options.messageremap.insert(std::make_pair(REMAP_IDENT, ident));
  }

  if (options.messageremap.find(REMAP_PHASE) == options.messageremap.end())
  {
    std::list<int> phaseofflight{8004, 8009};
    options.messageremap.insert(std::make_pair(REMAP_PHASE, phaseofflight));
  }

  if (options.messageremap.find(REMAP_ALTITUDE) == options.messageremap.end())
  {
    std::list<int> flaltitude{7002, 7007, 7010};
    options.messageremap.insert(std::make_pair(REMAP_ALTITUDE, flaltitude));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse command line options
 *
 * \return True, if execution may continue as usual
 */
// ----------------------------------------------------------------------

bool parse_options(int argc, char *argv[], Options &options)
{
  namespace po = boost::program_options;

  std::string producerinfo;

  std::string msg1 = "BUFR parameter configuration file (default='" + options.conffile + "')";
  std::string msg2 = "stations CSV file (default='" + options.stationsfile + "')";
  std::string msg3 = "minimum number of observations for acceptable amdar (default=" +
                     Fmi::to_string(options.minobservations) + ")";
  std::string msg4 = "maximum amdar takeoff/landing phase duration in hours (default=" +
                     Fmi::to_string(options.maxdurationhours) + "), exceeding data ignored";

#ifdef UNIX
  struct winsize wsz;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsz);
  const int desc_width = (wsz.ws_col < 80 ? 80 : wsz.ws_col);
#else
  const int desc_width = 100;
#endif

  po::options_description desc("Allowed options", desc_width);
  desc.add_options()("help,h", "print out help message")("version,V", "display version number")(
      "verbose,v", po::bool_switch(&options.verbose), "set verbose mode on")(
      "debug", po::bool_switch(&options.debug), "set debug mode on")(
      "subsets", po::bool_switch(&options.subsets), "decode all subsets, not just first ones")(
      "insignificant",
      po::bool_switch(&options.insignificant),
      "extract also insignificant sounding levels")(
      "roundtohours",
      po::value(&options.roundtohours),
      "round times to multiples of the given number of hours")(
      "config,c", po::value(&options.conffile), msg1.c_str())(
      "stations,s", po::value(&options.stationsfile), msg2.c_str())(
      "infile,i", po::value(&options.infile), "input BUFR file or directory")(
      "outfile,o", po::value(&options.outfile), "output querydata file")(
      "message,m", po::value(&options.messagenumber), "extract the given message number only")(
      "category,C", po::value(&options.category), "extract the given category")(
      "localtableB,B", po::value(&options.localtableB), "local BUFR table B")(
      "localtableD,D", po::value(&options.localtableD), "local BUFR table D")(
      "producer,p", po::value(&producerinfo), "producer number,name")(
      "producernumber", po::value(&options.producernumber), "producer number (default: 0)")(
      "producername", po::value(&options.producername), "producer name (default: UNKNOWN)")(
      "autoproducer,a", po::bool_switch(&options.autoproducer), "guess producer automatically")(
      "usebufrname",
      po::bool_switch(&options.usebufrname),
      "use BUFR parameter name instead of parameter code number from configuration file") (
      "Ident,I",
      po::bool_switch(&options.requireident),
      "load only amdar messages having aircraft reg.nr or other identification")(
      "remap,r",
      po::value(&options.remapdef),
      "remap amdar bufr message codes to first code; name1,code1,code2,code3;name2,code1,...")(
      "minobservatios,N", po::value(&options.minobservations), msg3.c_str())(
      "maxdurationhours,M", po::value(&options.maxdurationhours), msg4.c_str()) (
      "totalcloudoctas,t",
      po::bool_switch(&options.totalcloudoctas),
      "disable octas to percentage conversion for TotalCloudCover") (
      "forcepressurechangesign,f",
      po::bool_switch(&options.forcepressurechangesign),
      "Set PressureChange sign to match PressureTendency value");

  po::positional_options_description p;
  p.add("infile", 1);
  p.add("outfile", 1);

  po::variables_map opt;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);

  po::notify(opt);

  if (opt.count("version") != 0)
  {
    std::cout << "bufrtoqd v1.0 (" << __DATE__ << ' ' << __TIME__ << ')' << std::endl;
  }

  if (opt.count("help"))
  {
    std::cout
        << "Usage: bufrtoqd [options] infile/dir outfile\n"
           "Converts BUFR observations to querydata.\n\n"
        << desc
        << "\n"
           "If option --insignificant is used, sounding levels with a zero extended vertical\n"
           "significance (code 8042) value will be extracted. This may increase the size of\n"
           "high resolution sounding from a hundred levels to thousands, and thus significantly\n"
           "increase the size of the output querydata.\n\n"
           "The known data category names for option -C are:\n\n"
           " * 'land' or 'land surface'\n"
           " * 'sea' or 'sea surface'\n"
           " * 'sounding'\n"
           " * 'satellite sounding'\n"
           " * 'upper air level'\n"
           " * 'upper air level with satellite'\n"
           " * 'radar'\n"
           " * 'synoptic'\n"
           " * 'physical'\n"
           " * 'dispersal'\n"
           " * 'radiological'\n"
           " * 'tables'\n"
           " * 'satellite surface'\n"
           " * 'radiances'\n"
           " * 'oceanographic'\n"
           " * 'image'\n\n"
           "Conversion of all categories is not supported though.\n\n"
           "Earlier versions of bufrtoqd used the second column of the configuration file, i.e.\n"
           "the parameter name, to determine the mapping from BUFR to querydata parameters. This\n"
           "lead to problems since codes 010004 and 007004 both have the same name PRESSURE.\n"
           "The current default is to use the BUFR code instead of the name so that parameters\n"
           "with the same name can be discerned. The old behaviour can be restored using the\n"
           "option --usebufrname.\n\n"
           "New amdar processing (which handles data levels/altitudes) must be enabled with "
           "-I option, other related (-r, -N, -M) settings have no effect without it\n";

    return false;
  }

  if (opt.count("infile") == 0)
    throw std::runtime_error("Expecting input BUFR file as parameter 1");

  if (opt.count("outfile") == 0)
    throw std::runtime_error("Expecting output file as parameter 2");

  if (!fs::exists(options.infile))
    throw std::runtime_error("Input BUFR '" + options.infile + "' does not exist");

  if ((options.roundtohours > 0) && (24 % options.roundtohours != 0))
    throw std::runtime_error("Option roundtohours value must divide 24 evenly (1,2,3,4,6,8,12,24)");

  // Validate the category option
  if (!options.category.empty())
    data_category(options.category);

  // Handle the alternative ways to define the producer

  if (!producerinfo.empty())
  {
    if (options.autoproducer)
      throw std::runtime_error(
          "Cannot use options --producer (-p) and --autoproducer (-a) simultaneously");

    std::vector<std::string> parts;
    boost::algorithm::split(parts, producerinfo, boost::algorithm::is_any_of(","));
    if (parts.size() != 2)
      throw std::runtime_error("Option --producer expects a comma separated number,name argument");

    options.producernumber = Fmi::stol(parts[0]);
    options.producername = parts[1];
  }

  // Handle amdar/sounding bufr code remapping

  if (options.requireident)
    parse_option_remap(options.remapdef);

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Validate we can handle the data category
 */
// ----------------------------------------------------------------------

void validate_category(BufrDataCategory category)
{
  std::string name = data_category_name(category);

  if (name.empty())
    throw std::runtime_error("Data category " + std::to_string(category) + " unknown");

  switch (category)
  {
    // We can handle these, probably
    case kBufrLandSurface:
    case kBufrSeaSurface:
    case kBufrSounding:
    case kBufrSynoptic:
    case kBufrUpperAirLevel:
      break;
    // No sample data for these available during development:
    case kBufrSatSounding:
    case kBufrSatUpperAirLevel:
    case kBufrRadar:
    case kBufrPhysical:
    case kBufrDispersal:
    case kBufrRadiological:
    case kBufrTables:
    case kBufrSatSurface:
    case kBufrRadiances:
    case kBufrOceanographic:
    case kBufrImage:
    default:
      throw std::runtime_error("Cannot handle data category: " + name);
  }

  if (options.verbose)
    std::cout << "Data category: " << name << std::endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Expand the input options
 */
// ----------------------------------------------------------------------

std::list<std::string> expand_input_files()
{
  std::list<std::string> files;

  if (!fs::is_directory(options.infile))
  {
    files.push_back(options.infile);
    return files;
  }

  // List all regular files in the directory

  fs::path p(options.infile);
  std::list<fs::path> paths;
  copy(fs::directory_iterator(p), fs::directory_iterator(), back_inserter(paths));

  BOOST_FOREACH (const fs::path &path, paths)
  {
    if (fs::is_regular(path))
      files.push_back(path.string());
  }

  return files;
}

// ----------------------------------------------------------------------
/*!
 * \brief Information collected from the bufr
 */
// ----------------------------------------------------------------------

struct record
{
  std::string name;
  std::string units;
  double value;
  std::string svalue;

  record() : value(std::numeric_limits<double>::quiet_NaN()) {}
};

std::ostream &operator<<(std::ostream &out, const record &rec)
{
  out << rec.name << "=" << rec.svalue << " (" << rec.value << ") " << rec.units;
  return out;
}

using Message = std::map<int, record>;
using Messages = std::list<Message>;

using IdentMessageMap = std::map<std::string, Messages>;
using TimeIdentMessageMap = std::map<std::string, IdentMessageMap>;
using IdentTimeMap = std::map<std::string, NFmiMetTime>;
using TimeIdentList = std::list<std::string>;
using StationTimeSet = std::map<long, std::set<std::string>>;
using Phase = enum { None = 0, Flying = 3, Takeoff = 5, Landing = 6 };

// ----------------------------------------------------------------------
/*!
 * \brief Extract record name and units
 */
// ----------------------------------------------------------------------

void extract_record_name_and_units(record &rec, int desc, BufrDescriptor *bufr, BUFR_Tables *tables)
{
  if (bufr_is_table_b(desc))
  {
    EntryTableB *tb = bufr_fetch_tableB(tables, desc);
    if (tb)
    {
      /* according to descriptor 13+14=>64 15=>24 */
      rec.name = tb->description;
      rec.units = tb->unit;
    }
    else
    {
      std::cerr << "Warning: Descriptor " << desc << " not found in table B" << std::endl;
    }
  }
  else if (bufr->encoding.type == TYPE_CCITT_IA5)
  {
    int fx = desc / 1000;
    if (fx == 205)
    {
      rec.name = "Signify character";
      rec.units = "CCITT_IA5";
    }
  }
  else
  {
  }  // unknown
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract record value
 */
// ----------------------------------------------------------------------

void extract_record_value(record &rec, BufrDescriptor *bufr)
{
  if (bufr->value)
  {
    if (bufr->value->af)
    {
      // we do not handle associated fields
    }

    if (bufr->value->type == VALTYPE_INT32 || bufr->value->type == VALTYPE_INT64)
    {
      int64_t value = bufr_descriptor_get_ivalue(bufr);
      if (value == -1)
        rec.value = kFloatMissing;
      else
        rec.value = static_cast<double>(value);
    }

    else if (bufr->value->type == VALTYPE_FLT32)
    {
      float value = bufr_descriptor_get_fvalue(bufr);
      if (bufr_is_missing_float(value))
        rec.value = kFloatMissing;
      else
        rec.value = value;
    }

    else if (bufr->value->type == VALTYPE_FLT64)
    {
      double value = bufr_descriptor_get_dvalue(bufr);
      if (bufr_is_missing_double(value))
        rec.value = kFloatMissing;
      else
        rec.value = value;
    }

    else if (bufr->value->type == VALTYPE_STRING)
    {
      int len = 0;
      char *str = bufr_descriptor_get_svalue(bufr, &len);
      if (str && !bufr_is_missing_string(str, len))
        rec.svalue = str;
      // rec.value will remain to be NaN
    }
  }  // if(bufr->value)
}

// ----------------------------------------------------------------------
/*!
 * \brief Test message validity
 */
// ----------------------------------------------------------------------

bool message_looks_valid(const Message &msg)
{
  // Validate year
  auto yy = msg.find(4001);
  auto mm = msg.find(4002);
  auto dd = msg.find(4003);
  auto hh = msg.find(4004);
  auto mi = msg.find(4005);

  if (yy == msg.end() || mm == msg.end() || dd == msg.end() || hh == msg.end() || mi == msg.end())
    return false;

  if (yy->second.value < 1900 || yy->second.value > 2200)
    return false;
  if (mm->second.value < 1 || mm->second.value > 12)
    return false;
  if (dd->second.value < 1 || dd->second.value > 31)
    return false;
  if (hh->second.value < 0 || hh->second.value > 23)
    return false;
  if (mi->second.value < 0 || mi->second.value > 59)
    return false;

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract aircraft reg. nr or other identification from amdar message
 *        or station ident from sounding message
 */
// ----------------------------------------------------------------------

void get_ident(const Message &msg, const MessageReMap &remap, std::string &ident)
{
  ident.clear();

  auto codes = remap.find(REMAP_IDENT);

  for (const auto code : codes->second)
  {
    auto iit = msg.find(code);

    if ((iit != msg.end()) &&
        *(iit->second.svalue.c_str() + strspn(iit->second.svalue.c_str(), " ")))
    {
      ident = Fmi::trim_copy(iit->second.svalue);
      return;
    }
  }
}

Message::iterator get_ident(Message &msg, const MessageReMap &remap, std::string &ident)
{
  ident.clear();

  auto iit = msg.end();

  auto codes = remap.find(REMAP_IDENT);

  for (const auto code : codes->second)
  {
    iit = msg.find(code);

    if ((iit != msg.end()) &&
        *(iit->second.svalue.c_str() + strspn(iit->second.svalue.c_str(), " ")))
    {
      ident = Fmi::trim_copy(iit->second.svalue);
      break;
    }
  }

  return (ident.empty() ? msg.end() : iit);
}

// ----------------------------------------------------------------------
/*!
 * \brief Accept only amdars with aircraft reg. nr or other identification if option -I is used
 */
// ----------------------------------------------------------------------

bool message_has_ident(const Message &msg, std::string &ident)
{
  get_ident(msg, options.messageremap, ident);

  return !ident.empty();
}

// ----------------------------------------------------------------------
/*!
 * \brief Accept only significant sounding levels if option -S is used
 */
// ----------------------------------------------------------------------

bool message_is_significant(const Message &msg)
{
  // Keep all levels if --insignificant was used
  if (options.insignificant)
    return true;

  // If there is no vertical significance value in the message, keep it
  auto sig = msg.find(8042);  // extended vertical sounding sig.
  if (sig == msg.end())
    return true;

  // Significant levels are marked with nonzero values
  return (sig->second.value > 0);
}

// ----------------------------------------------------------------------
/*!
 * \brief Append records from a dataset to a list
 */
// ----------------------------------------------------------------------

static bool replicating = false;  // set to true if FLAG_CLASS31 is encountered

void append_message(Messages &messages, BUFR_Dataset *dts, BUFR_Tables *tables)
{
  int nsubsets = bufr_count_datasubset(dts);

  int replication_count = -1;  // value of that descriptor
  int replicating_desc = -1;   // the id of the descriptor following above
  Message replicated_message;  // the message when replication starts

  int nmax = (options.subsets ? nsubsets : 1);

  for (int i = 0; i < nmax; i++)
  {
    Message message;
    DataSubset *subset = bufr_get_datasubset(dts, i);
    int ndescriptors = bufr_datasubset_count_descriptor(subset);

    if (options.debug)
      std::cout << "Subset " << i + 1 << " has " << ndescriptors << " descriptors" << std::endl;

    // Loop over the descriptors

    for (int j = 0; j < ndescriptors; j++)
    {
      BufrDescriptor *bufr = bufr_datasubset_get_descriptor(subset, j);

      if (bufr->flags & FLAG_CLASS31)
      {
        replicating = true;
        replicated_message = message;
      }

      if (bufr->flags & FLAG_SKIPPED)
        continue;

      int desc = (bufr->s_descriptor != 0 ? bufr->s_descriptor : bufr->descriptor);

      // Begin recording the info
      record rec;
      extract_record_name_and_units(rec, desc, bufr, tables);
      extract_record_value(rec, bufr);

      // std::cout << desc << " " << rec.name << " = " << rec.value << std::endl;

      if (replicating && replicating_desc < 0)
      {
        if (!(bufr->flags & FLAG_CLASS31))
        {
          replicating_desc = desc;
        }
        else
        {
          replication_count = static_cast<int>(rec.value);
          replicated_message = message;
        }
      }
      else if (desc == replicating_desc)
      {
        if (!message.empty() && message_looks_valid(message))
        {
          if (message_is_significant(message))
            messages.push_back(message);
        }

        message = replicated_message;
        message[desc] = rec;

        if (--replication_count <= 0)
        {
          replicating = false;
          replicating_desc = -1;
        }
      }
      else
      {
        message[desc] = rec;
      }
    }

    if (!message.empty())
    {
      if (!message_looks_valid(message))
        message.clear();
      else if (message_is_significant(message))
        messages.push_back(message);
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Read one bufr message
 */
// ----------------------------------------------------------------------

void read_message(const std::string &filename,
                  Messages &messages,
                  BUFR_Tables *file_tables,
                  LinkedList *tables_list,
                  std::set<int> &datacategories)
{
  // Open the file

  FILE *bufr = fopen(filename.c_str(),
                     "rb");  // VC++ vaatii että avataan binäärisenä (Linuxissa se on default)
  if (bufr == nullptr)
  {
    throw std::runtime_error("Could not open BUFR file '" + filename + "' reading");
  }

  if (options.debug)
    std::cout << "Processing file '" << filename << "'" << std::endl;

  // Read the messages one by one

  int count = 0;

  BUFR_Message *msg = nullptr;

  while (bufr_read_message(bufr, &msg) > 0)
  {
    ++count;

    // If a particular message is wanted, skip all other messages
    if (options.messagenumber != 0 && count != options.messagenumber)
    {
      bufr_free_message(msg);
      if (options.debug)
        std::cout << "Skipping message number " << count << std::endl;
      continue;
    }

    // Try to parse a single message. If it fails, skip to the next one.

    try
    {
      // Print message headers

      if (options.verbose)
      {
        std::cout << "MESSAGE NUMBER " << count << std::endl;
        bufr_print_message(msg, bufr_print_output);
      }

      // Save data category if necessary

      if (options.category.empty())
        datacategories.insert(msg->s1.msg_type);
      else if (msg->s1.msg_type == data_category(options.category))
        datacategories.insert(msg->s1.msg_type);
      else
      {
        bufr_free_message(msg);
        if (options.debug)
          std::cout << "Message " << count << " in " << filename << " is not of desired category"
                    << std::endl;
        continue;
      }

      // Use default tables first

      BUFR_Tables *use_tables = file_tables;

      // Try to find another if not compatible

      if (use_tables->master.version != msg->s1.master_table_version)
        use_tables = bufr_use_tables_list(tables_list, msg->s1.master_table_version);

      // Read the dataset

      BUFR_Dataset *dts = nullptr;
      if (use_tables == nullptr)
      {
        // dts = nullptr;  // is already null
        std::cerr << "Warning: No BUFR table version " << msg->s1.master_table_version
                  << " available" << std::endl;
      }
      else
      {
        dts = bufr_decode_message(msg, use_tables);
        if (options.verbose)
        {
          std::cout << "Decoding message version " << msg->s1.master_table_version
                    << " with BUFR tables version " << use_tables->master.version << std::endl;
        }
      }

      if (dts == nullptr)
      {
        bufr_free_message(msg);
        // continuing at this point may cause a segmentation fault
        throw std::runtime_error("Could not decode message " + std::to_string(count));
      }
      if (dts->data_flag & BUFR_FLAG_INVALID)
      {
        bufr_free_message(msg);
        // continuing at this point may cause a segmentation fault
        throw std::runtime_error("Message number " + std::to_string(count) + " is invalid");
      }

      // Search for local table updates

      if (bufr_contains_tables(dts))
      {
        BUFR_Tables *tables = bufr_extract_tables(dts);
        if (tables != nullptr)
        {
          bufr_tables_list_merge(tables_list, tables);
          bufr_free_tables(tables);
        }
      }

      append_message(messages, dts, file_tables);

      // Done with the current message

      bufr_free_dataset(dts);
      bufr_free_message(msg);
    }
    catch (std::exception &e)
    {
      std::cerr << "Warning: " << e.what() << "  ...skipping to next message in '" << filename
                << "'" << std::endl;
    }
  }

  fclose(bufr);
}

// ----------------------------------------------------------------------
/*!
 * \brief Read all bufr messages
 */
// ----------------------------------------------------------------------

std::pair<BufrDataCategory, Messages> read_messages(const std::list<std::string> &files)
{
  Messages messages;

  // We verify that there is only one type of message
  std::set<int> datacategories;

  // Load CMC Table B and D

  BUFR_Tables *file_tables = bufr_create_tables();
  bufr_load_cmc_tables(file_tables);

  // Load all tables into a list

  int tablenos[2] = {13, 0};
  LinkedList *tables_list = bufr_load_tables_list(getenv("BUFR_TABLES"), tablenos, 1);

  // Add version 14 to the list

  lst_addfirst(tables_list, lst_newnode(file_tables));

  // Load local tables to the list (if they are used)

  if (!options.localtableB.empty() || !options.localtableD.empty())
  {
    if (options.verbose)
      std::cout << "Reading local tables B and D" << std::endl;

    char *tableB = const_cast<char *>(options.localtableB.c_str());
    char *tableD = const_cast<char *>(options.localtableD.c_str());

    // This prints a warning if both tableB and tableD are 0.
    // Never tested what happens if only one is given.

    bufr_tables_list_addlocal(tables_list, tableB, tableD);
  }

  // Process the files

  int succesful_parse_events = 0;
  int errorneous_parse_events = 0;

  BOOST_FOREACH (const std::string &file, files)
  {
    // Reset for each file
    replicating = false;

    try
    {
      read_message(file, messages, file_tables, tables_list, datacategories);
      succesful_parse_events++;
    }
    catch (std::exception &e)
    {
      errorneous_parse_events++;
      std::cerr << "Warning: " << e.what() << std::endl;
    }
    catch (...)
    {
      errorneous_parse_events++;
      std::cerr << "Warning: Failed to interpret message '" << file << "'" << std::endl;
    }
  }

  if (datacategories.size() == 0)
    throw std::runtime_error("Failed to find any bufr data categories");

  // Cannot handle soundings and other data simultaneously

  if (datacategories.size() > 1)
  {
    std::list<std::string> names;
    BOOST_FOREACH (int tmp, datacategories)
      names.push_back(data_category_name(BufrDataCategory(tmp)));
    throw std::runtime_error("BURF messages contain multiple data categories (" +
                             boost::algorithm::join(names, ",") +
                             "), please use the -C or --category option to select one");
  }

  if (options.verbose)
  {
    std::cout << "Examined " << files.size() << " files" << std::endl
              << "Succesfully parsed " << succesful_parse_events << " files" << std::endl
              << "Failed to parse " << errorneous_parse_events << " files" << std::endl;
  }

  return std::make_pair(BufrDataCategory(*datacategories.begin()), messages);
}

// ----------------------------------------------------------------------
/*!
 * \brief Collect unique parameter names from the records
 */
// ----------------------------------------------------------------------

std::set<std::string> collect_names(const Messages &messages)
{
  std::set<std::string> names;
  BOOST_FOREACH (const Message &msg, messages)
  {
    BOOST_FOREACH (const Message::value_type &value, msg)
    {
      if (options.usebufrname)
        names.insert(value.second.name);
      else
        names.insert(fmt::format("{:0>6}", value.first));
    }
  }
  return names;
}

// ----------------------------------------------------------------------
/*!
 * \brief Map the parameters to newbase names
 */
// ----------------------------------------------------------------------

NameMap map_names(const std::set<std::string> &names, const NameMap &pmap)
{
  NameMap namemap;
  BOOST_FOREACH (const ::std::string &name, names)
  {
    auto it = pmap.find(name);
    if (it != pmap.end())
    {
      namemap.insert(NameMap::value_type(name, it->second));
      if (options.debug)
        std::cout << name << " = " << it->second.parId << std::endl;
    }
    else if (options.debug)
    {
      std::cout << name << " = UNKNOWN" << std::endl;
    }
  }

  return namemap;
}

// ----------------------------------------------------------------------
/*!
 * \brief Utility struct for converting CSV rows into a map
 *
 * For some reason RHEL6 refused to compile the code when the
 * struct was local to read_bufr_config.
 */
// ----------------------------------------------------------------------

struct CsvConfig
{
  NameMap pmap;

  void add(const Fmi::CsvReader::row_type &row)
  {
    if (row.size() == 0)
      return;
    if (row[0].substr(0, 1) == "#")
      return;

    if (row.size() != 4)
    {
      std::ostringstream msg;
      msg << "Invalid row of size " << row.size() << " in '" << options.conffile << "':";
      std::ostream_iterator<std::string> out_it(msg, ",");
      std::copy(row.begin(), row.end(), out_it);
      throw std::runtime_error(msg.str());
    }

    ParNameInfo parInfo;
    parInfo.parId = static_cast<FmiParameterName>(converter.ToEnum(row[2]));
    parInfo.bufrId = row[0];
    parInfo.bufrName = row[1];
    parInfo.shortName = row[3];
    if (options.usebufrname)
      pmap.insert(NameMap::value_type(parInfo.bufrName, parInfo));
    else
      pmap.insert(NameMap::value_type(parInfo.bufrId, parInfo));
  }
};

// ----------------------------------------------------------------------
/*!
 * \brief Read the BUFR parameter conversion file
 */
// ----------------------------------------------------------------------

NameMap read_bufr_config()
{
  CsvConfig csv;
  if (!options.conffile.empty())
  {
    if (options.verbose)
      std::cout << "Reading " << options.conffile << std::endl;
    Fmi::CsvReader::read(options.conffile, boost::bind(&CsvConfig::add, &csv, _1));
  }
  return csv.pmap;
}

// ----------------------------------------------------------------------
/*!
 * \brief Read the station description file
 */
// ----------------------------------------------------------------------

NFmiAviationStationInfoSystem read_station_csv()
{
  if (options.verbose)
    std::cout << "Reading " << options.stationsfile << std::endl;

  bool wmostations = true;
  NFmiAviationStationInfoSystem stations(wmostations, options.verbose);

  if (options.stationsfile.empty())
    return stations;

  stations.InitFromMasterTableCsv(options.stationsfile);

  return stations;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the parameter descriptor
 */
// ----------------------------------------------------------------------

NFmiParamDescriptor create_pdesc(const NameMap &namemap, BufrDataCategory category)
{
  NFmiParamBag pbag;

  BOOST_FOREACH (const NameMap::value_type &values, namemap)
  {
    NFmiParam p(values.second.parId, values.second.shortName);
    p.InterpolationMethod(kLinearly);
    pbag.Add(NFmiDataIdent(p));
  }

  // Special case additions
  if (category == kBufrUpperAirLevel || category == kBufrSeaSurface)
  {
    // AMDAR querydata must record location as extra
    // BUOY/SHIP  querydata must record location as extra
    pbag.Add(NFmiDataIdent(NFmiParam(kFmiLongitude, "LONGITUDE")));
    pbag.Add(NFmiDataIdent(NFmiParam(kFmiLatitude, "LATITUDE")));
  }

  return NFmiParamDescriptor(pbag);
}

// ----------------------------------------------------------------------
/*!
 * \brief Count maximum number of sounding levels among the stations
 *
 * This is probably not the proper way to do this, but it seems to work.
 *
 * 1002 WMO STATION NUMBER
 */
// ----------------------------------------------------------------------

int count_sounding_levels(const Messages &messages)
{
  int wmo_station = 0;
  int max_levels = 0;
  int levels = 0;
  BOOST_FOREACH (const Message &msg, messages)
  {
    ++levels;

    BOOST_FOREACH (const Message::value_type &values, msg)
    {
      if (values.first == 1002)  // does wmo station change?
      {
        if (values.second.value != wmo_station)
        {
          wmo_station = static_cast<int>(values.second.value);
          max_levels = std::max(max_levels, levels);
          levels = 1;  // this message defines one level, so do not go to zero
          break;
        }
      }
    }
  }

  max_levels = std::max(max_levels, levels);

  return max_levels;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the vertical place descriptor
 *
 */
// ----------------------------------------------------------------------

NFmiVPlaceDescriptor create_vdesc(const Messages &messages,
                                  BufrDataCategory category,
                                  size_t levelcount)
{
  switch (category)
  {
    case kBufrLandSurface:
    case kBufrSeaSurface:
    case kBufrSynoptic:
      return NFmiVPlaceDescriptor();
    case kBufrUpperAirLevel:
    {
      NFmiLevelBag lbag;

      if (options.requireident)
      {
        // Given level count was set to max number of messages/times per amdar.
        // The data is in time order when filled to levels

        for (size_t i = 0; i < levelcount; ++i)
        {
          char name[16];
          sprintf(name, "%lu", i);
          std::string levelName(name);
          lbag.AddLevel(NFmiLevel(kFmiAmdarLevel, levelName.c_str(), static_cast<float>(i)));
        }
      }
      else
        lbag.AddLevel(NFmiLevel(kFmiAmdarLevel, "AMDAR", 0));

      return NFmiVPlaceDescriptor(lbag);
    }
    case kBufrSounding:
    {
      // We number the levels sequentially. The number
      // with most measurements determines the total number of levels.

      int levels = count_sounding_levels(messages);

      if (levels == 0)
        return NFmiVPlaceDescriptor();

      NFmiLevelBag lbag;
      for (int i = 1; i <= levels; ++i)
        lbag.AddLevel(NFmiLevel(kFmiSoundingLevel, "SoundingLevel", static_cast<float>(i)));

      return NFmiVPlaceDescriptor(lbag);
    }
    case kBufrSatSounding:
    case kBufrSatUpperAirLevel:
    case kBufrRadar:
    case kBufrPhysical:
    case kBufrDispersal:
    case kBufrRadiological:
    case kBufrTables:
    case kBufrSatSurface:
    case kBufrRadiances:
    case kBufrOceanographic:
    case kBufrImage:
    default:
      throw std::runtime_error("Cannot handle data category: " + data_category_name(category));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Definition of the dummy AMDAR (airplane) station
 */
// ----------------------------------------------------------------------

NFmiStation get_amdar_station()
{
  const int dummy_wmo = 0;  // Does any country have WMO-block zero??

  const float lon = kFloatMissing;
  const float lat = kFloatMissing;
  const char *name = "AMDAR Dummy Station";

  return NFmiStation(dummy_wmo, name, lon, lat);
}

// ----------------------------------------------------------------------
/*!
 * \brief Horizontal descriptor for AMDAR data
 *
 * Smartmet editor recognizes AMDAR data from the level type indicator.
 * It will assume there is just one dummy station pretending to be
 * all the airplanes at once.
 */
// ----------------------------------------------------------------------

NFmiHPlaceDescriptor create_hdesc_amdar()
{
  NFmiLocationBag lbag;
  lbag.AddLocation(get_amdar_station());
  return NFmiHPlaceDescriptor(lbag);
}

// ----------------------------------------------------------------------
/*!
 * \brief Horizontal descriptor for BUOY/SHIP data
 *
 * These stations do not have numeric IDs, but CCITT IA5 identifier strings.
 * We will number the stations sequentially, and use the first found coordinate
 * in the descriptor.
 *
 * 1005 SHIP OR MOBILE LAND STATION IDENTIFIER
 * 5001 LATITUDE(HIGH ACCURACY)
 * 6001 LONGITUDE (HIGH ACCURACY)
 *
 * 1011 SHIP OR MOBILE LAND STATION IDENTIFIER
 * 5002 LATITUDE(COARSE ACCURACY)
 * 6002 LONGITUDE (COARSE ACCURACY)
 */
// ----------------------------------------------------------------------

NFmiHPlaceDescriptor create_hdesc_buoy_ship(const Messages &messages)
{
  // First list all unique station IDs

  using Stations = std::map<std::string, NFmiPoint>;
  Stations stations;

  BOOST_FOREACH (const Message &msg, messages)
  {
    auto p_id = msg.find(1005);
    if (p_id == msg.end())
      p_id = msg.find(1011);

    if (p_id != msg.end())
    {
      const std::string name = p_id->second.svalue;

      float lon = kFloatMissing;
      float lat = kFloatMissing;

      p_id = msg.find(5001);
      if (p_id == msg.end())
        p_id = msg.find(5002);
      if (p_id != msg.end())
        lat = static_cast<float>(p_id->second.value);

      p_id = msg.find(6001);
      if (p_id == msg.end())
        p_id = msg.find(6002);
      if (p_id != msg.end())
        lon = static_cast<float>(p_id->second.value);

      if (lon != kFloatMissing && (lon < -180 || lon > 180))
      {
        if (options.debug)
          std::cerr << "Warning: BUFR message contains a station with longitude out of bounds "
                       "[-180,180]: "
                    << lon << std::endl;
      }
      else if (lat != kFloatMissing && (lat < -90 || lat > 90))
      {
        if (options.debug)
          std::cerr
              << "Warning: BUFR message contains a station with latitude out of bounds [-90,90]: "
              << lat << std::endl;
      }
      else
        stations.insert(std::make_pair(name, NFmiPoint(lon, lat)));
    }
  }

  // Then build the descriptor

  NFmiLocationBag lbag;
  int number = 0;
  BOOST_FOREACH (const Stations::value_type &name_coord, stations)
  {
    ++number;

    NFmiStation tmp(number, name_coord.first, name_coord.second.X(), name_coord.second.Y());
    lbag.AddLocation(tmp);
  }

  return NFmiHPlaceDescriptor(lbag);
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract station from a record
 *
 * 001001 WMO BLOCK NUMBER
 * 001002 WMO STATION NUMBER
 * 005001 LATITUDE(HIGH ACCURACY)
 * 006001 LONGITUDE(HIGH ACCURACY)
 */
// ----------------------------------------------------------------------

NFmiStation get_station(const Message &msg)
{
  auto wmoblock = msg.find(1001);
  auto wmonumber = msg.find(1002);
  auto latitude = msg.find(5001);
  auto longitude = msg.find(6001);

  if (wmoblock == msg.end() || wmonumber == msg.end())
  {
    if (options.debug)
      std::cerr << "Warning: BUFR message contains a station without a WMO number" << std::endl;
    return NFmiStation(-1, "", kFloatMissing, kFloatMissing);
  }

  int wmo = static_cast<int>(1000 * wmoblock->second.value + wmonumber->second.value);

  float lon =
      static_cast<float>((longitude == msg.end() ? kFloatMissing : longitude->second.value));
  float lat = static_cast<float>((latitude == msg.end() ? kFloatMissing : latitude->second.value));

  if (lon != kFloatMissing && (lon < -180 || lon > 180))
  {
    if (options.debug)
      std::cerr
          << "Warning: BUFR message contains a station with longitude out of bounds [-180,180]: "
          << lon << std::endl;
    lon = kFloatMissing;
  }

  if (lat != kFloatMissing && (lat < -90 || lat > 90))
  {
    if (options.debug)
      std::cerr << "Warning: BUFR message contains a station with latitude out of bounds [-90,90]: "
                << lat << std::endl;
    lat = kFloatMissing;
  }

  return NFmiStation(wmo, std::to_string(wmo), lon, lat);
}

// ----------------------------------------------------------------------
/*!
 * \brief Rough estimate on the accuracy of the coordinates
 */
// ----------------------------------------------------------------------

int accuracy_estimate(const NFmiPoint &coord)
{
  auto str = fmt::format("{}{}", coord.X(), coord.Y());
  return str.size();
}

// ----------------------------------------------------------------------
/*!
 *  \brief Create horizontal place descriptor
 *
 * For some reason NFmiAviationStationInfoSystem::FindStation is not const
 */
// ----------------------------------------------------------------------

NFmiHPlaceDescriptor create_hdesc(const Messages &messages,
                                  NFmiAviationStationInfoSystem &stationinfos,
                                  BufrDataCategory category)
{
  // AMDAR descriptor is special (airplane measurements)

  if (category == kBufrUpperAirLevel)
    return create_hdesc_amdar();

  if (category == kBufrSeaSurface)
    return create_hdesc_buoy_ship(messages);

  // First list all unique stations

  std::map<long, NFmiStation> stations;

  for (const Message &msg : messages)
  {
    NFmiStation station = get_station(msg);

    if (station.GetLongitude() != kFloatMissing && station.GetLatitude() != kFloatMissing)
    {
      auto pos = stations.insert(std::make_pair(station.GetIdent(), station));
      // Handle stations with different coordinates
      auto &iter = pos.first;
      if (!pos.second && iter->second.GetLocation() != station.GetLocation())
      {
        auto &oldstation = iter->second;
        auto acc1 = accuracy_estimate(oldstation.GetLocation());
        auto acc2 = accuracy_estimate(station.GetLocation());
        // Use the one which seems to have more significant decimals
        if (acc1 < acc2)
          oldstation = station;
      }
    }
  }

  // Then build the descriptor. The message may contain no name for the
  // station, hence we use the stations file to name the stations if
  // possible.

  NFmiLocationBag lbag;
  for (const auto &id_station : stations)
  {
    const auto &station = id_station.second;

    NFmiAviationStation *stationinfo = stationinfos.FindStation(station.GetIdent());

    if (stationinfo == nullptr)
    {
      // Cannot get a good name for the station
      lbag.AddLocation(station);
    }
    else
    {
      // we use fixed coordinates instead of message ones, since there is often some
      // variation in the message coordinates
      NFmiStation tmp(station.GetIdent(),
                      stationinfo->GetName().CharPtr(),
                      station.GetLongitude(),
                      station.GetLatitude());
      lbag.AddLocation(tmp);
    }
  }

  return NFmiHPlaceDescriptor(lbag);
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract valid time from a message
 *
 * 004001 YEAR
 * 004002 MONTH
 * 004003 DAY
 * 004004 HOUR
 * 004005 MINUTE
 */
// ----------------------------------------------------------------------

NFmiMetTime get_validtime(const Message &msg)
{
  auto yy_i = msg.find(4001);
  auto mm_i = msg.find(4002);
  auto dd_i = msg.find(4003);
  auto hh_i = msg.find(4004);
  auto mi_i = msg.find(4005);
  auto ss_i = (options.requireident ? msg.find(4006) : msg.end());

  if (yy_i == msg.end() || mm_i == msg.end() || dd_i == msg.end() || hh_i == msg.end() ||
      mi_i == msg.end())
    throw std::runtime_error("Message does not contain all required date/time fields");

  auto yy = yy_i->second.value;
  auto mm = mm_i->second.value;
  auto dd = dd_i->second.value;
  auto hh = hh_i->second.value;
  auto mi = mi_i->second.value;
  auto ss = ((ss_i != msg.end()) ? ss_i->second.value : 0);

  const int timeresolution = (options.roundtohours > 0 ? 60 * options.roundtohours : 1);

  NFmiMetTime t(static_cast<short>(yy),
                static_cast<short>(mm),
                static_cast<short>(dd),
                static_cast<short>(hh),
                static_cast<short>(mi),
                0,
                timeresolution);

  if ((ss > 0) && (timeresolution == 1))
    t.SetSec(ss);

  if (yy < 1900 || mm < 1 || mm > 12 || dd < 1 || dd > 31 || hh < 0 || hh > 23 || mi < 0 ||
      mi > 60 || ss < 0 || ss > 59 || dd > NFmiMetTime::DaysInMonth(mm, yy))
    throw std::runtime_error("Message contains a date whose components are out of range");

  return t;
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish valid time for individual AMDAR
 *
 * Without -I option, each message is expected to get a unique time. If there is a previous
 * identical time, the next message gets a time that is incremented
 * by one second. This is a requirement of the smartmet editor.
 *
 * With -I option, each message for given amdar gets the same (earliest) time.
 *
 * 4001 YEAR
 * 4002 MONTH
 * 4003 DAY
 * 4004 HOUR
 * 4005 MINUTE
 */
// ----------------------------------------------------------------------

NFmiMetTime get_validtime_amdar(std::set<NFmiMetTime> &used_times,
                                const Message &message,
                                std::string &ident,
                                bool requireident = false,
                                bool matchident = false,
                                const IdentTimeMap &identtimemap = IdentTimeMap())
{
  // If selected/stored validtime is required and already exists for the amdar, use it

  if (requireident && message_has_ident(message, ident))
  {
    auto tit = identtimemap.find(ident);

    if (tit != identtimemap.end())
      return tit->second;

    if (matchident)
      throw std::runtime_error(std::string("Internal error, no times for ident ") + ident);
  }
  else if (requireident)
    throw std::runtime_error("Internal error, message has no ident");

  // We permit the seconds to be missing, since that seems
  // to be the case in actual messages
  short year = -1;
  short month = -1;
  short day = -1;
  short hour = -1;
  short minute = -1;
  short second = 0;

  BOOST_FOREACH (const Message::value_type &values, message)
  {
    // shorthand variables
    const int descriptor = values.first;
    const record &rec = values.second;

    switch (descriptor)
    {
      case 4001:
        year = static_cast<short>(rec.value);
        break;
      case 4002:
        month = static_cast<short>(rec.value);
        break;
      case 4003:
        day = static_cast<short>(rec.value);
        break;
      case 4004:
        hour = static_cast<short>(rec.value);
        break;
      case 4005:
        minute = static_cast<short>(rec.value);
        break;
      case 4006:
        second = static_cast<short>(rec.value);
        break;
      default:
        break;
    }
  }

  if (year < 0 || month < 0 || day < 0 || hour < 0 || minute < 0)
    throw std::runtime_error("Insufficient date information in AMDAR message");

  const int timestep = (options.roundtohours > 0 ? 60 * options.roundtohours : 1);
  NFmiMetTime t(year, month, day, hour, minute, second, timestep);

  if (requireident)
  {
    // Use the time as is, just set the seconds (see API comment below)

    if (timestep == 1)
      t.SetSec(second);

    return t;
  }

  // Add up seconds until there is no previous such time.
  // The while test fails if the same time was already in the set.

  while (!used_times.insert(t).second)
  {
    // You cannot construct this correctly as the API implies, the
    // time is always rounded to even minutes. Since the API still
    // takes seconds as input, this should be considered a bug
    // in newbase.
    t = NFmiMetTime(year, month, day, hour, minute, ++second, timestep);

    // Hence you need this to fix it:
    t.SetSec(second);

    // And if there are more than 60 planes, there's no saying what could happen
    if (second >= 60)
      throw std::runtime_error("Cannot handle more than 60 AMDAR measurements for the same minute");
  }

  return t;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create AMDAR or sounding time descriptor when run with -I
 *
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor create_tdesc_ident(const Messages & /*messages*/,
                                      const TimeIdentList &timeidentlist,
                                      const IdentTimeMap &identtimemap)
{
  // Use the earliest message time for all messages/data for each amdar/sounding

  NFmiTimeList tlist;

  if (options.debug)
  {
    BOOST_FOREACH (auto const &identtime, identtimemap)
    {
      fprintf(stderr,
              "IdentTime %s %s\n",
              identtime.first.c_str(),
              to_iso_string(identtime.second.PosixTime()).c_str());
    }
  }

  BOOST_FOREACH (auto const &timeident, timeidentlist)
  {
    auto it = identtimemap.find(timeident);
    tlist.Add(new NFmiMetTime(it->second, true));

    if (options.debug)
      fprintf(stderr,
              "TimeList %s %s\n",
              it->first.c_str(),
              to_iso_string(it->second.PosixTime()).c_str());
  }

  NFmiMetTime origintime;
  return NFmiTimeDescriptor(origintime, tlist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Create AMDAR time descriptor
 *
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor create_tdesc_amdar(const Messages &messages)
{
  NFmiTimeList tlist;

  // Times used so far

  std::set<NFmiMetTime> validtimes;
  std::string ident;

  BOOST_FOREACH (const Message &msg, messages)
  {
    // Return value can be ignored here
    get_validtime_amdar(validtimes, msg, ident);
  }

  // Then the final timelist

  BOOST_FOREACH (const NFmiMetTime &t, validtimes)
  {
    tlist.Add(new NFmiMetTime(t));
  }

  NFmiMetTime origintime;
  return NFmiTimeDescriptor(origintime, tlist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Create time descriptor
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor create_tdesc(const Messages &messages,
                                BufrDataCategory category,
                                const TimeIdentList &timeidentlist,
                                const IdentTimeMap &identtimemap)
{
  // Special cases

  if (options.requireident && ((category == kBufrUpperAirLevel) || (category == kBufrSounding)))
    return create_tdesc_ident(messages, timeidentlist, identtimemap);
  else if (category == kBufrUpperAirLevel)
    return create_tdesc_amdar(messages);

  // Normal cases

  std::set<NFmiMetTime> validtimes;
  BOOST_FOREACH (const Message &msg, messages)
  {
    try
    {
      validtimes.insert(get_validtime(msg));
    }
    catch (const std::exception &e)
    {
      std::cerr << "Skipping errorneous valid time: " << e.what() << std::endl;
    }
  }

  NFmiTimeList tlist;
  BOOST_FOREACH (const NFmiMetTime &t, validtimes)
  {
    tlist.Add(new NFmiMetTime(t));
  }
  NFmiMetTime origintime;
  return NFmiTimeDescriptor(origintime, tlist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Normalize a record value
 */
// ----------------------------------------------------------------------

float normal_value(const record &rec)
{
  if (rec.value == kFloatMissing)
    return kFloatMissing;

  // Kelvin to Celsius
  if (rec.units == "K")
    return static_cast<float>(rec.value - 273.15);

  // Pascal to hecto-Pascal
  if (rec.units == "PA")
    return static_cast<float>(rec.value / 100.0);

  // Cloud 8ths to 0-100%. Note that obs may also be 9, hence a min check is needed
  if (!options.totalcloudoctas && rec.name == "CLOUD AMOUNT" && rec.units == "CODE TABLE")
    return static_cast<float>(std::min(100.0, rec.value * 100 / 8));

  if (rec.name.find("PRESENT WEATHER") != std::string::npos && rec.units == "CODE TABLE")
  {
    // 508 No significant phenomenon to report, present and past weather omitted

    if (floor(rec.value + 0.5) == 508)
      return 0;
    else if (rec.value < 0 || floor(rec.value + 0.5) >= 200)
      return kFloatMissing;
  }

  return static_cast<float>(rec.value);
}

// ----------------------------------------------------------------------
/*!
 * \brief All other iterators being still, copy parameters
 */
// ----------------------------------------------------------------------

void copy_params(NFmiFastQueryInfo &info, const Message &msg, const NameMap &namemap)
{
  BOOST_FOREACH (const Message::value_type &value, msg)
  {
    auto key = options.usebufrname ? value.second.name : fmt::format("{:0>6}", value.first);
    auto it = namemap.find(key);
    if (it != namemap.end())
    {
      if (!info.Param(it->second.parId))
        throw std::runtime_error("Internal error in handling parameters of the messages");
      info.FloatValue(normal_value(value.second));
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Copy values from the records
 */
// ----------------------------------------------------------------------

void copy_records_sounding(NFmiFastQueryInfo &info,
                           const Messages &messages,
                           const NameMap &namemap)
{
  info.First();

  // Remember last station. We start the sounding level from
  // scratch when the station changes. Last levels may be unfilled
  // for stations which do not measure as many levels as the one
  // that did the maximum number.

  NFmiStation laststation;
  NFmiMetTime t;
  std::string ident;
  std::string lastident;

  BOOST_FOREACH (const Message &msg, messages)
  {
    try
    {
      NFmiStation station = get_station(msg);

      // We ignore stations with invalid coordinates
      if (!info.Location(station.GetIdent()))
        continue;

      if (options.requireident)
      {
        // Messages have generated ident to identify soundings

        get_ident(msg, options.messageremap, ident);

        bool identchange = (ident != lastident);

        if (identchange || options.debug)
        {
          // Time rounded to nearest hour as when stored to tdesc

          t = get_validtime(msg);
          t.NearestMetTime(60);
        }

        if (identchange)
        {
          info.ResetLevel();

          if (!info.Time(t))
            throw std::runtime_error("Internal error in copying soundings");

          if (options.debug)
            fprintf(stderr,
                    "%s %s reset %s %lu\n",
                    ident.c_str(),
                    to_iso_string(t.PosixTime()).c_str(),
                    lastident.c_str(),
                    info.TimeIndex());

          lastident = ident;
        }
        else if (options.debug)
          fprintf(stderr,
                  "%s %s next %lu %lu\n",
                  ident.c_str(),
                  to_iso_string(t.PosixTime()).c_str(),
                  info.TimeIndex(),
                  info.LevelIndex());
      }
      else
      {
        if (!info.Time(get_validtime(msg)))
          throw std::runtime_error("Internal error in copying soundings");

        if (laststation != station)
        {
          info.ResetLevel();
          laststation = station;
        }
      }

      if (!info.NextLevel())
        throw std::runtime_error("Changing to next level failed");

      copy_params(info, msg, namemap);
    }
    catch (...)
    {
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Copy values from the records
 *
 * 5001 LATITUDE(HIGH ACCURACY)
 * 6001 LONGITUDE(HIGH ACCURACY)
 */
// ----------------------------------------------------------------------

void copy_records_amdar(NFmiFastQueryInfo &info,
                        const Messages &messages,
                        const NameMap &namemap,
                        const IdentTimeMap &identtimemap)
{
  info.First();

  if (options.requireident)
    info.ResetTime();

  // Valid times used so far. Algorithm must match create_tdesc at this point.

  std::set<NFmiMetTime> validtimes;
  std::string ident;
  std::string lastident;

  BOOST_FOREACH (const Message &msg, messages)
  {
    // Without -I option:
    // There is only one station and only one level, so the initial
    // info.First() is sufficient for setting both correctly.
    // However, the time is different for each message, possibly by
    // artificially added seconds.
    //
    // With -I option:
    // There is only one station. The level is different and the time is same
    // for amdar's each message.

    NFmiMetTime t = get_validtime_amdar(
        validtimes, msg, ident, options.requireident, options.requireident, identtimemap);

    if (options.requireident)
    {
      if (ident != lastident)
      {
        info.ResetLevel();

        if (!info.NextTime())
          throw std::runtime_error("Changing to next time failed");

        if (options.debug)
          fprintf(stderr,
                  "%s %s reset %s %lu\n",
                  ident.c_str(),
                  to_iso_string(t.PosixTime()).c_str(),
                  lastident.c_str(),
                  info.TimeIndex());

        lastident = ident;
      }
      else if (options.debug)
        fprintf(stderr,
                "%s %s next %lu %lu\n",
                ident.c_str(),
                to_iso_string(t.PosixTime()).c_str(),
                info.TimeIndex(),
                info.LevelIndex());
    }
    else if (!info.Time(t))
      throw std::runtime_error("Internal error in handling valid times of AMDAR message");

    // Copy the extra lon/lat parameters we added in create_pdesc

    float lon = kFloatMissing;
    float lat = kFloatMissing;

    Message::const_iterator it;

    it = msg.find(5001);
    if (it != msg.end())
      lat = static_cast<float>(it->second.value);

    it = msg.find(6001);
    if (it != msg.end())
      lon = static_cast<float>(it->second.value);

    if (lon != kFloatMissing && (lon < -180 || lon > 180))
    {
      if (options.debug)
        std::cerr << "Warning: BUFR message contains a station with longitude out of bounds "
                     "[-180,180]: "
                  << lon << std::endl;
    }
    else if (lat != kFloatMissing && (lat < -90 || lat > 90))
    {
      if (options.debug)
        std::cerr
            << "Warning: BUFR message contains a station with latitude out of bounds [-90,90]: "
            << lat << std::endl;
    }
    else
    {
      if (options.requireident && (!info.NextLevel()))
        throw std::runtime_error("Changing to next level failed");

      info.Param(kFmiLongitude);
      info.FloatValue(lon);

      info.Param(kFmiLatitude);
      info.FloatValue(lat);

      copy_params(info, msg, namemap);
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Copy values from the records
 *
 * 1005 SHIP OR MOBILE LAND STATION IDENTIFIER
 * 5001 LATITUDE(HIGH ACCURACY)
 * 6001 LONGITUDE (HIGH ACCURACY)
 *
 * 1011 SHIP OR MOBILE LAND STATION IDENTIFIER
 * 5002 LATITUDE(COARSE ACCURACY)
 * 6002 LONGITUDE (COARSE ACCURACY)
 */
// ----------------------------------------------------------------------

void copy_records_buoy_ship(NFmiFastQueryInfo &info,
                            const Messages &messages,
                            const NameMap &namemap)
{
  info.First();

  // Remember last station since changing it while copying
  // is a bit expensive

  std::string laststation;

  BOOST_FOREACH (const Message &msg, messages)
  {
    if (!info.Time(get_validtime(msg)))
      throw std::runtime_error("Internal error in handling valid times of the messages");

    // Skip unknown locations
    auto p_id = msg.find(1005);
    if (p_id == msg.end())
    {
      p_id = msg.find(1011);
      if (p_id == msg.end())
        continue;
    }

    const std::string &name = p_id->second.svalue;

    if (laststation != name)
    {
      // Find the station based on the name. If there's no match, the station is invalid and we
      // skip
      // it
      if (!info.Location(name))
        continue;
    }

    // Then copy the extra lon/lat parameters we added in create_pdesc

    float lon = kFloatMissing;
    float lat = kFloatMissing;

    p_id = msg.find(5001);
    if (p_id == msg.end())
      p_id = msg.find(5002);
    if (p_id != msg.end())
      lat = static_cast<float>(p_id->second.value);

    p_id = msg.find(6001);
    if (p_id == msg.end())
      p_id = msg.find(6002);
    if (p_id != msg.end())
      lon = static_cast<float>(p_id->second.value);

    if (lon != kFloatMissing && (lon < -180 || lon > 180))
    {
      if (options.debug)
        std::cerr << "Warning: BUFR message contains a station with longitude out of bounds "
                     "[-180,180]: "
                  << lon << std::endl;
    }
    else if (lat != kFloatMissing && (lat < -90 || lat > 90))
    {
      if (options.debug)
        std::cerr
            << "Warning: BUFR message contains a station with latitude out of bounds [-90,90]: "
            << lat << std::endl;
    }
    else
    {
      laststation = name;

      copy_params(info, msg, namemap);

      info.Param(kFmiLongitude);
      info.FloatValue(lon);

      info.Param(kFmiLatitude);
      info.FloatValue(lat);
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Copy values from the records
 */
// ----------------------------------------------------------------------

void copy_records(NFmiFastQueryInfo &info,
                  const Messages &messages,
                  const NameMap &namemap,
                  BufrDataCategory category,
                  const std::map<std::string, NFmiMetTime> &messageTimes)
{
  // Handle special cases

  if (category == kBufrSounding)
    return copy_records_sounding(info, messages, namemap);

  if (category == kBufrUpperAirLevel)
    return copy_records_amdar(info, messages, namemap, messageTimes);

  if (category == kBufrSeaSurface)
    return copy_records_buoy_ship(info, messages, namemap);

  // Normal case with no funny business with levels or times

  info.First();

  BOOST_FOREACH (const Message &msg, messages)
  {
    try
    {
      if (!info.Time(get_validtime(msg)))
        throw std::runtime_error("Internal error in handling valid times of the messages");

      NFmiStation station = get_station(msg);

      // We ignore stations with bad coordinates
      if (!info.Location(station.GetIdent()))
        continue;

      copy_params(info, msg, namemap);
    }
    catch (...)
    {
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Guess the producer for the data
 */
// ----------------------------------------------------------------------

void guess_producer(BufrDataCategory category)
{
  switch (category)
  {
    case kBufrLandSurface:
      options.producername = "SYNOP";
      options.producernumber = kFmiSYNOP;
      return;
    case kBufrSynoptic:
      options.producername = "SYNOP";
      options.producernumber = kFmiSYNOP;
      return;
    case kBufrSeaSurface:
      options.producername = "BUOY/SHIP";
      options.producernumber = kFmiBUOY;
      return;
    case kBufrSounding:
      options.producername = "AERO";
      options.producernumber = 1032;
      return;
    case kBufrRadar:
      options.producername = "RADAR";
      options.producernumber = kFmiRADARNRD;
      return;

    case kBufrUpperAirLevel:
      options.producername = "AMDAR";
      options.producernumber = 1015;
      return;

    case kBufrSatSounding:
    case kBufrSatUpperAirLevel:
    case kBufrPhysical:
    case kBufrDispersal:
    case kBufrRadiological:
    case kBufrTables:
    case kBufrSatSurface:
    case kBufrRadiances:
    case kBufrOceanographic:
    case kBufrImage:
      std::cerr << "Warning: Could not guess producer for the input data" << std::endl;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract phase of flight from amdar message
 */
// ----------------------------------------------------------------------

Message::const_iterator get_phase_amdar(const Message &msg)
{
  auto codes = options.messageremap.find(REMAP_PHASE);

  for (const auto code : codes->second)
  {
    auto it = msg.find(code);

    if ((it != msg.end()) && ((it->second.value == Flying) || (it->second.value == Takeoff) ||
                              (it->second.value == Landing)))
      return it;
  }

  return msg.end();
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract phase of flight from amdar message and remap bufr code if requested
 */
// ----------------------------------------------------------------------

Phase get_phase_amdar(Message &msg, bool remap, int &code)
{
  // Phase exists, it was checked at earlier state

  auto pit = get_phase_amdar(msg);

  code = pit->first;

  if (!remap)
    return (Phase)pit->second.value;

  auto codes = options.messageremap.find(REMAP_PHASE);
  const int msgcode = pit->first;
  const int primarycode = codes->second.front();

  for (const auto code : codes->second)
  {
    auto it = msg.find(code);

    if (code == primarycode)
    {
      // Primary/first code, no need to remap ?

      if (code == msgcode)
        continue;

      // Erase old row with primary code if it exists

      if (it != msg.end())
        msg.erase(it);

      // Insert new row with primary code

      msg.insert(std::make_pair(code, pit->second));

      msg.erase(pit);
    }
    else if (it != msg.end())
      msg.erase(it);
  }

  return (Phase)pit->second.value;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract altitude from amdar message
 */
// ----------------------------------------------------------------------

Message::const_iterator get_altitude(const Message &msg)
{
  auto codes = options.messageremap.find(REMAP_ALTITUDE);

  if (codes == options.messageremap.end())
    return msg.end();

  for (auto code : codes->second)
  {
    auto it = msg.find(code);

    if ((it != msg.end()) && (it->second.value != kFloatMissing))
      return it;
  }

  return msg.end();
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract altitude from amdar message and remap bufr code if requested
 */
// ----------------------------------------------------------------------

double get_altitude(Message &msg, bool remap)
{
  // Altitude exists, it was checked at earlier state

  auto hit = get_altitude(msg);

  double height = hit->second.value;

  if (!remap)
    return height;

  auto codes = options.messageremap.find(REMAP_ALTITUDE);
  const int msgcode = hit->first;
  const int primarycode = codes->second.front();

  for (auto const code : codes->second)
  {
    auto it = msg.find(code);

    if (code == primarycode)
    {
      // Primary/first code, no need to remap ?

      if (code == msgcode)
        continue;

      // Erase old row with primary code if it exists

      if (it != msg.end())
        msg.erase(it);

      // Insert new row with primary code

      msg.insert(std::make_pair(code, hit->second));

      msg.erase(hit);
    }
    else if (it != msg.end())
      msg.erase(it);
  }

  return height;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get current phase of flight from amdar message and set flags indicating phase change.
 *
 *        Ascending part of level flight is stored to join to the end of takeoff and descending part
 *        to the start of landing. Last takeoff message is stored to join with landing
 */
// ----------------------------------------------------------------------

Phase get_phase_amdar(Message &msg,
                      Message &lasttakeoffmsg,
                      Phase curphase,
                      Phase &lastmsgphase,
                      bool &phasechange,
                      bool &phaserestart,
                      bool &phasereset,
                      double &lastaltitude)
{
  int code;
  Phase msgphase = get_phase_amdar(msg, true, code);
  double altitude = get_altitude(msg, true);
  bool altitudechange = (fabs(altitude - lastaltitude) > 1);

  phasechange = phaserestart = phasereset = false;

  if (options.debug)
  {
    std::set<NFmiMetTime> dummytimes;
    std::string ident;
    NFmiMetTime t = get_validtime_amdar(dummytimes, msg, ident, true);

    fprintf(stderr,
            "%s %s height=%.0f last=%.0f curphase=%d msgphase=%d\n",
            ident.c_str(),
            Fmi::to_iso_string(t).c_str(),
            altitude,
            lastaltitude,
            (int)curphase,
            (int)msgphase);
  }

  if (curphase == Takeoff)
  {
    // Takeoff can be joined with ascending level flight

    if (msgphase == Landing)
      phasechange = true;

    // Takeoff - LevelFlight - Takeoff ?

    else if ((msgphase == Takeoff) && (lastmsgphase == Flying))
      phaserestart = true;

    // If takeoff or level flight continues lower, start a new phase

    else if (altitudechange && (altitude < lastaltitude))
    {
      if (msgphase == Takeoff)
        phaserestart = true;
      else
        phasechange = true;
    }
  }
  else if (curphase == Flying)
  {
    // Descending level flight can be joined with landing phase

    if (msgphase == Takeoff)
    {
      phasechange = true;

      if (lastmsgphase != Landing)
        phasereset = true;
    }

    // If level flight or landing continues higher, start a new phase

    else if (altitudechange && (altitude > lastaltitude))
    {
      if ((msgphase == Flying) || (lastmsgphase != Landing))
        phasereset = true;
      else
        phasechange = true;
    }
  }
  else if (curphase == Landing)
  {
    // If landing continues higher, start new phase

    if (msgphase != Landing)
      phasechange = true;
    else if (altitudechange && (altitude > lastaltitude))
      phaserestart = true;
  }
  else
    phasechange = true;

  // Remember last takeoff phase message to possibly be joined with landing

  if ((msgphase == Takeoff) || ((curphase == Takeoff) && (!phasechange)))
    lasttakeoffmsg = msg;

  if (phasechange)
  {
    if (curphase == None)
      altitudechange = true;

    // If landing starts higher than last takeoff phase altitude, they cannot be joined

    else if (((msgphase == Landing) && ((!altitudechange) || (altitude > lastaltitude))) ||
             (msgphase == Flying))
      lasttakeoffmsg.clear();

    curphase = msgphase;
  }

  lastmsgphase = msgphase;
  lastaltitude = altitude;

  return curphase;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get number of mapped parameters/observations from message
 */
// ----------------------------------------------------------------------

int get_obscount(const NameMap &namemap, const Message &msg)
{
  int obscount = 0;

  BOOST_FOREACH (const Message::value_type &value, msg)
  {
    auto key = fmt::format("{:0>6}", value.first);

    auto it = namemap.find(key);

    if ((it != namemap.end()) && (value.second.value != kFloatMissing))
      obscount++;
  }

  return obscount;
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove subsequent duplicate amdar messages based on time and
 *        max. number of observations
 */
// ----------------------------------------------------------------------

void remove_duplicate_messages_amdar(const NameMap &namemap, Phase phase, Messages &phasemessages)
{
  auto mit = phasemessages.begin();
  auto mit0 = phasemessages.end();
  std::set<NFmiMetTime> dummytimes;
  std::string ident;
  double lastaltitude = 0.0;
  int obscount0 = -1;

  for (; (mit != phasemessages.end());)
  {
    Message &msg = *mit;

    double altitude = get_altitude(msg, false);
    bool altitudechange = ((mit == phasemessages.begin()) || (fabs(altitude - lastaltitude) > 1));

    if (!altitudechange)
    {
      if (obscount0 < 0)
      {
        mit0 = prev(mit);
        obscount0 = get_obscount(namemap, *mit0);
      }

      int obscount = get_obscount(namemap, msg);

      if (options.debug)
      {
        NFmiMetTime t = get_validtime_amdar(dummytimes, msg, ident, true);

        fprintf(stderr,
                "Duplicate altitude %s %s %d %d %.0f\n",
                ident.c_str(),
                to_iso_string(t.PosixTime()).c_str(),
                obscount,
                obscount0,
                altitude);
      }

      if (obscount > obscount0)
      {
        phasemessages.erase(mit0);

        mit0 = mit;
        obscount0 = obscount;
      }
      else if ((obscount < obscount0) || (phase == Takeoff))
      {
        // Keep earliest message for takeoff, latest otherwise

        mit = phasemessages.erase(mit);
        continue;
      }
      else
      {
        // Keep latest message

        phasemessages.erase(mit0);
        mit0 = mit;
      }
    }
    else
    {
      lastaltitude = altitude;
      obscount0 = -1;
    }

    mit++;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove duplicate amdar messages and amdars with too few observations/times, and limit
 *        amdar duration. Keep track of max number of levels (observations/times) per amdar
 */
// ----------------------------------------------------------------------

void limit_duration_amdar(const NameMap &namemap,
                          Phase phase,
                          Messages &phasemessages,
                          IdentTimeMap &identtimemap,
                          size_t &levelcount)
{
  using namespace boost::posix_time;

  remove_duplicate_messages_amdar(namemap, phase, phasemessages);

  time_duration maxduration(hours(options.maxdurationhours));
  time_duration td;

  Messages::iterator mit = phasemessages.begin();
  Messages::iterator mit0;
  std::set<NFmiMetTime> dummytimes;
  NFmiMetTime t;
  NFmiMetTime t0;
  std::string ident;

  for (; (mit != phasemessages.end()); mit++)
  {
    t = get_validtime_amdar(dummytimes, *mit, ident, true);

    if (mit != phasemessages.begin())
    {
      td = t.PosixTime() - t0.PosixTime();

      if (options.debug)
        fprintf(stderr,
                "  Duration %s %s %s %ld secs\n",
                ident.c_str(),
                to_iso_string(t.PosixTime()).c_str(),
                to_iso_string(t0.PosixTime()).c_str(),
                td.total_seconds());

      if (td > maxduration)
      {
        if (phase == Takeoff)
        {
          phasemessages.erase(mit, phasemessages.end());
          break;
        }

        for (; (mit0 != mit);)
        {
          mit0 = phasemessages.erase(mit0);

          if (mit0 != mit)
          {
            t0 = get_validtime_amdar(dummytimes, *mit0, ident, true);

            td = t.PosixTime() - t0.PosixTime();

            if (options.debug)
              fprintf(stderr,
                      "    Duration %s %s %s %ld secs\n",
                      ident.c_str(),
                      to_iso_string(t.PosixTime()).c_str(),
                      to_iso_string(t0.PosixTime()).c_str(),
                      td.total_seconds());

            if (td <= maxduration)
              break;
          }
          else
            t0 = t;
        }
      }
    }
    else
    {
      if (options.debug)
        fprintf(stderr, "Duration %s %s\n", ident.c_str(), to_iso_string(t.PosixTime()).c_str());

      t0 = t;
      mit0 = mit;
    }
  }

  // Remove amdars having too few observations/times (levels) and keep track of max number of levels

  size_t lvlcnt = 0;

  for (mit = phasemessages.begin();;)
  {
    if (mit != phasemessages.end())
    {
      t = get_validtime_amdar(dummytimes, *mit, ident, true);

      if (options.debug)
        fprintf(stderr, "  Limit %s %s\n", ident.c_str(), to_iso_string(t.PosixTime()).c_str());

      mit++;
      lvlcnt++;
    }
    else
    {
      if ((int)lvlcnt < options.minobservations)
      {
        phasemessages.clear();

        if (options.debug)
          fprintf(stderr, "  Limit erase %s\n", ident.c_str());

        identtimemap.erase(ident);
      }
      else
      {
        if (options.debug)
          fprintf(stderr, "  %s levelcount %lu\n", ident.c_str(), lvlcnt);

        levelcount = std::max(levelcount, lvlcnt);
      }

      break;
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Remap amdar message bufr codes
 */
// ----------------------------------------------------------------------

void remap_message_amdar(Message &msg)
{
  for (auto const &codes : options.messageremap)
  {
    // These have been remapped already

    if ((codes.first == REMAP_IDENT) || (codes.first == REMAP_PHASE) ||
        (codes.first == REMAP_ALTITUDE))
      continue;

    auto mit = msg.end();

    for (const auto code : codes.second)
    {
      mit = msg.find(code);

      if ((mit != msg.end()) && (mit->second.value != kFloatMissing))
        break;
    }

    const int msgcode = ((mit != msg.end()) ? mit->first : 0);
    const int primarycode = codes.second.front();

    for (const auto code : codes.second)
    {
      auto it = msg.find(code);

      if (code == primarycode)
      {
        // Primary/first code, no need to remap ?

        if ((mit == msg.end()) || (code == msgcode))
          continue;

        // Erase old row with primary code if it exists

        if (it != msg.end())
          msg.erase(it);

        // Insert new row with primary code

        msg.insert(std::make_pair(code, mit->second));

        msg.erase(mit);
      }
      else if (it != msg.end())
        msg.erase(it);
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set flight phase/state string for debug output
 */
// ----------------------------------------------------------------------

void debug_state_amdar(const std::string &ident,
                       const std::string &lastorigident,
                       Phase curphase,
                       Phase lastphase,
                       std::string &state)
{
  state = (ident != lastorigident) ? "new " : "";

  if ((ident == lastorigident) && (curphase != Flying) && (lastphase != Flying))
  {
    if (curphase == Flying)
    {
      if (lastphase == Landing)
      {
        state += "landing_fly !";
      }
      else if (lastphase == Takeoff)
      {
        state += "takeoff_fly";
      }
      else
      {
        state += "fly";
      }
    }
    else if (curphase == Takeoff)
    {
      if (lastphase == Flying)
      {
        state += "fly_takeoff !";
      }
      else if (lastphase == Landing)
      {
        state += "landing_takeoff";
      }
      else
        state += "takeoff";
    }
    else
    {
      if (lastphase == Flying)
      {
        state += "fly_landing";
      }
      else if (lastphase == Takeoff)
      {
        state += "takeoff_landing";
      }
      else
        state += "landing";
    }
  }
  else
  {
    if (curphase == Flying)
      state += "fly";
    else if (curphase == Takeoff)
      state += "takeoff";
    else
      state += "landing";
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set phase (takeoff for takeoff and level flight, and landing for
 *        takeoff and landing or level flight and landing) for joined messages
 */
// ----------------------------------------------------------------------

void set_phase_amdar(Phase phase, Messages &phasemessages)
{
  int code;

  if (phase != Takeoff)
    phase = Landing;

  for (auto &msg : phasemessages)
  {
    get_phase_amdar(msg, false, code);

    auto pit = msg.find(code);

    pit->second.value = phase;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Store amdar i.e. messages from single takeoff or landing
 */
// ----------------------------------------------------------------------

void store_messages_amdar(const NameMap &namemap,
                          const std::string &nextident,
                          Phase phase,
                          Messages &phasemessages,
                          IdentTimeMap &identtimemap,
                          TimeIdentMessageMap &timeidentmessages,
                          size_t &levelcount)
{
  // Taking allowed max duration into account, ignore messages from the end of takeoff
  // or the start of landing. Remove duplicate messages. Ignore the phase/amdar if it
  // has too few messages/observations

  limit_duration_amdar(namemap, phase, phasemessages, identtimemap, levelcount);

  if (phasemessages.empty())
    return;

  // Set phase for (possibly) joined messages

  set_phase_amdar(phase, phasemessages);

  // Store idents into a list in time order

  std::set<NFmiMetTime> dummytimes;
  std::string ident;
  NFmiMetTime t = get_validtime_amdar(dummytimes, phasemessages.front(), ident, true);

  identtimemap.insert(std::make_pair(ident, t));

  if (options.debug)
    fprintf(stderr,
            "TimeIdent %s next %s %s add %lu\n",
            ident.c_str(),
            nextident.c_str(),
            to_iso_string(t.PosixTime()).c_str(),
            phasemessages.size());

  // Store messages into a map with time as the (main) key

  auto ti =
      timeidentmessages.insert(std::make_pair(to_iso_string(t.PosixTime()), IdentMessageMap()));
  auto im = ti.first->second.insert(std::make_pair(ident, Messages()));
  im.first->second.insert(im.first->second.begin(), phasemessages.begin(), phasemessages.end());
}

// ----------------------------------------------------------------------
/*!
 * \brief Sort amdar messages to time and ident (aircraft reg. nr or other identification) order.
 *
 *        Parameter mapping is set to namemap. Data times (the first time for each amdar)
 *        for the idents/amdars are set to 'identtimemap' and idents are stored to 'timeidentlist'
 *        in time and ident order.
 *
 *        Also set number of data levels from max number of time instants per amdar to 'levelcount'
 *
 *        Messages with missing/unknown ident, phase of flight or altitude are filtered off.
 */
// ----------------------------------------------------------------------

void organize_messages_amdar(const Messages &origmessages,
                             const NameMap &parammap,
                             NameMap &namemap,
                             Messages &messages,
                             TimeIdentList &timeidentlist,
                             IdentTimeMap &identtimemap,
                             size_t &levelcount)
{
  using TimeMessageMap = std::map<std::string, Message>;
  using MessageMap = std::map<std::string, TimeMessageMap>;
  MessageMap messagemap;
  Messages filteredmessages;
  Message remappedmsg;
  std::set<NFmiMetTime> dummytimes;
  NFmiMetTime t;
  std::string ident;
  size_t msgnbr = 0;

  // Store messages into a map using aircraft/amdar ident as the (main) key.
  // Filter off messages with missing/unknown ident, phase of flight or altitude

  BOOST_FOREACH (const Message &msg, origmessages)
  {
    try
    {
      // Messages must have aircraft reg. nr or other identification

      msgnbr++;

      if (!message_has_ident(msg, ident))
        continue;

      // Messages must have Takeoff, Flying (level flight) or Landing phase of flight

      if (get_phase_amdar(msg) == msg.end())
        continue;

      // Messages must have altitude information

      if (get_altitude(msg) == msg.end())
        continue;

      // Remap bufr codes. There are 3 remappings by default; ident, phase of flight and altitude

      const Message &fmsg = ((options.messageremap.size() > 3) ? remappedmsg : msg);

      if (options.messageremap.size() > 3)
      {
        remappedmsg = msg;
        remap_message_amdar(remappedmsg);
      }

      // Get aircraft/amdar ident and message time

      t = get_validtime_amdar(dummytimes, fmsg, ident, true);

      // Store messages into a list for parameter mapping

      filteredmessages.push_back(fmsg);

      // Store messages into a map for sorting by message time

      auto it = messagemap.find(ident);

      if (it == messagemap.end())
      {
        TimeMessageMap tm;
        tm.insert(std::make_pair(to_iso_string(t.PosixTime()), fmsg));

        messagemap.insert(std::make_pair(ident, tm));
      }
      else
        it->second.insert(std::make_pair(to_iso_string(t.PosixTime()), fmsg));
    }
    catch (std::exception &e)
    {
      std::cerr << "Warning: " << e.what() << " ... skipping message " << msgnbr << std::endl;
    }
  }

  // Build a list of all parameter names

  std::set<std::string> names = collect_names(filteredmessages);

  namemap = map_names(names, parammap);

  // Collect/group data by ident, phase of flight and time

  Phase curphase = None;
  Phase lastphase = None;
  Phase lastmsgphase = None;
  TimeIdentMessageMap timeidentmessages;
  Messages phasemessages;
  Message lasttakeoffmsg;
  std::string lastident;
  std::string lastorigident;
  boost::posix_time::ptime lasttime;
  std::string msgident;
  std::string state;
  double lastaltitude = 0.0;
  bool phasechange;
  bool phaserestart;
  bool phasereset;
  int counter = 0;

  levelcount = 0;

  auto imm = messagemap.begin();
  for (; imm != messagemap.end(); imm++)
  {
    auto imt = imm->second.begin();
    NFmiMetTime t;

    for (; imt != imm->second.end(); imt++)
    {
      auto &msg = imt->second;
      auto iit = get_ident(msg, options.messageremap, msgident);
      const std::string ident = imm->first;
      bool identchange = (ident != lastorigident);

      // Get phase of flight. Join takeoff and flight or landing, and flight and landing messages
      // when applicable.
      //
      // Remap phase of flight and altitude bufr codes if requested

      curphase = get_phase_amdar(msg,
                                 lasttakeoffmsg,
                                 curphase,
                                 lastmsgphase,
                                 phasechange,
                                 phaserestart,
                                 phasereset,
                                 lastaltitude);

      if (phasereset)
      {
        // Ignore collected descending level fligth messages on phase change or Flying phase
        // restart; they can not be joined with landing phase messages

        if (options.debug)
          fprintf(stderr,
                  "%s reset %lu messages curphase=%d lastphase=%d\n",
                  ident.c_str(),
                  phasemessages.size(),
                  (int)curphase,
                  (int)lastphase);

        phasemessages.clear();
      }

      if (options.debug)
        debug_state_amdar(ident, lastorigident, curphase, lastphase, state);

      if (identchange || phasechange || phaserestart)
      {
        char cntrstr[16];
        sprintf(cntrstr, "%06d", counter);
        counter++;

        lastident = ident + "_" + cntrstr + "_" + Fmi::to_string(curphase);
        lastorigident = ident;

        auto *firstmessage = &msg;

        if (!phasemessages.empty())
        {
          // Store the ident/amdar

          store_messages_amdar(namemap,
                               lastident,
                               lastphase,
                               phasemessages,
                               identtimemap,
                               timeidentmessages,
                               levelcount);
          phasemessages.clear();

          if ((!identchange) && (curphase == Landing) && (!lasttakeoffmsg.empty()))
          {
            // Set message ident and join last takeoff message to the start of landing phase

            auto fit = get_ident(lasttakeoffmsg, options.messageremap, msgident);
            fit->second.svalue = lastident;

            if (options.debug)
              fprintf(stderr, "Set ident takeoff %s to %s\n", msgident.c_str(), lastident.c_str());

            phasemessages.push_back(lasttakeoffmsg);

            firstmessage = &lasttakeoffmsg;
          }
        }

        if (options.debug)
        {
          t = get_validtime_amdar(dummytimes, *firstmessage, msgident, true);
          fprintf(stderr,
                  "Set ident %s %s %d %s %5.0f %s\n",
                  iit->second.svalue.c_str(),
                  lastident.c_str(),
                  (int)curphase,
                  to_iso_string(t.PosixTime()).c_str(),
                  lastaltitude,
                  state.c_str());
        }

        if (curphase != Takeoff)
          lasttakeoffmsg.clear();
      }
      else if (options.debug)
      {
        t = get_validtime_amdar(dummytimes, msg, msgident, true);
        fprintf(stderr,
                "  set ident %s %s %d %s %5.0f %s\n",
                iit->second.svalue.c_str(),
                lastident.c_str(),
                (int)curphase,
                to_iso_string(t.PosixTime()).c_str(),
                lastaltitude,
                state.c_str());
      }

      iit->second.svalue = lastident;

      phasemessages.push_back(imt->second);

      lastphase = curphase;
    }
  }

  // Handle last ident/phase

  if ((!phasemessages.empty()) && ((curphase != Flying) || (lastmsgphase == Landing)))
    store_messages_amdar(
        namemap, "null", lastphase, phasemessages, identtimemap, timeidentmessages, levelcount);

  // Store the amdar idents in time and messages in time and ident order into lists

  BOOST_FOREACH (auto const &timeidents, timeidentmessages)
  {
    if (options.debug)
      fprintf(
          stderr, "TimeIdent %s %lu idents\n", timeidents.first.c_str(), timeidents.second.size());

    BOOST_FOREACH (auto const &identmessages, timeidents.second)
    {
      if (options.debug)
        fprintf(stderr,
                "  %s %lu messages\n",
                identmessages.first.c_str(),
                identmessages.second.size());

      timeidentlist.push_back(identmessages.first);
      messages.insert(messages.end(), identmessages.second.begin(), identmessages.second.end());
    }
  }

  // Rebuild list of all parameter names with messages eventually collected

  names = collect_names(messages);

  namemap = map_names(names, parammap);
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove subsequent duplicate sounding messages based on time and
 *        max. number of observations
 */
// ----------------------------------------------------------------------

void remove_duplicate_messages_sounding(const NameMap &namemap,
                                        const std::string &ident,
                                        Messages &soundingmessages)
{
  auto mit = soundingmessages.begin();
  auto mit0 = soundingmessages.end();
  double lastaltitude = 0.0;
  int obscount0 = -1;

  for (; (mit != soundingmessages.end());)
  {
    Message &msg = *mit;

    double altitude = get_altitude(msg, false);
    bool altitudechange =
        ((mit == soundingmessages.begin()) || (fabs(altitude - lastaltitude) > 1));

    if (!altitudechange)
    {
      if (obscount0 < 0)
      {
        mit0 = prev(mit);
        obscount0 = get_obscount(namemap, *mit0);
      }

      int obscount = get_obscount(namemap, msg);

      if (options.debug)
        fprintf(stderr,
                "Duplicate altitude %s %s %d %d %.0f\n",
                ident.c_str(),
                to_iso_string(get_validtime(msg).PosixTime()).c_str(),
                obscount,
                obscount0,
                altitude);

      if (obscount > obscount0)
      {
        soundingmessages.erase(mit0);

        mit0 = mit;
        obscount0 = obscount;
      }
      else
      {
        // Keep earliest message

        mit = soundingmessages.erase(mit);
        continue;
      }
    }
    else
    {
      lastaltitude = altitude;
      obscount0 = -1;
    }

    mit++;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove duplicate sounding messages and soundungs with too few observations/times, and
 *        limit sounding duration. Keep track of max number of levels (observations/times) per
 *        sounding
 */
// ----------------------------------------------------------------------

void limit_duration_sounding(const NameMap &namemap,
                             const std::string &ident,
                             Messages &soundingmessages,
                             IdentTimeMap &identtimemap,
                             size_t &levelcount)
{
  using namespace boost::posix_time;

  remove_duplicate_messages_sounding(namemap, ident, soundingmessages);

  // Test data seems to have the same timestamp for all messages for given sounding,
  // but checking the duration anyway

  time_duration maxduration(hours(options.maxdurationhours));
  time_duration td;

  auto mit = soundingmessages.begin();
  NFmiMetTime t;
  NFmiMetTime t0;

  for (; (mit != soundingmessages.end()); mit++)
  {
    t = get_validtime(*mit);

    if (mit != soundingmessages.begin())
    {
      td = t.PosixTime() - t0.PosixTime();

      if (options.debug)
        fprintf(stderr,
                "  Duration %s %s %s %ld secs\n",
                ident.c_str(),
                to_iso_string(t.PosixTime()).c_str(),
                to_iso_string(t0.PosixTime()).c_str(),
                td.total_seconds());

      if (td > maxduration)
      {
        soundingmessages.erase(mit, soundingmessages.end());
        break;
      }
    }
    else
    {
      if (options.debug)
        fprintf(stderr, "Duration %s %s\n", ident.c_str(), to_iso_string(t.PosixTime()).c_str());

      t0 = t;
    }
  }

  // Remove soundings having too few observations/times (levels) and keep track of max number of
  // levels

  size_t lvlcnt = 0;

  for (mit = soundingmessages.begin();;)
  {
    if (mit != soundingmessages.end())
    {
      t = get_validtime(*mit);

      if (options.debug)
        fprintf(stderr, "  Limit %s %s\n", ident.c_str(), to_iso_string(t.PosixTime()).c_str());

      mit++;
      lvlcnt++;
    }
    else
    {
      if ((int)lvlcnt < options.minobservations)
      {
        soundingmessages.clear();

        if (options.debug)
          fprintf(stderr, "  Limit erase %s\n", ident.c_str());

        identtimemap.erase(ident);
      }
      else
      {
        if (options.debug)
          fprintf(stderr, "  %s levelcount %lu\n", ident.c_str(), lvlcnt);

        levelcount = std::max(levelcount, lvlcnt);
      }

      break;
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Store sounding i.e. station's successive messages with ascending altitude
 */
// ----------------------------------------------------------------------

void store_messages_sounding(const NameMap &namemap,
                             long station,
                             StationTimeSet &stationtimeset,
                             const std::string &ident,
                             const std::string &nextident,
                             Messages &soundingmessages,
                             IdentTimeMap &identtimemap,
                             TimeIdentMessageMap &timeidentmessages,
                             size_t &levelcount)
{
  // Taking allowed max duration into account, ignore messages from the end of sounding.
  // Remove duplicate messages. Ignore the sounding if it has too few messages/observations

  limit_duration_sounding(namemap, ident, soundingmessages, identtimemap, levelcount);

  if (soundingmessages.empty())
    return;

  // Store idents into a list in time order. Round the time to nearest hour

  NFmiMetTime t = get_validtime(soundingmessages.front());
  t.NearestMetTime(60);

  auto its = stationtimeset.find(station);

  if (its == stationtimeset.end())
    its = stationtimeset.insert(std::make_pair(station, std::set<std::string>())).first;

  auto result = its->second.insert(to_iso_string(t.PosixTime()));

  if (!result.second)
  {
    // Select the most comprehensive sounding from duplicates.
    //
    // First search the currently selected/stored duplicate; idents contain the station
    // at the beginning (idents are of form <station>_<counter>)

    auto tit = timeidentmessages.find(to_iso_string(t.PosixTime()));
    auto sident = Fmi::to_string(station) + "_";

    auto tii = tit->second.begin();
    for (; ((tii != tit->second.end()) && (tii->first.find(sident) != 0)); tii++)
      ;

    if (tii == tit->second.end())
      throw std::runtime_error("Internal error, duplicate sounding for ident " + ident +
                               " not found");

    // Sounding with most levels

    if (tii->second.size() != soundingmessages.size())
    {
      if (options.debug)
        fprintf(stderr,
                "%s duplicate %s %s levels %lu/%lu\n",
                (tii->second.size() > soundingmessages.size()) ? "Keep" : "Select",
                tii->first.c_str(),
                to_iso_string(t.PosixTime()).c_str(),
                std::max(tii->second.size(), soundingmessages.size()),
                std::min(tii->second.size(), soundingmessages.size()));

      if (tii->second.size() > soundingmessages.size())
        return;
    }
    else
    {
      // Sounding with most observations

      int obscount1 = 0;
      int obscount2 = 0;
      int obscount;

      for (auto const &msg1 : tii->second)
        if ((obscount = get_obscount(namemap, msg1)) > obscount1)
          obscount1 = obscount;

      for (auto const &msg2 : soundingmessages)
        if ((obscount = get_obscount(namemap, msg2)) > obscount2)
          obscount2 = obscount;

      if (obscount1 != obscount2)
      {
        if (options.debug)
          fprintf(stderr,
                  "%s duplicate %s %s obscount %d/%d\n",
                  (obscount1 > obscount2) ? "Keep" : "Select",
                  tii->first.c_str(),
                  to_iso_string(t.PosixTime()).c_str(),
                  std::max(obscount1, obscount2),
                  std::min(obscount1, obscount2));

        if (obscount1 > obscount2)
          return;
      }
      else
      {
        // Sounding with widest vertical range

        auto range1 =
            get_altitude(tii->second.back(), false) - get_altitude(tii->second.front(), false);
        auto range2 = get_altitude(soundingmessages.back(), false) -
                      get_altitude(soundingmessages.front(), false);

        if (options.debug)
          fprintf(stderr,
                  "%s duplicate %s %s range %.0f/%.0f\n",
                  (range1 >= range2) ? "Keep" : "Select",
                  tii->first.c_str(),
                  to_iso_string(t.PosixTime()).c_str(),
                  std::max(range1, range2),
                  std::min(range1, range2));

        if (range1 >= range2)
          return;
      }
    }

    tit->second.erase(tii->first);
  }

  identtimemap.insert(std::make_pair(ident, t));

  if (options.debug)
    fprintf(stderr,
            "TimeIdent %s next %s %s add %lu\n",
            ident.c_str(),
            nextident.c_str(),
            to_iso_string(t.PosixTime()).c_str(),
            soundingmessages.size());

  // Store messages into a map with time as the (main) key

  auto ti =
      timeidentmessages.insert(std::make_pair(to_iso_string(t.PosixTime()), IdentMessageMap()));
  auto im = ti.first->second.insert(std::make_pair(ident, Messages()));
  im.first->second.insert(
      im.first->second.begin(), soundingmessages.begin(), soundingmessages.end());
}

// ----------------------------------------------------------------------
/*!
 * \brief Sort sounding messages to station and time order
 *
 *        Parameter mapping is set to namemap. Data times (the first time for each sounding)
 *        for the soundings are set to 'stationtimemap' and stations are stored to 'timestationlist'
 *        in time and station order.
 *
 *        Also set number of data levels from max number of time instants per sounding to
 *        'levelcount'
 */
// ----------------------------------------------------------------------

void organize_messages_sounding(const Messages &origmessages,
                                const NameMap &parammap,
                                NameMap &namemap,
                                Messages &messages,
                                TimeIdentList &timeidentlist,
                                IdentTimeMap &identtimemap,
                                size_t &levelcount)
{
  using TimeMessageMap = std::multimap<std::string, Message>;
  using MessageMap = std::map<long, TimeMessageMap>;
  MessageMap messagemap;
  Messages filteredmessages;
  Message remappedmsg;
  NFmiMetTime t;
  size_t msgnbr = 0;

  // Store messages into a map using station and time as the keys
  // Filter off messages with missing/unknown station or altitude

  BOOST_FOREACH (const Message &msg, origmessages)
  {
    try
    {
      // Messages must have wmo station number

      msgnbr++;

      auto station = get_station(msg).GetIdent();

      if (station < 0)
        continue;

      // Messages must have altitude information

      if (get_altitude(msg) == msg.end())
        continue;

      // Remap bufr codes. There are 3 remappings by default; ident, phase of flight and altitude

      const Message &fmsg = ((options.messageremap.size() > 3) ? remappedmsg : msg);

      if (options.messageremap.size() > 3)
      {
        remappedmsg = msg;
        remap_message_amdar(remappedmsg);
      }

      // Store messages into a list for parameter mapping

      filteredmessages.push_back(fmsg);

      // Get message time

      t = get_validtime(fmsg);

      // Store messages into a map for sorting by message time

      auto it = messagemap.find(station);

      if (it == messagemap.end())
      {
        TimeMessageMap tm;
        tm.insert(std::make_pair(to_iso_string(t.PosixTime()), msg));

        messagemap.insert(std::make_pair(station, tm));
      }
      else
        it->second.insert(std::make_pair(to_iso_string(t.PosixTime()), msg));
    }
    catch (std::exception &e)
    {
      std::cerr << "Warning: " << e.what() << " ... skipping message " << msgnbr << std::endl;
    }
  }

  // Build a list of all parameter names

  std::set<std::string> names = collect_names(filteredmessages);

  namemap = map_names(names, parammap);

  // Collect/group data by station and time

  StationTimeSet stationtimeset;
  TimeIdentMessageMap timeidentmessages;
  Messages soundingmessages;
  std::string lastident;
  double lastaltitude = 0.0;
  long laststation = -1;
  bool soundingrestart;
  int counter = 0;

  record identrec;       // Sounding ident is added to messages to identify station's
  identrec.name = "ID";  // soundings (multiple soundings for station or should given
                         // sounding be split since altitude is not monotonically ascending)

  levelcount = 0;

  auto imm = messagemap.begin();
  for (; imm != messagemap.end(); imm++)
  {
    auto imt = imm->second.begin();

    for (; imt != imm->second.end(); imt++)
    {
      auto &msg = imt->second;
      long station = get_station(msg).GetIdent();
      bool stationchange = (station != laststation);
      double altitude = get_altitude(msg, true);

      if (stationchange)
        soundingrestart = false;
      else
      {
        bool altitudechange = (fabs(altitude - lastaltitude) > 1);

        soundingrestart = (altitudechange && (altitude < lastaltitude));
        lastaltitude = altitude;
      }

      if (options.debug)
        t = get_validtime(msg);

      if (stationchange || soundingrestart)
      {
        char cntrstr[16];
        sprintf(cntrstr, "%06d", counter);
        counter++;

        std::string ident(Fmi::to_string(station) + "_" + cntrstr);

        if (!soundingmessages.empty())
        {
          // Store the ident/amdar

          store_messages_sounding(namemap,
                                  laststation,
                                  stationtimeset,
                                  lastident,
                                  ident,
                                  soundingmessages,
                                  identtimemap,
                                  timeidentmessages,
                                  levelcount);
          soundingmessages.clear();
        }

        laststation = station;
        lastident = ident;

        if (options.debug)
          fprintf(stderr,
                  "Set ident %s %s %5.0f\n",
                  ident.c_str(),
                  to_iso_string(t.PosixTime()).c_str(),
                  lastaltitude);
      }
      else if (options.debug)
        fprintf(stderr,
                "  set ident %s %s %5.0f\n",
                lastident.c_str(),
                to_iso_string(t.PosixTime()).c_str(),
                lastaltitude);

      identrec.svalue = lastident;

      msg.insert(std::make_pair(REMAP_PRIMARY_IDENT, identrec));

      soundingmessages.push_back(imt->second);
    }
  }

  // Handle last station/sounding

  if (!soundingmessages.empty())
    store_messages_sounding(namemap,
                            laststation,
                            stationtimeset,
                            lastident,
                            "null",
                            soundingmessages,
                            identtimemap,
                            timeidentmessages,
                            levelcount);

  // Store the sounding idents in time and messages in time and ident order into lists

  BOOST_FOREACH (auto const &timeidents, timeidentmessages)
  {
    if (options.debug)
      fprintf(
          stderr, "TimeIdent %s %lu idents\n", timeidents.first.c_str(), timeidents.second.size());

    BOOST_FOREACH (auto const &identmessages, timeidents.second)
    {
      if (options.debug)
        fprintf(stderr,
                "  %s %lu messages\n",
                identmessages.first.c_str(),
                identmessages.second.size());

      timeidentlist.push_back(identmessages.first);
      messages.insert(messages.end(), identmessages.second.begin(), identmessages.second.end());
    }
  }

  // Rebuild list of all parameter names with messages eventually collected

  names = collect_names(messages);

  namemap = map_names(names, parammap);
}

// ----------------------------------------------------------------------
/*!
 * \brief Decode low/middle/high cloud types
 */
// ----------------------------------------------------------------------

void decode_cloudtypes(const Messages &origmessages, Messages &messages)
{
  /*
    B 08 002	Vertical significance (surface observation)
    If CL are observed then B 08 002 = 07 (low clouds),
    If CL are not observed and CM are observed, then B 08 002 = 08 (medium clouds),
    If only CH are observed, B 08 002 = 0
    If N = 9, then B 08 002 = 05
    If N = 0, then B 08 002 = 62
    If N = /, then B 08 002 = missing

    B 20 012	Cloud type (low clouds)        	CL
    B 20 012 = CL + 30
    If N = 0, then B 20 012 = 30
    If N = 9 or /, then B 20 012 = 62

    B 20 012	Cloud type (medium clouds)	CM
    B 20 012 = CM + 20,
    If N = 0, then B 20 012 = 20
    If N = 9 or / or CM = /, then B 20 012 = 61

    B 20 012	Cloud type (high clouds)	CH
    0 20 012 = CH + 10,
    If N = 0, then B 20 012 = 10,
    If N = 9 or / or CH = /, then B 20 012 = 60

    12.2.7 Group 8NhCLCMCH 12.2.7.1 This group shall be omitted in the following cases: (a) When
    there are no clouds (N = 0); (b) When the sky is obscured by fog and/or other meteorological
    phenomena (N = 9); (c) When the cloud cover is indiscernible for reasons other than (b) above,
    or observation is not made (N = /). Note: All cloud observations at sea including no cloud
    observation shall be reported in the SHIP message. 12.2.7.2 Certain regulations concerning the
    coding of N shall also apply to the coding of Nh. 12.2.7.2.1 (a) If there are CL clouds then the
    total amount on all CL clouds, as actually seen by the observer during the observation, shall be
    reported for Nh; (b) If there are no CL clouds but there are CM clouds, then the total amount of
    the CM clouds shall be reported for Nh; (c) If there are no CL clouds and there are no CM level
    clouds, but there are CH clouds, then Nh shall be coded as 0. 12.2.7.2.2 If the variety of the
    cloud reported for Nh is perlucidus (stratocumulus perlucidus for a CL cloud or altocumulus
    perlucidus for a CM cloud) then Nh shall be coded as 7 or less. Note: See Regulation 12.2.2.2.2.
    12.2.7.2.3 When the clouds reported for Nh are observed through fog or an analogous
    phenomenon their amount shall be reported as if these phenomena were not present.
    12.2.7.2.4 If the clouds reported for Nh include contrails, then Nh shall include the amount of
    persistent contrails. Rapidly dissipating contrails shall not be included in the value for Nh.
    Note: See Regulation 12.5 concerning the use of Section 4.
  */

  /* Test data contais following 8002 and 12002 combinations / value ranges:

       CL, low clouds:

         Stored as LowCloudType values

           7	3-9	1-9 stored (if 0, not stored)

         Other values not stored, unknown 20012 coding, N/A or missing observation

           7	10	CH ?: If N = 0, then B 20 012 = 10 ?
           7	11-19	shift to 1-9 ?
           7	32,37	?
           7	60	CH ?: If N = 9 or / or CH = /, then B 20 012 = 60 ?
                        CL ?: If N = 9 or /, then B 20 012 = 62 ?

       CM, middle clouds:

         Stored as MiddleCloudType values

           8	0-9	1-9 stored (if 0, not stored)

         Other values not stored, unknown 20012 coding, N/A or missing observation

           8	10	CH ?: If N = 0, then B 20 012 = 10 ?
           8	11-19	shift to 1-9 ? wrong 8002 ?
           8	60	CH ?: If N = 9 or / or CH = /, then B 20 012 = 60 ?
                        CM ?: If N = 9 or /, then B 20 012 = 61 ?

       CH, high clouds:

         Stored as HighCloudType values

           0	0-9	1-9 stored (if 0, not stored)

         Other values not stored, unknown 20012 coding (or N/A or missing observation)

           0	11-19	shift to 1-9 ? wrong 8002 ?

       Not stored, unknown 8002 coding, N/A or missing observation

         If N = 9, then B 08 002 = 05

           5	59,60

         Unknown 8002 coding

           1,2,3,4,9,11

         If N = 0, then B 08 002 = 62

           62	6,7,10
  */

  // Decode low/middle/high cloud types and store the types using hardcoded codes for mapping
  // the data to LowCloudType, MiddleCloudType and HighCloudType qd parameters.
  //
  // Only types 1-9 are accepted, 0 (no clouds, and all others) get MissingValue

  const int QDMappingCodeLowCloudType = 990411;
  const int QDMappingCodeMiddleCloudType = 990412;
  const int QDMappingCodeHighCloudType = 990413;

  Message msg;

  for (auto const &message : origmessages)
  {
    msg = message;

    auto itl = msg.find(8002);

    if ((itl != msg.end()) && (itl->second.value != kFloatMissing))
    {
      auto itt = msg.find(20012);

      if ((itt != msg.end()) && (itt->second.value != kFloatMissing))
      {
        int code = 0;

        if (itl->second.value == 7)
        {
          // Low cloud

          if ((itt->second.value >= 1) && (itt->second.value <= 9))
            code = QDMappingCodeLowCloudType;
        }
        else if (itl->second.value == 8)
        {
          // Middle cloud

          if ((itt->second.value >= 1) && (itt->second.value <= 9))
            code = QDMappingCodeMiddleCloudType;
        }
        else if (itl->second.value == 0)
        {
          // High cloud

          if ((itt->second.value >= 1) && (itt->second.value <= 9))
            code = QDMappingCodeHighCloudType;
        }

        if (code != 0)
          msg.insert(std::make_pair(code, itt->second));
      }
    }

    messages.push_back(msg);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief If requested with -f, set PressureChange sign to match PressureTendency
 *        (positive when increasing tendency and vice versa)
 *
 *        Allow only values 0-8 for PressureTendency, set it to missing value otherwise
 */
// ----------------------------------------------------------------------

void set_pressurechange_sign_from_pressuretendency(Messages &messages)
{
  /*
    0 Increasing, then decreasing; atmospheric pressure the same or higher than three hours ago
    1 Increasing, then steady; or increasing, then increasing more slowly
    2 Increasing (steadily or unsteadily)
    3 Decreasing or steady, then increasing; or increasing, then increasing more rapidly
    4 Steady; atmospheric pressure the same as three hours ago
    5 Decreasing, then increasing; atmospheric pressure the same or lower than three hours ago
    6 Decreasing, then steady; or decreasing, then decreasing more slowly
    7 Decreasing (steadily or unsteadily)
    8 Steady or increasing, then decreasing; or decreasing, then decreasing more rapidly

    1-3 Atmospheric pressure now higher than three hours ago
    6-8 Atmospheric pressure now lower than three hours ago

    Data (e.g. Latvia) seems to have negative PressureChange values even though PressureTendency
    is increasing and vice versa. PressureTendency 0 is taken as increasing and 5 decreasing
  */

  for (auto &msg : messages)
  {
    // PressureChange

    auto itc = msg.find(10061);

    if ((!options.forcepressurechangesign) ||
        ((itc != msg.end()) && (itc->second.value != kFloatMissing)))
    {
      // PressureTendency

      auto itt = msg.find(10063);

      if ((itt != msg.end()) && (itt->second.value != kFloatMissing))
      {
        int tendency = (int)(itt->second.value + 0.1);

        if ((tendency < 0) || (tendency > 8))
          itt->second.value = kFloatMissing;
        else if (options.forcepressurechangesign)
        {
          if ((tendency >= 0) && (tendency <= 3))
          {
            // Increasing; positive

            itc->second.value = fabs(itc->second.value);
          }
          else if (tendency >= 5)
          {
            // Decreasing; negative

            itc->second.value = 0 - fabs(itc->second.value);
          }
        }
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set missing totalcloudcover octas from percentage when available
 */
// ----------------------------------------------------------------------

void set_totalcloud_octas_from_percentage(Messages &messages)
{
  for (auto &msg : messages)
  {
    // Totalcloudcover octas

    auto ito = msg.find(20011);

    if ((ito == msg.end()) || (ito->second.value == kFloatMissing))
    {
      // Totalcloudcover percentage

      auto itp = msg.find(20010);

      if ((itp != msg.end()) && (itp->second.value != kFloatMissing))
      {
        float octas;

        if (itp->second.value < 0)
          octas = kFloatMissing;
        else if (itp->second.value > 100)
          octas = 9;
        else
          octas = floor((itp->second.value / 12.5) + 0.00001);

        if (ito != msg.end())
          ito->second.value = octas;
        else
        {
          auto r = itp->second;
          r.value = octas;
          r.svalue.clear();
          r.name = "CLOUD AMOUNT";
          r.units = "CODE TABLE";

          msg.insert(std::make_pair(20011, r));
        }
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program without exception handling
 */
// ----------------------------------------------------------------------

int run(int argc, char *argv[])
{
  if (!parse_options(argc, argv, options))
    return 0;

#ifdef _MSC_VER
  if (options.debug)
    bufr_set_debug(1);
#endif

  // Read the parameter conversion table

  NameMap parammap = read_bufr_config();

  // Read the station names

  NFmiAviationStationInfoSystem stations = read_station_csv();

  // Expand input options to a list of files if necessary

  std::list<std::string> infiles = expand_input_files();

  // Do the bufr operations

  bufr_begin_api();
  std::pair<BufrDataCategory, Messages> tmp = read_messages(infiles);
  bufr_end_api();

  BufrDataCategory category = tmp.first;
  validate_category(category);

  options.requireident &= ((category == kBufrUpperAirLevel) || (category == kBufrSounding));

  // If requested sort messages to time and ident (aircraft reg. nr or other id) and
  // filter off messages with no (or unknown) phase or altitude information

  NameMap namemap;

  Messages preparedmessages;
  const Messages &messages =
      (options.requireident || (category == kBufrLandSurface)) ? preparedmessages : tmp.second;
  TimeIdentList timeidentlist;
  IdentTimeMap identtimemap;
  size_t levelcount = 0;

  if (options.requireident)
    if (category == kBufrUpperAirLevel)
      organize_messages_amdar(
          tmp.second, parammap, namemap, preparedmessages, timeidentlist, identtimemap, levelcount);
    else
      organize_messages_sounding(
          tmp.second, parammap, namemap, preparedmessages, timeidentlist, identtimemap, levelcount);
  else
  {
    if (category == kBufrLandSurface)
    {
      // Decode low/middle/high cloud types

      decode_cloudtypes(tmp.second, preparedmessages);

      // Set PressureChange values's sign to match PressureTendency value and/or
      // filter off unknown PressureTendency values

      set_pressurechange_sign_from_pressuretendency(preparedmessages);

      // If requested with -t, set missing totalcloudcover octas from percentage when available

      if (options.totalcloudoctas)
        set_totalcloud_octas_from_percentage(preparedmessages);
    }

    // Build a list of all parameter names

    std::set<std::string> names = collect_names(messages);

    namemap = map_names(names, parammap);
  }

  // Guess the producer if so requested

  if (options.autoproducer)
    guess_producer(category);

  // This should be before further processing since we may need
  // to get debugging info before anything goes wrong

  if (options.debug)
  {
    int i = 0;
    BOOST_FOREACH (const Message &msg, messages)
    {
      std::cout << std::endl << "Message " << ++i << std::endl << std::endl;

      BOOST_FOREACH (const Message::value_type &value, msg)
        std::cout << value.first << "," << value.second.name << "," << value.second.units << ","
                  << value.second.value << "," << value.second.svalue << std::endl;
    }
  }

  // Build the querydata descriptors from the file names etc

  NFmiParamDescriptor pdesc = create_pdesc(namemap, category);
  NFmiVPlaceDescriptor vdesc = create_vdesc(messages, category, levelcount);
  NFmiTimeDescriptor tdesc = create_tdesc(messages, category, timeidentlist, identtimemap);
  NFmiHPlaceDescriptor hdesc = create_hdesc(messages, stations, category);

  // Initialize the data to missing values

  NFmiFastQueryInfo qi(pdesc, tdesc, hdesc, vdesc);
  boost::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(qi));
  if (data.get() == nullptr)
    throw std::runtime_error("Could not allocate memory for result data");

  NFmiFastQueryInfo info(data.get());

  info.SetProducer(NFmiProducer(options.producernumber, options.producername));

  // Add each file to the data

  copy_records(info, messages, namemap, category, identtimemap);

  // Output

  if (options.outfile == "-")
    std::cout << *data;
  else
  {
    std::ofstream out(options.outfile.c_str(),
                      std::ios::out | std::ios::binary);  // VC++ vaatii että tiedosto avataan
    // binäärisenä (Linuxissa se on default
    // optio)
    out << *data;
  }

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program
 */
// ----------------------------------------------------------------------

int main(int argc, char *argv[])
try
{
  return run(argc, argv);
}
catch (std::exception &e)
{
  std::cerr << "Fatal Error: " << e.what() << std::endl;
  return 1;
}
catch (...)
{
  std::cerr << "Fatal Error: Caught an unknown exception" << std::endl;
  return 1;
}
