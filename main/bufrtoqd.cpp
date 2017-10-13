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

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

extern "C" {
#include <bufr_api.h>
#include <bufr_local.h>
#include <bufr_value.h>
}

namespace fs = boost::filesystem;
struct ParNameInfo
{
  ParNameInfo() : bufrName(), shortName(), parId(kFmiBadParameter) {}
  std::string bufrName;
  std::string shortName;
  FmiParameterName parId;
};

typedef std::map<std::string, ParNameInfo> NameMap;
// typedef std::map<std::string, FmiParameterName> NameMap;

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
  if (name == "land surface") return kBufrLandSurface;
  if (name == "sea surface") return kBufrSeaSurface;
  if (name == "sounding") return kBufrSounding;
  if (name == "satellite sounding") return kBufrSatSounding;
  if (name == "upper air level") return kBufrUpperAirLevel;
  if (name == "upper air level with satellite") return kBufrSatUpperAirLevel;
  if (name == "radar") return kBufrRadar;
  if (name == "synoptic") return kBufrSynoptic;
  if (name == "physical") return kBufrPhysical;
  if (name == "dispersal") return kBufrDispersal;
  if (name == "radiological") return kBufrRadiological;
  if (name == "tables") return kBufrTables;
  if (name == "satellite surface") return kBufrSatSurface;
  if (name == "radiances") return kBufrRadiances;
  if (name == "oceanographic") return kBufrOceanographic;
  if (name == "image") return kBufrImage;

  // Aliases

  if (name == "land") return kBufrLandSurface;
  if (name == "sea") return kBufrSeaSurface;

  throw std::runtime_error("Unknown BUFR data category '" + name + "'");
}

// ----------------------------------------------------------------------
/*!
 * \brief Container for command line options
 */
// ----------------------------------------------------------------------

struct Options
{
  Options();

  bool verbose;  // -v --verbose
  bool debug;    //    --debug
  // Code 8042 = extended vertical sounding significance. Insignificant levels
  // will also be output if this option is used. This may significantly increase
  // the size of the sounding (from ~100 to ~5000 levels)
  bool insignificant;        //    --insignificant
  bool subsets;              //    --subsets
  std::string category;      // -C --category
  std::string conffile;      // -c --config
  std::string stationsfile;  // -s --stations
  std::string infile;        // -i --infile
  std::string outfile;       // -o --outfile
  std::string localtableB;   // -B --localtableB
  std::string localtableD;   // -D --localtableD
  std::string producername;  // --producername
  long producernumber;       // --producernumber
  bool autoproducer;         // -a --autoproducer
  int messagenumber;         // -m --message
};

Options options;

// ----------------------------------------------------------------------
/*!
 * \brief Default options
 */
// ----------------------------------------------------------------------

Options::Options()
    : verbose(false),
      debug(false),
      insignificant(false),
      subsets(false),
      category()
#ifdef UNIX
      ,
      conffile("/usr/share/smartmet/formats/bufr.conf"),
      stationsfile("/usr/share/smartmet/stations.csv")
#else
      ,
      conffile(""),
      stationsfile("")
#endif
      ,
      infile("."),
      outfile("-"),
      localtableB(""),
      localtableD(""),
      producername("UNKNOWN"),
      producernumber(0),
      autoproducer(false),
      messagenumber(0)
{
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

  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "print out help message")("version,V", "display version number")(
      "verbose,v", po::bool_switch(&options.verbose), "set verbose mode on")(
      "debug", po::bool_switch(&options.debug), "set debug mode on")(
      "subsets", po::bool_switch(&options.subsets), "decode all subsets, not just first ones")(
      "insignificant",
      po::bool_switch(&options.insignificant),
      "extract also insignificant sounding levels")(
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
      "autoproducer,a", po::bool_switch(&options.autoproducer), "guess producer automatically");

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
           "Conversion of all categories is not supported though.\n";
    return false;
  }

  if (opt.count("infile") == 0)
    throw std::runtime_error("Expecting input BUFR file as parameter 1");

  if (opt.count("outfile") == 0) throw std::runtime_error("Expecting output file as parameter 2");

  if (!fs::exists(options.infile))
    throw std::runtime_error("Input BUFR '" + options.infile + "' does not exist");

  // Validate the category option
  if (!options.category.empty()) data_category(options.category);

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
    throw std::runtime_error("Data category " + boost::lexical_cast<std::string>(category) +
                             " unknown");

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

  if (options.verbose) std::cout << "Data category: " << name << std::endl;
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
    if (fs::is_regular(path)) files.push_back(path.string());
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

  record() : name(), units(), value(std::numeric_limits<double>::quiet_NaN()), svalue() {}
};

typedef std::map<int, record> Message;
typedef std::list<Message> Messages;

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
      if (str && !bufr_is_missing_string(str, len)) rec.svalue = str;
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
  Message::const_iterator yy = msg.find(4001);
  Message::const_iterator mm = msg.find(4002);
  Message::const_iterator dd = msg.find(4003);
  Message::const_iterator hh = msg.find(4004);
  Message::const_iterator mi = msg.find(4005);

  if (yy == msg.end() || mm == msg.end() || dd == msg.end() || hh == msg.end() || mi == msg.end())
    return false;

  if (yy->second.value < 1900 || yy->second.value > 2200) return false;
  if (mm->second.value < 1 || mm->second.value > 12) return false;
  if (dd->second.value < 1 || dd->second.value > 31) return false;
  if (hh->second.value < 0 || hh->second.value > 23) return false;
  if (mi->second.value < 0 || mi->second.value > 59) return false;

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Accept only significant sounding levels if option -S is used
 */
// ----------------------------------------------------------------------

bool message_is_significant(const Message &msg)
{
  // Keep all levels if --insignificant was used
  if (options.insignificant) return true;

  // If there is no vertical significance value in the message, keep it
  Message::const_iterator sig = msg.find(8042);  // extended vertical sounding sig.
  if (sig == msg.end()) return true;

  // Significant levels are marked with nonzero values
  return (sig->second.value > 0);
}

// ----------------------------------------------------------------------
/*!
 * \brief Append records from a dataset to a list
 */
// ----------------------------------------------------------------------

void append_message(Messages &messages, BUFR_Dataset *dts, BUFR_Tables *tables)
{
  int nsubsets = bufr_count_datasubset(dts);

  bool replicating = false;    // set to true if FLAG_CLASS31 is encountered
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

      if (bufr->flags & FLAG_SKIPPED) continue;

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
          if (message_is_significant(message)) messages.push_back(message);
        }

        message = replicated_message;
        message.insert(Message::value_type(desc, rec));

        if (--replication_count <= 0)
        {
          replicating = false;
          replicating_desc = -1;
        }
      }
      else
      {
        message.insert(Message::value_type(desc, rec));
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
                     "rb");  // VC++ vaatii ett‰ avataan bin‰‰risen‰ (Linuxissa se on default)
  if (bufr == NULL)
  {
    throw std::runtime_error("Could not open BUFR file '" + filename + "' reading");
  }

  if (options.debug) std::cout << "Processing file '" << filename << "'" << std::endl;

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
      if (options.debug) std::cout << "Skipping message number " << count << std::endl;
      continue;
    }

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
      // dts = NULL;  // is already null
      std::cerr << "Warning: No BUFR table version " << msg->s1.master_table_version << " available"
                << std::endl;
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
      throw std::runtime_error("Could not decode message " +
                               boost::lexical_cast<std::string>(count));
    }
    if (dts->data_flag & BUFR_FLAG_INVALID)
    {
      bufr_free_message(msg);
      // continuing at this point may cause a segmentation fault
      throw std::runtime_error("Message number " + boost::lexical_cast<std::string>(count) +
                               " is invalid");
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
    if (options.verbose) std::cout << "Reading local tables B and D" << std::endl;

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
    try
    {
      read_message(file, messages, file_tables, tables_list, datacategories);
      succesful_parse_events++;
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
                             "), plase use the -C or --category option to select one");
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
      names.insert(value.second.name);
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
    NameMap::const_iterator it = pmap.find(name);
    if (it != pmap.end())
    {
      namemap.insert(NameMap::value_type(name, it->second));
      if (options.debug) std::cout << name << " = " << it->second.parId << std::endl;
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
    if (row.size() == 0) return;
    if (row[0].substr(0, 1) == "#") return;

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
    parInfo.bufrName = row[1];
    parInfo.shortName = row[3];
    pmap.insert(NameMap::value_type(parInfo.bufrName, parInfo));
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
    if (options.verbose) std::cout << "Reading " << options.conffile << std::endl;
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
  bool wmostations = true;
  NFmiAviationStationInfoSystem stations(wmostations, options.verbose);

  if (options.stationsfile.empty()) return stations;

  if (options.verbose) std::cout << "Reading " << options.stationsfile << std::endl;

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

NFmiVPlaceDescriptor create_vdesc(const Messages &messages, BufrDataCategory category)
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
      lbag.AddLevel(NFmiLevel(kFmiAmdarLevel, "AMDAR", 0));
      return NFmiVPlaceDescriptor(lbag);
    }
    case kBufrSounding:
    {
      // We number the levels sequentially. The number
      // with most measurements determines the total number of levels.

      int levels = count_sounding_levels(messages);

      if (levels == 0) return NFmiVPlaceDescriptor();

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

  typedef std::map<std::string, NFmiPoint> Stations;
  Stations stations;

  BOOST_FOREACH (const Message &msg, messages)
  {
    Message::const_iterator p_id = msg.find(1005);
    if (p_id == msg.end()) p_id = msg.find(1011);

    if (p_id != msg.end())
    {
      const std::string name = p_id->second.svalue;

      float lon = kFloatMissing, lat = kFloatMissing;

      p_id = msg.find(5001);
      if (p_id == msg.end()) p_id = msg.find(5002);
      if (p_id != msg.end()) lat = static_cast<float>(p_id->second.value);

      p_id = msg.find(6001);
      if (p_id == msg.end()) p_id = msg.find(6002);
      if (p_id != msg.end()) lon = static_cast<float>(p_id->second.value);

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
  Message::const_iterator wmoblock = msg.find(1001);
  Message::const_iterator wmonumber = msg.find(1002);
  Message::const_iterator latitude = msg.find(5001);
  Message::const_iterator longitude = msg.find(6001);

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

  return NFmiStation(wmo, boost::lexical_cast<std::string>(wmo), lon, lat);
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

  if (category == kBufrUpperAirLevel) return create_hdesc_amdar();

  if (category == kBufrSeaSurface) return create_hdesc_buoy_ship(messages);

  // First list all unique stations

  typedef std::set<NFmiStation> Stations;
  Stations stations;

  BOOST_FOREACH (const Message &msg, messages)
  {
    NFmiStation station = get_station(msg);

    if (station.GetLongitude() != kFloatMissing && station.GetLatitude() != kFloatMissing)
      stations.insert(station);
  }

  // Then build the descriptor. The message may contain no name for the
  // station, hence we use the stations file to name the stations if
  // possible.

  NFmiLocationBag lbag;
  BOOST_FOREACH (const NFmiStation &station, stations)
  {
    NFmiAviationStation *stationinfo = stationinfos.FindStation(station.GetIdent());

    if (stationinfo == 0)
    {
      // Cannot get a good name for the station
      lbag.AddLocation(station);
    }
    else
    {
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
  Message::const_iterator yy = msg.find(4001);
  Message::const_iterator mm = msg.find(4002);
  Message::const_iterator dd = msg.find(4003);
  Message::const_iterator hh = msg.find(4004);
  Message::const_iterator mi = msg.find(4005);

  if (yy == msg.end() || mm == msg.end() || dd == msg.end() || hh == msg.end() || mi == msg.end())
    throw std::runtime_error("Message does not contain all required date/time fields");

  const int timeresolution = 1;

  return NFmiMetTime(static_cast<short>(yy->second.value),
                     static_cast<short>(mm->second.value),
                     static_cast<short>(dd->second.value),
                     static_cast<short>(hh->second.value),
                     static_cast<short>(mi->second.value),
                     0,
                     timeresolution);
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish valid time for individual AMDAR
 *
 * Each message is expected to get a unique time. If there is a previous
 * identical time, the next message gets a time that is incremented
 * by one second. This is a requirement of the smartmet editor.
 *
 * 4001 YEAR
 * 4002 MONTH
 * 4003 DAY
 * 4004 HOUR
 * 4005 MINUTE
 */
// ----------------------------------------------------------------------

NFmiMetTime get_validtime_amdar(std::set<NFmiMetTime> &used_times, const Message &message)
{
  // We permit the seconds to be missing, since that seems
  // to be the case in actual messages
  short year = -1, month = -1, day = -1, hour = -1, minute = -1, second = 0;

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

  const int timestep = 0;
  NFmiMetTime t(year, month, day, hour, minute, second, timestep);

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
 * \brief Create AMDAR time descriptor
 *
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor create_tdesc_amdar(const Messages &messages)
{
  // Times used so far
  std::set<NFmiMetTime> validtimes;

  BOOST_FOREACH (const Message &msg, messages)
  {
    // Return value can be ignored here
    get_validtime_amdar(validtimes, msg);
  }

  // Then the final timelist

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
 * \brief Create time descriptor
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor create_tdesc(const Messages &messages, BufrDataCategory category)
{
  // Special cases

  if (category == kBufrUpperAirLevel) return create_tdesc_amdar(messages);

  // Normal cases

  std::set<NFmiMetTime> validtimes;
  BOOST_FOREACH (const Message &msg, messages)
    validtimes.insert(get_validtime(msg));

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
  if (rec.value == kFloatMissing) return kFloatMissing;

  // Kelvin to Celsius
  if (rec.units == "K") return static_cast<float>(rec.value - 273.15);

  // Pascal to hecto-Pascal
  if (rec.units == "PA") return static_cast<float>(rec.value / 100.0);

  // Cloud 8ths to 0-100%. Note that obs may also be 9, hence a min check is needed
  if (rec.name == "CLOUD AMOUNT" && rec.units == "CODE TABLE")
    return static_cast<float>(std::min(100.0, rec.value * 100 / 8));

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
    NameMap::const_iterator it = namemap.find(value.second.name);
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

  BOOST_FOREACH (const Message &msg, messages)
  {
    if (!info.Time(get_validtime(msg)))
      throw std::runtime_error("Internal error in handling valid times of the messages");

    NFmiStation station = get_station(msg);

    // We ignore stations with invalid coordinates
    if (!info.Location(station.GetIdent())) continue;

    if (laststation != station)
    {
      info.ResetLevel();
      laststation = station;
    }

    if (!info.NextLevel()) throw std::runtime_error("Changing to next level failed");

    copy_params(info, msg, namemap);
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

void copy_records_amdar(NFmiFastQueryInfo &info, const Messages &messages, const NameMap &namemap)
{
  info.First();

  // Valid times used so far. Algorithm must match create_tdesc at this point.

  std::set<NFmiMetTime> validtimes;

  BOOST_FOREACH (const Message &msg, messages)
  {
    // There is only one station and only one level, so the initial
    // info.First() is sufficient for setting both correctly.
    // However, the time is different for each message, possibly by
    // artificially added seconds.

    NFmiMetTime t = get_validtime_amdar(validtimes, msg);

    if (!info.Time(t))
      throw std::runtime_error("Internal error in handling valid times of AMDAR message");

    // Copy the extra lon/lat parameters we added in create_pdesc

    float lon = kFloatMissing, lat = kFloatMissing;

    Message::const_iterator it;

    it = msg.find(5001);
    if (it != msg.end()) lat = static_cast<float>(it->second.value);

    it = msg.find(6001);
    if (it != msg.end()) lon = static_cast<float>(it->second.value);

    if (lon != kFloatMissing && (lon < -180 || lon > 180))
    {
      if (options.debug)
        std::cerr
            << "Warning: BUFR message contains a station with longitude out of bounds [-180,180]: "
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
      info.Param(kFmiLongitude);
      info.FloatValue(lon);

      info.Param(kFmiLatitude);
      info.FloatValue(lat);

      // Copy regular parameters

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

  std::string laststation = "";

  BOOST_FOREACH (const Message &msg, messages)
  {
    if (!info.Time(get_validtime(msg)))
      throw std::runtime_error("Internal error in handling valid times of the messages");

    // Skip unknown locations
    Message::const_iterator p_id = msg.find(1005);
    if (p_id == msg.end())
    {
      p_id = msg.find(1011);
      if (p_id == msg.end()) continue;
    }

    const std::string &name = p_id->second.svalue;

    if (laststation != name)
    {
      // Find the station based on the name. If there's no match, the station is invalid and we skip
      // it
      if (!info.Location(name)) continue;
    }

    // Then copy the extra lon/lat parameters we added in create_pdesc

    float lon = kFloatMissing, lat = kFloatMissing;

    p_id = msg.find(5001);
    if (p_id == msg.end()) p_id = msg.find(5002);
    if (p_id != msg.end()) lat = static_cast<float>(p_id->second.value);

    p_id = msg.find(6001);
    if (p_id == msg.end()) p_id = msg.find(6002);
    if (p_id != msg.end()) lon = static_cast<float>(p_id->second.value);

    if (lon != kFloatMissing && (lon < -180 || lon > 180))
    {
      if (options.debug)
        std::cerr
            << "Warning: BUFR message contains a station with longitude out of bounds [-180,180]: "
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
                  BufrDataCategory category)
{
  // Handle special cases

  if (category == kBufrSounding) return copy_records_sounding(info, messages, namemap);

  if (category == kBufrUpperAirLevel) return copy_records_amdar(info, messages, namemap);

  if (category == kBufrSeaSurface) return copy_records_buoy_ship(info, messages, namemap);

  // Normal case with no funny business with levels or times

  info.First();

  BOOST_FOREACH (const Message &msg, messages)
  {
    if (!info.Time(get_validtime(msg)))
      throw std::runtime_error("Internal error in handling valid times of the messages");

    NFmiStation station = get_station(msg);

    // We ignore stations with bad coordinates
    if (!info.Location(station.GetIdent())) continue;

    copy_params(info, msg, namemap);
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
 * \brief Main program without exception handling
 */
// ----------------------------------------------------------------------

int run(int argc, char *argv[])
{
  if (!parse_options(argc, argv, options)) return 0;

#ifdef _MSC_VER
  if (options.debug) bufr_set_debug(1);
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
  Messages &messages = tmp.second;

  validate_category(category);

  // Guess the producer if so requested

  if (options.autoproducer) guess_producer(category);

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

  // Build a list of all parameter names

  std::set<std::string> names = collect_names(messages);
  NameMap namemap = map_names(names, parammap);

  // Build the querydata descriptors from the file names etc

  NFmiParamDescriptor pdesc = create_pdesc(namemap, category);
  NFmiVPlaceDescriptor vdesc = create_vdesc(messages, category);
  NFmiTimeDescriptor tdesc = create_tdesc(messages, category);
  NFmiHPlaceDescriptor hdesc = create_hdesc(messages, stations, category);

  // Initialize the data to missing values

  NFmiFastQueryInfo qi(pdesc, tdesc, hdesc, vdesc);
  boost::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(qi));
  if (data.get() == 0) throw std::runtime_error("Could not allocate memory for result data");

  NFmiFastQueryInfo info(data.get());

  info.SetProducer(NFmiProducer(options.producernumber, options.producername));

  // Add each file to the data

  copy_records(info, messages, namemap, category);

  // Output

  if (options.outfile == "-")
    std::cout << *data;
  else
  {
    std::ofstream out(options.outfile.c_str(),
                      std::ios::out | std::ios::binary);  // VC++ vaatii ett‰ tiedosto avataan
                                                          // bin‰‰risen‰ (Linuxissa se on default
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

int main(int argc, char *argv[]) try
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
