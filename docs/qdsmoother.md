The {﻿{qdsmoother}} command is used to smoothen querydata so that animations drawn from data would look better in animations.

## Command line

The command line syntax is

    qdsmoother [options] configfile inputquerydata outputquerydata

The available options are

* **-h**  
    Print help information on the command line.
* **-v**  
    Verbose mode.

## Configuration file

The configuration file lists

*    the parameters to be included in the output file
*    the times picked from the input file
*    how individual parameters are smoothed, if at all

### Parameters

The parameters can be given in a comma separated list as in

    parameters = Temperature,Pressure,Humidity

or often more conveniently on separate lines as in

    parameters = Temperature
    parameters += Pressure
    parameters += Humidity

### Times

By default all time steps in the input data will be processed. One can omit a sub interval of times using

    times::omit startoffset:endoffset:timestep

Once omissions have been done, a requirement can also be made to get time steps back in:

    times::require startoffset:endoffset:timestep

### Smoothers

The known smoother types are:

*    None
*    Neighbourhood. Weights are of the form 1/(1+d*d) where d=3/r*distance*factor
*    PseudoGaussian. Weights are of the form r*r/(r*r+distance*factor)  
    Distance in the formulas is the distance of the grid point being included in the smoothing from the grid point getting the smoothed value. Radius is the maximum distance for the grid points to be included in the weighted value. Factor is used to adjust the strength of the smoothening.

When smoothing is done, extra control parameters are used to define the strength and extent of the smoothing.
Examples:

    smoother::Temperature
    {
      type   = PseudoGaussian
      radius = 100   # kilometers
      factor = 12    # strength
    }
    smoother::Humidity
    {
      type   = Neighbourhood
      radius = 100
      factor = 16
    }
