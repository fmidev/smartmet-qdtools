qdscript runs SmartTool-scripts for querydata. For example SmartTool-script

    T = T + 273.15

changes Celcius-degrees to Kelvin and

    if (PAR353 == MISS){
        PAR353 = 0
    }

modifies missing values in parameter number 353 (Precipitation1h) to zero. Please refer to Smartmet-editor manual for more complicated examples and more detailed documentation about SmartTool-syntax.

Usage:

    qdscript [options] macroFile [qdata1 qdata2 _.] < inputdata > outputData

Options:

* **-i infile**  
 input querydata filename, default is to read standard input
* **-o outfile**  
 output querydata filename, default is to write standard output
* **-d dictionary-file**  
 defines what file to use as a dictionaqry for error messages
* **-a param1,param2,...**  
 parameters to be added to the data
* **-A id1,name1,id2,name2,...**  
 parameter id and name pairs to be added to the data
* **-l**  
 disables automatic looping over all levels
* **-s**  
 do not iterate time in data which have only a single time step

Example usages:

    qdscript MyScript.st < myData.sqd > modifiedData.sqd


    qdscript MyScript.st helpData1 helpData2 < myData.sqd > modifiedData.sqd