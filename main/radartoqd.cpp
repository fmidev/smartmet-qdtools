// ======================================================================
/*!
 * \brief BUFR radar data conversion to querydata
 *
 * Ref: http://www.eumetnet.eu/opera-software
 */
// ======================================================================

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <fmt/format.h>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiHPlaceDescriptor.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiTimeDescriptor.h>
#include <newbase/NFmiTimeList.h>
#include <newbase/NFmiVPlaceDescriptor.h>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef UNIX
#include <sys/ioctl.h>
#endif

// compression
#include <zlib.h>

#define MAXBLOCK 65534

// libbufr
extern "C"
{
#include <bufrlib.h>
}

// Bufr_io must be loaded after bufrlib.h
// Keep them in different extern block to prevent autoformatter from redordering them
extern "C"
{
#include <bufr_io.h>
}

// Default location of libbufr tables

#ifdef UNIX
std::string default_tabdir = "/usr/share/bufr";  // libbufr 3.2 default directory
#else
std::string default_tabdir = "C:/bufr";  // random guess until someone wants a fixed location
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Global instance for parameter name conversions
 */
// ----------------------------------------------------------------------

NFmiEnumConverter converter;

// ----------------------------------------------------------------------
/*!
 * \brief Structures from Opera apisample.h
 *
 */
// ----------------------------------------------------------------------

/* A coordinate pair */

struct point_t
{
  boost::optional<varfl> lat{}; /* latitude */
  boost::optional<varfl> lon{}; /* longitude */
};

/* Meta information about image */

struct meta_t
{
  boost::optional<int> year{};
  boost::optional<int> month{};
  boost::optional<int> day{};
  boost::optional<int> hour{};
  boost::optional<int> min{};
  point_t radar{};  // Radar position
  boost::optional<varfl> radar_height{};
  boost::optional<varfl> height_above_station{};
};

/* Level slicing table */

struct scale_t
{
  // dBZ quantization limits
  std::vector<varfl> dbz_values{};

  // rainfall intensities
  std::vector<varfl> intensity_values{};
  boost::optional<varfl> z_to_r_conversion{};
  boost::optional<varfl> z_to_r_conversion_factor{};
  boost::optional<varfl> z_to_r_conversion_exponent{};

  /* another method: */
  boost::optional<varfl> offset{};    /* offset */
  boost::optional<varfl> increment{}; /* increment */
};

/* Radar image */

struct img_t
{
  boost::optional<int> type{};            /* Image type */
  boost::optional<varfl> qual{};          /* quality indicator */
  boost::optional<int> grid{};            /* Co-ordinate grid type */
  point_t nw{};                           /* Northwest corner of the image */
  point_t ne{};                           /* NE corner */
  point_t se{};                           /* SE corner */
  point_t sw{};                           /* SW corner */
  boost::optional<int> nrows{};           /* Number of pixels per row */
  boost::optional<int> ncols{};           /* Number of pixels per column */
  boost::optional<varfl> psizex{};        /* Pixel size along x coordinate */
  boost::optional<varfl> psizey{};        /* Pixel size along y coordinate */
  scale_t scale{};                        /* Level slicing table */
  boost::optional<varfl> elevation{};     /* Antenna elevation angle */
  boost::optional<int> ns_organisation{}; /* North south view organisation */
  boost::optional<int> ew_organisation{}; /* East west view organisation */
  std::vector<varfl> heights{};           /* Heights */
  std::vector<varfl> cappi_heights{};     /* CAPPI heights */

  // These are present in some input BUFRs. These serve no useful purpose,
  // but we parse the respective messages in order to prevent warnings
  // and in order to provide useful debugging output.

  boost::optional<int> calibration_method{};
  boost::optional<int> clutter_treatment{};
  boost::optional<varfl> ground_occultation_correction{};
  boost::optional<varfl> range_attenuation_correction{};
  boost::optional<varfl> bright_band_correction{};
  boost::optional<varfl> radome_attenuation_correction{};
  boost::optional<varfl> clear_air_attenuation_correction{};
  boost::optional<varfl> precipitation_attenuation_correction{};

  // Parsed image data
  unsigned short *data{nullptr};
};

/* Projection information */

struct proj_t
{
  boost::optional<int> type{};      /* Projection type */
  boost::optional<varfl> majax{};   /* Semi-major axis or rotation ellipsoid */
  boost::optional<varfl> minax{};   /* Semi-minor axis or rotation ellipsoid */
  point_t origin{};                 /* Projection origin */
  boost::optional<int> xoff{};      /* False easting */
  boost::optional<int> yoff{};      /* False northing */
  boost::optional<varfl> stdpar1{}; /* 1st standard parallel */
  boost::optional<varfl> stdpar2{}; /* 2nd standard parallel */
};

/* This is our internal data structure */

struct radar_data_t
{
  boost::optional<int> wmoblock{}; /* WMO block number */
  boost::optional<int> wmostat{};  /* WMO station number */
  meta_t meta{};                   /* Meta information about the product */
  img_t img{};                     /* Radar reflectivity image */
  proj_t proj{};                   /* Projection information */
};

// ----------------------------------------------------------------------
/*!
 * \brief Global instance of the structure
 *
 * The decoder uses a call back technique which is easiest to implement
 * using a global. Using boost::bind would be an alternative.
 */
// ----------------------------------------------------------------------

radar_data_t radar_data;

// ----------------------------------------------------------------------
/*!
 * \brief Debugging output helpers
 */
// ----------------------------------------------------------------------

template <typename T>
std::ostream &operator<<(std::ostream &out, const boost::optional<T> &ob)
{
  if (!ob)
    out << '-';
  else
    out << *ob;
  return out;
}

template <typename T>
std::ostream &operator<<(std::ostream &out, const std::vector<T> &ob)
{
  if (ob.empty())
    out << "-";
  else if (ob.size() == 1)
    out << *ob.begin();
  else
  {
    bool first = true;
    out << '[';
    BOOST_FOREACH (const T &o, ob)
    {
      if (!first) out << ",";
      out << o;
      first = false;
    }
    out << ']';
  }
  return out;
}

std::ostream &operator<<(std::ostream &out, const point_t &pt)
{
  out << pt.lon << ',' << pt.lat;
  return out;
}

// ----------------------------------------------------------------------
/*!
 * \Projection types
 *
 * http://www.eumetnet.eu/sites/default/files/bufr_sw_desc.pdf
 * Section 3.6 Geographical projection information, page 23
 */
// ----------------------------------------------------------------------

std::string projection_type(int type)
{
  switch (type)
  {
    case 0:
      return "Gnomonic";
    case 1:
      return "Stereographic";
    case 2:
      return "Lambert's conic";
    case 3:
      return "Oblique Mercator";
    case 4:
      return "Azimuthal equidistant";
    case 5:
      return "Lambert azimuthal equal area";
    case 31:
      return "missing";
    default:
    {
      if (type >= 6 && type <= 30)
        return "reserved";
      else
        return "invalid";
    }
  }
}

std::string projection_type(const boost::optional<int> type)
{
  if (!type)
    return "-";
  else
    return projection_type(*type);
}

// ----------------------------------------------------------------------
/*!
 * \brief Print metadata from the radar_data structure
 */
// ----------------------------------------------------------------------

void print_metadata()
{
  const radar_data_t &b = radar_data;
  const proj_t &p = b.proj;
  const meta_t &m = b.meta;
  const img_t &i = b.img;

  std::cout << std::endl
            << "Radar BUFR metadata" << std::endl
            << "-------------------" << std::endl
            << "WMO Block                             = " << b.wmoblock << std::endl
            << "WMO Station                           = " << b.wmostat << std::endl
            << std::endl
            << "Year                                  = " << m.year << std::endl
            << "Month                                 = " << m.month << std::endl
            << "Day                                   = " << m.day << std::endl
            << "Hour                                  = " << m.hour << std::endl
            << "Minute                                = " << m.min << std::endl
            << "Position                              = " << m.radar << std::endl
            << "Height                                = " << m.radar_height << std::endl
            << "Height above station                  = " << m.height_above_station << std::endl
            << std::endl
            << "Projection type                       = " << p.type << " = "
            << projection_type(p.type) << std::endl
            << "Semi-major axis                       = " << p.majax << std::endl
            << "Semi-minor axis                       = " << p.minax << std::endl
            << "Origin                                = " << p.origin << std::endl
            << "False easting                         = " << p.xoff << std::endl
            << "False northing                        = " << p.yoff << std::endl
            << "Standard parallel 1                   = " << p.stdpar1 << std::endl
            << "Standard parallel 2                   = " << p.stdpar2 << std::endl
            << std::endl
            << "Image type                            = " << i.type << std::endl
            << "Quality indicator                     = " << i.qual << std::endl
            << "Co-ordinate grid type                 = " << i.grid << std::endl
            << "NW-corner                             = " << i.nw << std::endl
            << "NE-corner                             = " << i.ne << std::endl
            << "SE-corner                             = " << i.se << std::endl
            << "SW-corner                             = " << i.sw << std::endl
            << "Pixels per row                        = " << i.nrows << std::endl
            << "Pixels per column                     = " << i.ncols << std::endl
            << "Pixel size along X-dim                = " << i.psizex << std::endl
            << "Pixel size along Y-dim                = " << i.psizey << std::endl
            << "Antenna elevation angle               = " << i.elevation << std::endl
            << "North South organisation              = " << i.ns_organisation << std::endl
            << "East West organisation                = " << i.ew_organisation << std::endl
            << "Heights                               = " << i.heights << std::endl
            << "Calibration method                    = " << i.calibration_method << std::endl
            << "Clutter treatment                     = " << i.clutter_treatment << std::endl
            << "Ground occultation correction         = " << i.ground_occultation_correction
            << std::endl
            << "Range attenuation correction          = " << i.range_attenuation_correction
            << std::endl
            << "Bright-band correction                = " << i.bright_band_correction << std::endl
            << "Radome attenuation correction         = " << i.radome_attenuation_correction
            << std::endl
            << "Clear-air attenuation correction      = " << i.clear_air_attenuation_correction
            << std::endl
            << "Precipitation attentuation correction = " << i.precipitation_attenuation_correction
            << std::endl
            << std::endl
            << "dBZ scale                             = " << i.scale.dbz_values << std::endl
            << "intensity scale                       = " << i.scale.intensity_values << std::endl
            << "Z to R conversion                     = " << i.scale.z_to_r_conversion << std::endl
            << "Z to R conversion factor              = " << i.scale.z_to_r_conversion_factor
            << std::endl
            << "Z to R conversion exponent            = " << i.scale.z_to_r_conversion_exponent
            << std::endl
            << "dBZ offset (alpha)                    = " << i.scale.offset << std::endl
            << "dbZ increment (beta)                  = " << i.scale.increment << std::endl
            << std::endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Container for command line options
 */
// ----------------------------------------------------------------------

struct Options
{
  Options();

  bool verbose;              // -v --verbose
  bool quiet;                // -q --quiet
  bool debug;                //    --debug
  bool allow_overflow;       // --allow-overflow
  std::string tabdir;        // -t --tables
  std::string infile;        // -i --infile
  std::string outfile;       // -o --outfile
  std::string parameter;     // -p --param
  std::string projection;    // -P --projection
  std::string producername;  //    --producername
  long producernumber;       //    --producernumber
};

Options options;

// ----------------------------------------------------------------------
/*!
 * \brief Default options
 */
// ----------------------------------------------------------------------

Options::Options()
    : verbose(false),
      quiet(false),
      debug(false),
      allow_overflow(false),
      tabdir(default_tabdir),
      infile("-"),
      outfile("-"),
      parameter(),
      projection(),
      producername("RADAR"),
      producernumber(1014)
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
  namespace fs = boost::filesystem;
  namespace ba = boost::algorithm;

  std::string producerinfo;
  std::string tab_msg = "BUFR tables directory (default=" + default_tabdir + ")";

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
      "quiet,q", po::bool_switch(&options.quiet), "disable warning messages")(
      "debug", po::bool_switch(&options.debug), "print debugging information")(
      "allow-overflow",
      po::bool_switch(&options.allow_overflow),
      "allow overflow in packed intensities")(
      "tabdir,t", po::value(&options.tabdir), tab_msg.c_str())(
      "infile,i", po::value(&options.infile), "input BUFR file")(
      "outfile,o", po::value(&options.outfile), "output querydata file")(
      "param", po::value(&options.parameter), "parameter name for output")(
      "projection,P", po::value(&options.projection), "output projection")(
      "producer,p", po::value(&producerinfo), "producer number,name")(
      "producernumber", po::value(&options.producernumber), "producer number (default: 1014)")(
      "producername", po::value(&options.producername), "producer name (default: RADAR)");

  po::positional_options_description p;
  p.add("infile", 1);
  p.add("outfile", 1);

  po::variables_map opt;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);

  po::notify(opt);

  if (opt.count("version") != 0)
  {
    std::cout << "radartoqd v1.0 (" << __DATE__ << ' ' << __TIME__ << ')' << std::endl;
  }

  if (opt.count("help"))
  {
    std::cout << "Usage: radartoqd [options] infile outfile" << std::endl
              << std::endl
              << "Converts Opera BUFR radar data to querydata." << std::endl
              << std::endl
              << desc << std::endl
              << std::endl;
    return false;
  }

  if (opt.count("infile") == 0)
    throw std::runtime_error("Expecting input BUFR file as parameter 1");

  if (opt.count("outfile") == 0) throw std::runtime_error("Expecting output file as parameter 2");

  if (!fs::exists(options.infile))
    throw std::runtime_error("Input BUFR '" + options.infile + "' does not exist");

  // Handle the alternative ways to define the producer

  if (!producerinfo.empty())
  {
    std::vector<std::string> parts;
    ba::split(parts, producerinfo, ba::is_any_of(","));
    if (parts.size() != 2)
      throw std::runtime_error("Option --producer expects a comma separated number,name argument");

    options.producernumber = Fmi::stol(parts[0]);
    options.producername = parts[1];
  }

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Byte swap of 64bit values if host platform uses big endian
 *
 * \param[in,out] buf  buffer holding 64 bit values
 * \param[in]     n    number of 64 bit values in buffer
 */
// ----------------------------------------------------------------------

void byteswap64(unsigned char *buf, int n)
{
  int i;
  unsigned char c;

  unsigned one = 1;
  unsigned char *test = (unsigned char *)&one;

  if (*test == 1) return;

  for (i = 0; i < n; i += 8)
  {
    c = buf[0];
    buf[0] = buf[7];
    buf[7] = c;
    c = buf[1];
    buf[1] = buf[6];
    buf[6] = c;
    c = buf[2];
    buf[2] = buf[5];
    buf[5] = c;
    c = buf[3];
    buf[3] = buf[4];
    buf[4] = c;
    buf += 8;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Callback for storing the values in a radar_data_t structure
 *
 * See libbufr bufr_data_to_file() for original sample codeo.
 */
// ----------------------------------------------------------------------

static int bufr_callback(varfl val, int ind)
{
  static int in_seq = 0;   /* flag to indicate sequences */
  static int first_in_seq; /* flag to indicate first element in sequence */

  std::string imgfile = "bufr_image";

  std::string fname = imgfile;

  /* element descriptor */

  if (des[ind]->id == ELDESC)
  {
    dd *d = &(des[ind]->el->d);

    /* output descriptor if not inside a sequence */

    if (options.debug && !in_seq && ind != ccitt_special && ind != add_f_special)
      fprintf(stdout, "%2d %2d %3d ", d->f, d->x, d->y);

    /* descriptor without data (1.x.y, 2.x.y) or ascii) */

    if (ind == _desc_special)
    {
      /* descriptor -> add newline */

      if (!in_seq && options.debug)
      {
        fprintf(stdout, "\n");
      }
    }

    /* "normal" data */

    else
    {
      /* check for missing values and flag tables */

      if (options.debug)
      {
        char sval[80];

        if (val == MISSVAL)
          strcpy(sval, "      missing");
        else
          sprintf(sval, "%15.7f", val);

        /* do we have a descriptor before the data element? */

        if (!in_seq && ind != add_f_special)
          fprintf(stdout, "%s            %s\n", sval, des[ind]->el->elname);
        else
        {
          if (!first_in_seq) fprintf(stdout, "          ");

          fprintf(stdout, "%s  %2d %2d %3d %s\n", sval, d->f, d->x, d->y, des[ind]->el->elname);
          first_in_seq = 0;
        }
      }

      // Store known descriptors
      if (val != MISSVAL)
      {
        if (bufr_check_fxy(d, 0, 1, 1)) /* WMO block number */
          radar_data.wmoblock = val;
        else if (bufr_check_fxy(d, 0, 1, 2)) /* WMO station number */
          radar_data.wmostat = val;

        else if (bufr_check_fxy(d, 0, 2, 135)) /* Antenna elevation */
          radar_data.img.elevation = val;

        else if (bufr_check_fxy(d, 0, 4, 1)) /* Year */
          radar_data.meta.year = val;
        else if (bufr_check_fxy(d, 0, 4, 2)) /* Month */
          radar_data.meta.month = val;
        else if (bufr_check_fxy(d, 0, 4, 3)) /* Day */
          radar_data.meta.day = val;
        else if (bufr_check_fxy(d, 0, 4, 4)) /* Hour */
          radar_data.meta.hour = val;
        else if (bufr_check_fxy(d, 0, 4, 5)) /* Minute */
          radar_data.meta.min = val;

        else if (bufr_check_fxy(d, 0, 5, 2)) /* Latitude */
        {
          if (!radar_data.img.nw.lat)
            radar_data.img.nw.lat = val;
          else if (!radar_data.img.ne.lat)
            radar_data.img.ne.lat = val;
          else if (!radar_data.img.se.lat)
            radar_data.img.se.lat = val;
          else if (!radar_data.img.sw.lat)
            radar_data.img.sw.lat = val;
          else if (!radar_data.meta.radar.lat)
            radar_data.meta.radar.lat = val;
          else if (!options.quiet)
            std::cerr << "Ignoring latitudes after 5th time" << std::endl;
        }
        else if (bufr_check_fxy(d, 0, 5, 33)) /* Pixel size on horizontal */
          radar_data.img.psizex = val;

        else if (bufr_check_fxy(d, 0, 6, 2)) /* Longitude */
        {
          if (!radar_data.img.nw.lon)
            radar_data.img.nw.lon = val;
          else if (!radar_data.img.ne.lon)
            radar_data.img.ne.lon = val;
          else if (!radar_data.img.se.lon)
            radar_data.img.se.lon = val;
          else if (!radar_data.img.sw.lon)
            radar_data.img.sw.lon = val;
          else if (!radar_data.meta.radar.lon)
            radar_data.meta.radar.lon = val;
          else if (!options.quiet)
            std::cerr << "Ignoring longitudes after 5th time" << std::endl;
        }
        else if (bufr_check_fxy(d, 0, 6, 33)) /* Pixel size on vertical */
          radar_data.img.psizey = val;

        else if (bufr_check_fxy(d, 0, 7, 1)) /* Height of station */
          radar_data.meta.radar_height = val;
        else if (bufr_check_fxy(d, 0, 7, 6)) /* Height above station */
          radar_data.meta.height_above_station = val;

        else if (bufr_check_fxy(d, 0, 10, 7)) /* Measurement heigts */
          radar_data.img.heights.push_back(val);

        else if (bufr_check_fxy(d, 0, 21, 200)) /* Measurement heigts */
          radar_data.img.cappi_heights.push_back(val);

        else if (bufr_check_fxy(d, 0, 21, 1)) /* dBZ measurement limit */
          radar_data.img.scale.dbz_values.push_back(val);
        else if (bufr_check_fxy(d, 0, 21, 36)) /* rainfall intensity limit */
          radar_data.img.scale.intensity_values.push_back(val);

        else if (bufr_check_fxy(d, 0, 25, 6)) /* Z to R conversion */
          radar_data.img.scale.z_to_r_conversion = val;
        else if (bufr_check_fxy(d, 0, 25, 7)) /* Z to R conversion factor */
          radar_data.img.scale.z_to_r_conversion_factor = val;
        else if (bufr_check_fxy(d, 0, 25, 8)) /* Z to R conversion exponent */
          radar_data.img.scale.z_to_r_conversion_exponent = val;

        else if (bufr_check_fxy(d, 0, 25, 9)) /* Calibration method*/
          radar_data.img.calibration_method = val;
        else if (bufr_check_fxy(d, 0, 25, 10)) /* Clutter treatment */
          radar_data.img.clutter_treatment = val;
        else if (bufr_check_fxy(d, 0, 25, 11)) /* Ground occultation correction (screening)*/
          radar_data.img.ground_occultation_correction = val;
        else if (bufr_check_fxy(d, 0, 25, 12)) /* Range attenuation correction */
          radar_data.img.range_attenuation_correction = val;
        else if (bufr_check_fxy(d, 0, 25, 13)) /* Bright-band correction */
          radar_data.img.bright_band_correction = val;
        else if (bufr_check_fxy(d, 0, 25, 15)) /* Radome attenuation correction */
          radar_data.img.radome_attenuation_correction = val;
        else if (bufr_check_fxy(d, 0, 25, 16)) /* Clear-air attenuation correction */
          radar_data.img.clear_air_attenuation_correction = val;
        else if (bufr_check_fxy(d, 0, 25, 17)) /* Precipitation attenuation correction */
          radar_data.img.precipitation_attenuation_correction = val;

        // WHY IS THERE 29.1 and 29.201 ???

        else if (bufr_check_fxy(d, 0, 29, 1)) /* Projection type */
          radar_data.proj.type = val;
        else if (bufr_check_fxy(d, 0, 29, 2)) /* Co-ordinate grid type */
          radar_data.img.grid = (int)val;
        else if (bufr_check_fxy(d, 0, 21, 198)) /* dBZ Value offset */
          radar_data.img.scale.offset = val;
        else if (bufr_check_fxy(d, 0, 21, 199)) /* dBZ Value increment */
          radar_data.img.scale.increment = val;
        else if (bufr_check_fxy(d, 0, 29, 193)) /* Longitude Origin */
          radar_data.proj.origin.lon = val;
        else if (bufr_check_fxy(d, 0, 29, 194)) /* Latitude Origin */
          radar_data.proj.origin.lat = val;
        else if (bufr_check_fxy(d, 0, 29, 195)) /* False Easting */
          radar_data.proj.xoff = (int)val;
        else if (bufr_check_fxy(d, 0, 29, 196)) /* False Northing */
          radar_data.proj.yoff = (int)val;
        else if (bufr_check_fxy(d, 0, 29, 197)) /* 1st Standard Parallel */
          radar_data.proj.stdpar1 = val;
        else if (bufr_check_fxy(d, 0, 29, 198)) /* 2nd Standard Parallel */
          radar_data.proj.stdpar2 = val;
        else if (bufr_check_fxy(d, 0, 29, 199)) /* Semi-major axis of ellipsoid */
          radar_data.proj.majax = val;
        else if (bufr_check_fxy(d, 0, 29, 200)) /* Semi-minor axis of ellipsoid */
          radar_data.proj.minax = val;
        else if (bufr_check_fxy(d, 0, 29, 201)) /* Projection type */
          radar_data.proj.type = val;

        else if (bufr_check_fxy(d, 0, 30, 21)) /* Number of pixels per row */
          radar_data.img.nrows = val;
        else if (bufr_check_fxy(d, 0, 30, 22)) /* Number of pixels per column */
          radar_data.img.ncols = val;
        else if (bufr_check_fxy(d, 0, 30, 192)) /* North south view organisation */
          radar_data.img.ns_organisation = val;
        else if (bufr_check_fxy(d, 0, 30, 193)) /* East west view organisation */
          radar_data.img.ew_organisation = val;

        else if (bufr_check_fxy(d, 0, 30, 31)) /* Image type */
          radar_data.img.type = (int)val;
        else if (bufr_check_fxy(d, 0, 30, 32)) /* Combination with other data */
          ;                                    // we have no use for the value

        else if (bufr_check_fxy(d, 0, 31, 1)) /* Delayed descriptor replication factor */
          ;

        else if (bufr_check_fxy(d, 0, 33, 3)) /* Quality information */
          radar_data.img.qual = val;

        else
        {
          if (!options.quiet)
            fprintf(stderr, "Unknown element descriptor %d %d %d\n", d->f, d->x, d->y);
        }
      }
    }

  } /* end if ("Element descriptor") */

  /* sequence descriptor */

  else if (des[ind]->id == SEQDESC)
  {
    dd *d = &(des[ind]->seq->d);

    /* output descriptor if not inside another sequence descriptor */

    if (options.debug && !in_seq) fprintf(stdout, "%2d %2d %3d ", d->f, d->x, d->y);

    // Detect bitmaps. From desc.c: 3,21,192 3,21,193 3,21,194 3,21,195 3,21,196 3,21,197 3,21,200
    // 3,21,202

    int depth = check_bitmap_desc(d);

    if (depth > 0)  // 3,21,193 etc descriptors, see bufr.c desc.c list
    {
      /* read bitmap and run length decode */

      bufrval_t *vals = bufr_open_val_array();
      if (vals == nullptr) return 0;

      if (!bufr_parse_out(des[ind]->seq->del, 0, des[ind]->seq->nel - 1, bufr_val_to_global, 0))
      {
        bufr_close_val_array();
        return 0;
      }

      // Decode vals to our output array
      int nvals, nrows, ncols;

      if (!rldec_to_mem(vals->vals, &(radar_data.img.data), &nvals, &nrows, &ncols))
      {
        bufr_close_val_array();
        fprintf(stderr, "Error during runlength-decompression.\n");
        return 0;
      }

      if (options.debug)
        std::cerr << " image of size " << nrows << 'x' << ncols << " from " << nvals << " values\n";

      // TODO: Should we push data to a vector of raw buffers???

      bufr_close_val_array();
      return 1;
    }

    else
    {
#if 0
		  if(options.debug)
			fprintf (stderr, "Expanding sequence descriptor %d %d %d\n", d->f, d->x, d->y);
#endif
    }

    /* normal sequence descriptor - just call bufr_parse_out and
           remember that we are in a sequence */

    if (in_seq == 0) first_in_seq = 1;
    in_seq++;
    int ok = bufr_parse_out(des[ind]->seq->del, 0, des[ind]->seq->nel - 1, &bufr_callback, 1);
    bufr_close_val_array();
    in_seq--;
    return ok;

  } /* if ("seqdesc") */
  return 1;
}

// ----------------------------------------------------------------------
/*!
 * \brief Read the BUFR data into the global radar_data structure
 */
// ----------------------------------------------------------------------

void read_bufr()
{
  // Read the BUFR message.

  bufr_t bufr_msg;
  memset(&bufr_msg, 0, sizeof(bufr_t));

  if (!bufr_read_file(&bufr_msg, options.infile.c_str()))
  {
    bufr_free_data(&bufr_msg);
    throw std::runtime_error("Failed to read BUFR message from '" + options.infile + "'");
  }

  // Decode section 1

  sect_1_t s1;
  memset(&s1, 0, sizeof(sect_1_t));
  if (!bufr_decode_sections01(&s1, &bufr_msg))
  {
    bufr_free_data(&bufr_msg);
    throw std::runtime_error("Failed to decode BUFR section 1 from '" + options.infile + "'");
  }

  // Read descriptor tables

  char *tabdir = const_cast<char *>(options.tabdir.c_str());  // C-API annoyance

  if (read_tables(tabdir, s1.vmtab, s1.vltab, s1.subcent, s1.gencent) < 0)
  {
    bufr_free_data(&bufr_msg);
    throw std::runtime_error("Failed to read descriptor tables from '" + options.tabdir + "'");
  }

  // Decode data descriptor and data section into a radar data structure

  radar_data = radar_data_t{};
  // memset(&radar_data, 0, sizeof(radar_data_t));

  // Open bitstreams for section 3 and 4

  int subsets = 0;
  int desch = bufr_open_descsec_r(&bufr_msg, &subsets);
  bool ok = (desch >= 0);

  if (ok) ok = (bufr_open_datasect_r(&bufr_msg) >= 0);

  // Calculate number of data descriptors

  int ndescs = bufr_get_ndescs(&bufr_msg);

  // Allocate memory and read data descriptors from bitstream

  dd *dds = nullptr;
  if (ok) ok = bufr_in_descsec(&dds, ndescs, desch);

  if (!ok)
    throw std::runtime_error("Failed to parse the BUFR data according to Opera specifications");

  // Output data to our data structure

  if (ok)
    while (subsets--)
      ok = bufr_parse_out(dds, 0, ndescs - 1, &bufr_callback, 1);

  // Close bitstreams and free descriptor array

  if (dds != nullptr) free(dds);
  bufr_close_descsec_r(desch);
  bufr_close_datasect_r();

  bufr_free_data(&bufr_msg);
  free_descs();
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the parameter descriptor from radar_data
 *
 * If the user has chosen a parameter name, use it.
 * Otherwise make an educated guess based on the parsed metadata.
 */
// ----------------------------------------------------------------------

NFmiParamDescriptor create_pdesc()
{
  NFmiParamBag pbag;

  if (!options.parameter.empty())
  {
    FmiParameterName p = FmiParameterName(converter.ToEnum(options.parameter));
    if (p == kFmiBadParameter)
      throw std::runtime_error("Unknown parameter name: '" + options.parameter + "'");

    NFmiParam param(p, options.parameter);
    param.InterpolationMethod(kLinearly);
    pbag.Add(NFmiDataIdent(param));
  }
  else
  {
    if (!radar_data.img.scale.dbz_values.empty() ||
        (!!radar_data.img.scale.offset && !!radar_data.img.scale.increment))
    {
      NFmiParam param(kFmiReflectivity, "Reflectivity");
      param.InterpolationMethod(kLinearly);
      pbag.Add(NFmiDataIdent(param));
    }
    else if (!radar_data.img.scale.intensity_values.empty())
    {
      NFmiParam param(kFmiPrecipitationRate, "PrecipitationRate");
      param.InterpolationMethod(kLinearly);
      pbag.Add(NFmiDataIdent(param));
    }
    else
      throw std::runtime_error(
          "Unable to guess parameter name from metadata, specify parameter with --param");
  }

  return NFmiParamDescriptor(pbag);
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the vertical descriptor from radar_data
 */
// ----------------------------------------------------------------------

NFmiVPlaceDescriptor create_vdesc()
{
  if (!radar_data.img.heights.empty())
    throw std::runtime_error("Heights list is not empty: this format is not supported yet");

  if (!radar_data.img.cappi_heights.empty())
    throw std::runtime_error("CAPPI heights list is not empty: this format is not supported yet");

  return NFmiVPlaceDescriptor();
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the horizontal descriptor from radar_data
 */
// ----------------------------------------------------------------------

NFmiHPlaceDescriptor create_hdesc()
{
  if (!radar_data.proj.type) throw std::runtime_error("Projection type not set in BUFR");

  double central_lon = 0;
  double central_lat = 90;
  double true_lat = 0;

  if (radar_data.proj.origin.lon) central_lon = *radar_data.proj.origin.lon;
  if (radar_data.proj.origin.lat) central_lat = *radar_data.proj.origin.lat;
  if (radar_data.proj.stdpar1) true_lat = *radar_data.proj.stdpar1;  // TODO: Is this correct???

  if (!radar_data.img.sw.lat || !radar_data.img.sw.lon)
    throw std::runtime_error("SW corner coordinate not set");
  if (!radar_data.img.ne.lat || !radar_data.img.ne.lon)
    throw std::runtime_error("NE corner coordinate not set");

  NFmiPoint bottomleft(*radar_data.img.sw.lon, *radar_data.img.sw.lat);
  NFmiPoint bottomright(*radar_data.img.se.lon, *radar_data.img.se.lat);
  NFmiPoint topright(*radar_data.img.ne.lon, *radar_data.img.ne.lat);
  NFmiPoint topleft(*radar_data.img.nw.lon, *radar_data.img.nw.lat);

  if (!radar_data.img.nrows) throw std::runtime_error("Number of rows not set in metadata");
  if (!radar_data.img.ncols) throw std::runtime_error("Number of columns not set in metadata");

  int ny = *radar_data.img.nrows;
  int nx = *radar_data.img.ncols;

  NFmiPoint corner1(0, 0);
  NFmiPoint corner2(1, 1);

  switch (*radar_data.proj.type)
  {
    case 0:
    {
      auto proj = fmt::format(
          "+proj=gnom +lat_0={} +lat_ts={} +lon_0={} +k=1 +x_0=0 +y_0=0 +R={:.0f} "
          "+units=m +wktext +towgs84=0,0,0 +no_defs",
          central_lat,
          true_lat,
          central_lon,
          kRearth);
      auto *area = NFmiArea::CreateFromCorners(proj, "FMI", bottomleft, topright);
      return NFmiHPlaceDescriptor(NFmiGrid(area, nx, ny));
    }
    case 1:
    {
      auto proj = fmt::format(
          "+proj=stere +lat_0={} +lat_ts={} +lon_0={} +k=1 +x_0=0 +y_0=0 +R={:.0f} "
          "+units=m +wktext +towgs84=0,0,0 +no_defs",
          central_lat,
          true_lat,
          central_lon,
          kRearth);
      auto *area = NFmiArea::CreateFromCorners(proj, "FMI", bottomleft, topright);
      return NFmiHPlaceDescriptor(NFmiGrid(area, nx, ny));
    }
    case 2:
    {
      auto proj = fmt::format(
          "+proj=lcc +lat_0={} +lon_0={} +x_0=0 +y_0=0 +R={:.0f} +units=m +wktext "
          "+towgs84=0,0,0 +no_defs",
          central_lat,
          central_lon,
          kRearth);
      auto *area = NFmiArea::CreateFromCorners(proj, "FMI", bottomleft, topright);
      return NFmiHPlaceDescriptor(NFmiGrid(area, nx, ny));
    }
    case 3:
    {
      auto proj =
          fmt::format("+proj=merc +lon_0={} +wktext +over +towgs84=0,0,0 +no_defs", central_lon);
      auto *area = NFmiArea::CreateFromCorners(proj, "FMI", bottomleft, topright);
      return NFmiHPlaceDescriptor(NFmiGrid(area, nx, ny));
    }
    case 4:
    {
      auto proj = fmt::format(
          "+proj=aeqd +lat_0={} +lon_0={} +x_0=0 +y_0=0 +R={:.0f} +units=m +wktext "
          "+towgs84=0,0,0 +no_defs",
          central_lat,
          central_lon,
          kRearth);
      auto *area = NFmiArea::CreateFromCorners(proj, "FMI", bottomleft, topright);
      return NFmiHPlaceDescriptor(NFmiGrid(area, nx, ny));
    }
    case 5:
    {
      auto proj = fmt::format(
          "+proj=laea +lat_0={} +lon_0={} +x_0=0 +y_0=0 +R={:.0f} +units=m +wktext "
          "+towgs84=0,0,0 +no_defs",
          central_lat,
          central_lon,
          kRearth);
      auto *area = NFmiArea::CreateFromCorners(proj, "FMI", bottomleft, topright);
      return NFmiHPlaceDescriptor(NFmiGrid(area, nx, ny));
    }
    default:
      throw std::runtime_error("Unknown projection type " +
                               boost::lexical_cast<std::string>(*radar_data.proj.type));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Create the time descriptor from radar_data
 */
// ----------------------------------------------------------------------

NFmiTimeDescriptor create_tdesc()
{
  if (!radar_data.meta.year) throw std::runtime_error("Year has not been set in the BUFR metadata");
  if (!radar_data.meta.month)
    throw std::runtime_error("Month has not been set in the BUFR metadata");
  if (!radar_data.meta.day) throw std::runtime_error("Day has not been set in the BUFR metadata");
  if (!radar_data.meta.hour) throw std::runtime_error("Hour has not been set in the BUFR metadata");
  if (!radar_data.meta.min)
    throw std::runtime_error("Minute has not been set in the BUFR metadata");

  NFmiMetTime t(*radar_data.meta.year,
                *radar_data.meta.month,
                *radar_data.meta.day,
                *radar_data.meta.hour,
                *radar_data.meta.min,
                0,
                0);

  NFmiTimeList tlist;
  tlist.Add(new NFmiMetTime(t));
  return NFmiTimeDescriptor(t, tlist);
}

// ----------------------------------------------------------------------
/*!
 * \brief Decode a BUFR bitmap value
 */
// ----------------------------------------------------------------------

float decode_value(unsigned short value)
{
  float ret = kFloatMissing;

  if (value == std::numeric_limits<unsigned short>::max())
    ;

  else if (!!radar_data.img.scale.offset && !!radar_data.img.scale.increment)
    ret = *radar_data.img.scale.offset + value * *radar_data.img.scale.increment;

  else if (!radar_data.img.scale.dbz_values.empty())
  {
    auto n = radar_data.img.scale.dbz_values.size();

    if (value == 0)
      ret = -32;
    else if (static_cast<size_t>(value - 1) < n)
      ret = radar_data.img.scale.dbz_values[value - 1];
    else if (!options.allow_overflow)
      throw std::runtime_error("Overflow index " + boost::lexical_cast<std::string>(value) +
                               ", size of legend is " + boost::lexical_cast<std::string>(n));
    else
    {
      ret = radar_data.img.scale.dbz_values[n - 1];
      if (!options.quiet)
        std::cerr << "Warning: Overflow index " << value << ", size of legend is only " << n
                  << std::endl;
    }
  }
  else if (!radar_data.img.scale.intensity_values.empty())
  {
    auto n = radar_data.img.scale.intensity_values.size();

    if (value == 0)
      ret = 0;
    else if (static_cast<size_t>(value - 1) < n)
      ret = radar_data.img.scale.intensity_values[value - 1];
    else if (!options.allow_overflow)
      throw std::runtime_error("Overflow index " + boost::lexical_cast<std::string>(value) +
                               ", size of legend is " + boost::lexical_cast<std::string>(n));
    else
    {
      ret = radar_data.img.scale.intensity_values[n - 1];
      if (!options.quiet)
        std::cerr << "Warning: Overflow index " << value << ", size of legend is only " << n
                  << std::endl;
    }
  }
  else
    throw std::runtime_error("No known method for decoding the bitmap values has been set");

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Copy the raw data into the querydata
 */
// ----------------------------------------------------------------------

void copy_data(NFmiFastQueryInfo &info)
{
  if (radar_data.img.data == nullptr)
    throw std::runtime_error("No radar data found from the image");

  // Horizontal descriptor has been made so this is safe

  int ny = *radar_data.img.nrows;
  int nx = *radar_data.img.ncols;

  // We assume bitmap data starts from NW corner, we have no other sample data

  if (radar_data.img.ns_organisation) throw std::runtime_error("North-South view not supported");
  if (radar_data.img.ew_organisation) throw std::runtime_error("East-West view not supported");

  info.First();
  long pos = 0;
  for (info.ResetLocation(); info.NextLocation();)
  {
    int j = pos / nx;
    int i = pos % nx;
    float value =
        decode_value(radar_data.img.data[i + (ny - j - 1) * nx]);  // flip data in Y-direction
    info.FloatValue(value);
    ++pos;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief BUFR corners should be in NW, NE, SE, NW order
 *
 * But they aren't in some data. Fix it.
 */
// ----------------------------------------------------------------------

void check_corners()
{
  if (radar_data.img.se.lon && radar_data.img.sw.lon)
  {
    if (*radar_data.img.se.lon < *radar_data.img.sw.lon)
      std::swap(radar_data.img.se, radar_data.img.sw);
  }
  if (radar_data.img.ne.lon && radar_data.img.nw.lon)
  {
    if (*radar_data.img.ne.lon < *radar_data.img.nw.lon)
      std::swap(radar_data.img.ne, radar_data.img.nw);
  }
  if (radar_data.img.se.lat && radar_data.img.ne.lat)
  {
    if (*radar_data.img.se.lat > *radar_data.img.ne.lat)
      std::swap(radar_data.img.se, radar_data.img.ne);
  }
  if (radar_data.img.sw.lat && radar_data.img.nw.lat)
  {
    if (*radar_data.img.sw.lat > *radar_data.img.nw.lat)
      std::swap(radar_data.img.sw, radar_data.img.nw);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Create querydata from the input BUFR
 *
 *  See Opera sample apisample.c in libbufr for original sample code
 */
// ----------------------------------------------------------------------

boost::shared_ptr<NFmiQueryData> make_querydata()
{
  // Create the output projection if there is one. We do it before doing any
  // work so that the user gets a fast response to a possible syntax error

  boost::shared_ptr<NFmiArea> area;
  if (!options.projection.empty()) area = NFmiAreaFactory::Create(options.projection);

  // Parse the BUFR data

  read_bufr();

  // Check corners

  check_corners();

  // Print metadata

  if (options.debug || options.verbose) print_metadata();

  // Build descriptors from parsed BUFR

  NFmiParamDescriptor pdesc = create_pdesc();
  NFmiVPlaceDescriptor vdesc = create_vdesc();
  NFmiTimeDescriptor tdesc = create_tdesc();
  NFmiHPlaceDescriptor hdesc = create_hdesc();

  // Initialize output data

  NFmiFastQueryInfo qi(pdesc, tdesc, hdesc, vdesc);
  boost::shared_ptr<NFmiQueryData> qd(NFmiQueryDataUtil::CreateEmptyData(qi));
  if (qd.get() == 0) throw std::runtime_error("Failed to allocate memory for resulting querydata");

  NFmiFastQueryInfo info(qd.get());
  info.SetProducer(NFmiProducer(options.producernumber, options.producername));

  // Copy the raw data

  copy_data(info);

  if (area)
  {
    int width = static_cast<int>(round(area->XYArea(area.get()).Width()));
    int height = static_cast<int>(round(area->XYArea(area.get()).Height()));

    NFmiGrid grid(area.get(), width, height);
    boost::shared_ptr<NFmiQueryData> tmp(
        NFmiQueryDataUtil::Interpolate2OtherGrid(qd.get(), &grid, nullptr));
    std::swap(qd, tmp);
  }

  return qd;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program without exception handling
 */
// ----------------------------------------------------------------------

int run(int argc, char *argv[])
{
  if (!parse_options(argc, argv, options)) return 0;

  auto qd = make_querydata();

  if (qd)
    qd->Write(options.outfile);
  else
    throw std::runtime_error("Failed to create querydata from the message");

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
  std::cerr << "Error: " << e.what() << std::endl;
  return 1;
}
catch (...)
{
  std::cerr << "Error: Caught an unknown exception" << std::endl;
  return 1;
}
