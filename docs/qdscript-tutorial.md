qdscript is a program to modify querydata and create calculated parameters with a SmartTool-scripts. Exactly same scripts work in Smartmet-editor and qdscript. In many cases, meteorologists can create and test the script in the editor, which can then be ran automatically in production environment with qdscript.

A fictional very trivial sailing index is created in this tutorial. The tutorial demonstrates:

1. how to investigate required information with qdinfo
2. how to create a simple script and calculate the index with that
3. how to put calculation into a routine.

Related tutorials:

* [qdinfo tutorial](qdinfo-tutorial.md)

Related manuals:

* [qdscript](qdscript.md)
* [qdinfo](qdinfo.md)
* [qdset](qdset.md)
* [qdcrop](qdcrop.md)

See also:

* [Executing script](executing-script.md)

Table of contents:

* [Design the Index](#design-the-index)
* [Create Script](#create-script)
* [Create Production Scripts](#create-production-scripts)
* [Put Execution into a Routine](#put-execution-into-a-routine)

## Design the Index

First, we need to define the index we are creating. In most cases, this part is done by meteorologists. If the script is ready, one can step directly to creating production scripts. Here anyway, the very simple index is designed from the scratch to demonstrate how to investigate required information from the data.

Let's assume we want to create a simple sailing index. The index gets values from one to three, where one is poor and three is superior. Let's also assume for simplicity that only wind speed and temperature affects to the joy of sailing. In temperature more is better but wind is good for sailing only on some range.

Following kind of rules could be made:

* To achieve the best rating, wind needs to be between five and ten meters per second and temperature over 15 degrees.
* The second rating is achieved if
  * wind is between three and five or between ten and 12 and temperature is over 15 degrees.
  * wind is between five and ten and temperature is under 15 degree.
* Otherwise the lowest rating is achieved.

## Create Script

Ok, we have some rules. To create a script, we need to know temperature and wind speed parameter numbers. If, dwd data is used, they can be investigated with following command:

    qdinfo -P -q /smartmet/data/dwd/europe/surface/querydata/

Relevant parts of the output are:

    The parameters stored in the querydata are:
    Number  Name                    Description
    ======  ====                    ===========
    1       Pressure                                Pressure
    4   Temperature             T
    21  -WindSpeedMS                Wind speed

From where we can see that parameter number of temperature is 4 and number of wind speed is 21.

We also need some parameter where to save the index. Unfortunately qdscript is unable to save parameter directly to the new name, so we have to select one which is already in the data. So we select pressure with number 1 as a save place. The parameter will be changed to a new name later.

From the rules and with that information we can create the script which looks like:

    IF(PAR4 >= 15)
    {
      IF(PAR21 >= 5)
      {
        IF(PAR21 <= 10)
        {
          PAR1 = 3
        }
      }
      ELSE
      {
        PAR1 = 1
      }
      IF(PAR21 >= 3)
      {
        IF(PAR21 < 5)
        {
          PAR1 = 2
        }
        ELSE
        {
          PAR1 = 1
        }
      }
      ELSE
      {
        PAR1 = 1
      }
      IF(PAR21 > 10)
      {
        IF(PAR21 <= 12)
        {
          PAR1 = 2
        }
        ELSE
        {
          PAR1 = 1
        }
      }
      ELSE
      {
        PAR1 = 1
      }
    }
    ELSE
    {
      IF(PAR21 >= 5)
      {
        IF(PAR21 <= 10)
        {
          PAR1 = 2
        }
        ELSE  -print \
        {
          PAR1 = 1
        }
      }
      ELSE
      {
        PAR1 = 1
      }
    }

The script can tested with Smartmet-editor or with qdscript-program. With qdscript, the command is:

    qdscript -d /smartmet/cnf/dictionary_en.conf sailingindex.st \
     < /smartmet/data/dwd/europe/surface/querydata/201110261200_dwd_europe_surface.sqd > output.sqd

After that for example qdpoint return index for Riga (with parameter Pressure, since we saved the index into that parameter):

    qdpoint -p Riga -P WindSpeedMS,Temperature,Pressure -q output.sqd

Output:

    201110261500 3.3 8.3 1.0
    201110261800 3.2 5.6 1.0
    [...]
    201110290300 2.6 1.2 1.0
    201110291500 1.8 8.3 1.0

As can be seen, it has been better weather to write tutorial than to sail. Again, Smartmet-editor is a very handy tool to check the results.

## Create Production Scripts

If the index defined here would be real usable product, a correct place to put the production would be

    /smartmet/run/data/sailingindex

and the output would be stored to

    /smartmet/data/lvgmc/europe/sailingindex/querydata.

Here, there's no sense to mess up the production since this is just an example. The production will be made to:

    /smartmet/run/products/example_qdscripts

The directory will contain two subdirectory:

* **bin**  
    for executables
* **cnf**  
    for smarttoolfilter

Output is saved here to tmp-direcotry:

    /smartmet/tmp/products/example_qdscripts

The running script /smartmet/run/products/example_qdscripts/bin/update_sailingindex looks like following:

    #!/bin/sh
    STARTTIME=`date`
 
    # log information
    LOGDIR=/smartmet/logs/products/example_dqscripts
    LOGFILE=$LOGDIR/update_sailingindex.log
    # make sure that log directory exists
    mkdir -p $LOGDIR
 
    # function to print error, this function is called if some error occurs during the executiona
    function print_error
    {
      echo SCRIPT FAILURE
      echo ==============
      echo Script name: $0
      echo Output stored to $LOGFILE:
      echo
      cat $LOGFILE
      exit 1
    }
     
    # start trapping errors with codes 1 2 3 4 5 6 7 8 10 11 15
    trap print_error 1 2 3 4 5 6 7 8 10 11 15
    (
     
        # if any statement in this return none true, exit with an error
        # (trapping will catch the exit)
        set -e
         # display commands and their arguments
        set -x
 
        # define output directory
        OUTDIR=/smartmet/tmp/products/example_qdscripts
        # define final output file
        OUTFILE=$OUTDIR/sailingindex.sqd
        # where qdscript script is
        CNFDIR=/smartmet/run/products/example_qdscripts/cnf
        # which data to use
        DATADIR=/smartmet/data/dwd/europe/surface/querydata
 
        # make sure output directory exists
        mkdir -p $OUTDIR
 
        # find the newest file in directory and execute qdscript to it,
        # note that qdscript does not find the newest file by itself like
        # qdpoint and qdcontour does
        for FILE in `ls -tr1 $DATADIR | tail -1`
        do
        # execute the qdscript program (actual calculation)
        qdscript -d /smartmet/cnf/dictionary_en.conf $CNFDIR/sailingindex.st < $DATADIR/$FILE > $OUTDIR/$FILE
        # index is saved with parameter pressure, crop it out, crop
        # also temperature and wind speed where from the index is
        # calculated
        qdcrop -p Pressure,Temperature,WindSpeedMS $OUTDIR/$FILE $OUTFILE
        # change parameter to number 999 and name it sailing index
        qdset -d 999 -n SailingIndex $OUTFILE Pressure
        # clean unnecessary tmp-file
        rm $OUTDIR/$FILE
          done
 
        # log information
        ENDTIME=`date`
        echo $STARTTIME - $ENDTIME >> $LOGFILE
    # end trapping errors and direct output to log file
    ) >$LOGFILE 2>&1
    # if exit code of trapping block is something else than 0 (success), call print error function
    if
        [[ $? != 0 ]]; then
        print_error
    fi

The code comments explains it quite comprehensively, but three actual program call deserves some more words:

    qdscript -d /smartmet/cnf/dictionary_en.conf $CNFDIR/sailingindex.st < $DATADIR/$FILE > $OUTDIR/$FILE
    qdcrop -p Pressure,Temperature,WindSpeedMS $OUTDIR/$FILE $OUTFILE
    qdset -d 999 -n SailingIndex $OUTFILE Pressure

First actual qdscript command is ran. The output is saved to output directory (here tmp directory) with a same name as a actual data file.

The script saves sailing index as a parameter pressure. The required parameters pressure, temperature and wind speed are cropped from the data with a qdcrop program. It's a simple tool for cropping certain parameters, times etc. from the data. Consult qdcrop manual for more information.

Last, pressure is not maybe the best parameter name for the data. It can be changed with a qdset program. It's a simple tool for changing certain parameters, producers etc. in the data. Consult qdset manual for more information. Here the parameter is changed to number 999 and name is changed to SailingIndex. After that qdinfo shows following:

    qdinfo -p -q /smartmet/tmp/products/example_qdscripts/sailingindex.sqd
 
    The parameters stored in the querydata are:
 
    Number  Name                    Description
    ======  ====                    ===========
    999                     SailingIndex
    4   Temperature             T
    21  WindSpeedMS             Wind speed
 
    There are 3 stored parameters in total

As can be seen, parameter number 999 exists and description is correct. Unfortunately parameter name does not exist since qdtools can handle by name only predefined parameters. Those parameters can be listed with qdinfo -A -q /smartmet/tmp/products/example_qdscripts/sailingindex.sqd command.

Due to that shortage, the data need to be queried with parameter number:

    dpoint -p Riga -P Temperature,WindSpeedMS,999 -q /smartmet/tmp/products/example_qdscripts/sailingindex.sqd

Output:

    201110280300 1.8 2.2 1.0
    201110280600 2.3 2.5 1.0
    [...]
    201110301500 10.0 3.3 1.0
    201110310300 9.2 3.6 1.0

## Put Execution into a Routine

Ok, everything is fine and it is time to add the production to routine. More detailed instructions about triggering the script are detailed in map creation tutorial.

dwd data from directory /smartmet/data/dwd/europe/surface/querydata is used here, so trigger:

    /smartmet/cnf/triggers.d/lazy/smartmet:data:dwd:europe:surface:querydata

need to be added with following content:

    #!/bin/sh
 
    # Run example animations
    /smartmet/run/products/example_qdscripts/bin/update_sailingindex

Note that it's also important to give trigger execution rights:

    chmod 775 smartmet:data:dwd:europe:surface:querydata

This part is not actually done to the server when writing this tutorial.
