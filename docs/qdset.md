qdset can be used to change parameter names, numbers, producers and so on.

See also: [Querydata producer names](querydata-producer-names.md)

Usage:

    qdset [options] inputfile parameter_id

parameter_id can be number (for example 4 is temperature) or name (for example Temperature). parameter_id defines parameter that is modified.

Options:

* **-n paramName**  
    New parameter name
* **-d New parameter id**  
* **-N producerName**  
    New producer name
* **-D New producer id**  
* **-l Parameter's new min value**  
* **-u Parameter's new max value**
* **-i <0-5> New interpolation method**  
* **-t <0-8> New parameter type**  
* **-s New parameter scale value**  
* **-b New parameter base value**  
* **-p precisionString New parameter precision string**  

Example usage:

    Example usage: qdset
    -n Lämpötila dataFile Temperature

(Lämpötila means temperature in finnish language)

Possible interpolation methods (use number to select):

*    kNoneInterpolation = 0
*    kLinearly = 1
*    kNearestPoint = 2
*    kByCombinedParam = 3
*    kLinearlyFast = 4
*    kLagrange = 5

Possible parameter types:

*    kUndefinedParam = 0,
*    kContinuousParam = 1,
*    kSteppingParam = 2,
*    kNumberParam = 3,
*    kCharacterParam = 4,
*    kSymbolicParam = 5,
*    kSimpleSymbolicParam = 6,
*    kIntervalParam = 7,
*    kWindBarbParam = 8,
*    kIncrementalParam = 100

