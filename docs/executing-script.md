Scripts can be executed routinely in two ways. They can be scheduled in cron or they can be triggered whenever new data arrives.

* [Scheduled scripts](#scheduled-scripts)
  * [Cron](#cron)
  * [/etc/crontab](#etccrontab)
  * [/data/conf/cron/cron.d](#dataconfcroncrond)
* [Triggered scripts](#triggered-scripts)
  * [What are triggers?](#what-are-triggers)
  * [Runtime algorithm](#runtime-algorithm)
  * [Trigger scripts](#trigger-scripts)
  * [Logfiles](#logfiles)
    * [/smartmet/logs/triggers/logs](#smartmetlogstriggerslogs)
    * [/smartmet/logs/triggers/output](#smartmetlogstriggersoutput)
    * [/smartmet/logs/triggers/trigger_output](#smartmetlogstriggerstrigger_output)
    * [/smartmet/logs/triggers/checks](#smartmetlogstriggerschecks)

## Scheduled scripts

### Cron

The root user runs a system called cron to execute scheduled tasks (The name derives from the Greek Chronos meaning time). Basic information on cron can be obtained by typingman cron on the command line or from Wikipedia.

### /etc/crontab

The essential part of the cron-system is the root-owned file /etc/crontab
The file begins with this block of text:

    01 * * * * root run-parts /etc/cron.hourly
    02 4 * * * root run-parts /etc/cron.daily
    22 4 * * 0 root run-parts /etc/cron.weekly
    42 4 1 * * root run-parts /etc/cron.monthly

This essentially means that all scripts located in directory /etc/cron.hourly are run every hour at XX:01 local time, every script in directory /etc/cron.daily at 04:02 local time every day and so on.

The directories are reserved only for scripts which are to be run as root. Since running scripts as root is potentially very dangerous, production scripts should **not** be placed there.

For safety reason an extra set of directories is used for scripts which are to be run as the www-user. This is accomplished by adding a new block as root to the file /etc/crontab as follows

    # Run as www-user
    */10 * * * * www run-parts /smartmet/cnf/cron/cron.10min
    01 * * * * www run-parts /smartmet/cnf/cron/cron.hourly
    00 0 * * * www run-parts /smartmet/cnf/cron/cron.daily
    02 2 * * 0 www run-parts /smartmet/cnf/cron/cron.weekly
    02 3 1 * * www run-parts /smartmet/cnf/cron/cron.monthly

Note that we have added a 10-minute interval directory for scripts which must be run very frequently.

Creating a scheduled script is thus often as simple as just dropping a script into one of the subdirectories in /smartmet/cnf/cron

### /data/conf/cron/cron.d

If the simple /etc/crontab time is not suitable for some production run, we can drop a production specification in a file into directory /smartmet/cnf/cron/cron.d. The syntax of the files is the same as in /etc/crontab except that the user part is omitted - the www-user is automatically invoked by root.

For example, suppose you need to send a forecast to a radio station every day automatically a bit before the broadcast, say at 14:15. This can be accomplished by dropping a file with the line

    15 14 * * * /smartmet/run/products/client/bin/send_text_forecast

into the cron.d directory. Here /smartmet/run/producs/client/bin/send_text_forecast is the script doing all the actual work of finding the forecast and sending it to the client.

## Triggered scripts

### What are triggers?

Triggers are scripts, which are executed when a file or the latest file in a directory is changed. This is not a native feature of Linux, but a system built at the Finnish Meteorological Institute.

In a typical scenario we have a directory containing the most recent forecasts from a numerical model. We usually timestamp the files like 200601010000_gfs_world_surface.sqd

Every time a new forecast arrives, it is copied to this directory containing the latest forecasts. If the files are compared according to their modification times, the newest forecast will have the latest modification time unless someone messes up with the directory manually. Hence, in this scenario, the appearance of a new file in the directory indicates the arrival of a new numerical forecast, which must then be processed.

Such processing cannot be done based on a scheduler like cron, since there is a strong possibility numerical forecasts are delayed due to problems in the simulation run. Hence an explicit check on the actual arrival of the file is needed to trigger post processing.

### Runtime algorithm

This is basically what happens in the system:

1. Root cron checks every minute whether the trigger needs to be run. This is accomplished by these lines in /etc/crontab:
    
    `* * * * * www /smartmet/bin/run-triggers-quick > /dev/null 2>&1`  
    `* * * * * www /smartmet/bin/run-triggers-lazy > /dev/null 2>&1`

Note that we have two kinds of triggers, quick ones and lazy ones. The only difference between them is that the execution of lazy triggers is delayed if the system load is high. The delay is accomplished simply by trying again the next minute.

2. The run-triggers-* scripts check - the lazy one only if the load is low enough - the contents of directories:
    
    `/smartmet/cnf/triggers.d/quick`  
    `/smartmet/cnf/triggers.d/lazy`

The directories contain executable files, which are named like:

    smartmet:data:ecmwf:europe:surface:querydata

The file names correspond the following directories

    /smartmet/data/ecmwf/europe/surface/querydata

The filenames could also refer to plain files in the system, not just directories.

3. For each trigger the run-triggers-* script checks whether the file corresponding to the trigger or the newest file in the corresponding directory has changed. If so, the trigger script is executed on the background, and run-triggers-* moves on to check the next trigger.

The process is repeated every minute. The triggering script remembers the status of the previous run in order to minimize the hard drive scans necessary to investigate whether for example a directory has changed.

### Trigger scripts

The trigger scripts can basically contain everything. However, in time as more and more production is started from a single trigger, it makes sense to write the trigger in a structured manner.

The recommended way to structure the scripts is like this:

    #\!/bin/sh
    /smartmet/run/products/client1/bin/update_client1_products
    /smartmet/run/products/client2/bin/update_client2_products
    /smartmet/run/products/client3/bin/update_client3_products
    /smartmet/run/products/client4/bin/update_client4_products

That is, we update client products by separate scripts for each client. If some script is particularly heavy, it may make sense to start it in the background as follows so as not to delay the products of other clients:

    #\!/bin/sh
    /smartmet/run/products/client1/bin/update_client1_products &
    /smartmet/run/products/client2/bin/update_client2_products &
    /smartmet/run/products/client3/bin/update_client3_products
    /smartmet/run/products/client4/bin/update_client4_products

Here client 1 and client 2 are updated in parallel on the background, while execution proceeds in parallel to client 3, but not yet client 4.

Note that the scripts must be executable. If you create a new trigger, make sure it is executable with chmod ug+x triggerfile

### Logfiles

The scripts make logfiles of what has been done. The files are stored in /smartmet/logs/triggers
subdirectories.

#### /smartmet/logs/triggers/logs

This directory contains execution time logs for the triggers. The files will grow in time, and it may at some time in the future be required to shrink or delete some logs

#### /smartmet/logs/triggers/output

The output from the last run of the run-triggers-* commands. Usually there is little information in these output files.

#### /smartmet/logs/triggers/trigger_output

These files contain the output from the last run of the actual trigger, and hence may be useful when tracking down problems.

#### /smartmet/logs/triggers/checks

These files are informative only via the modification times, they tell when the trigger was last started.
