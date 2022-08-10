cleaner is used to clean directories based on time stamps in filenames.

Table of Contents:

* [Introduction](#introduction)
* [Command line](#command-line)
* [Examples](#examples)

## Introduction

It is common in Unix to clean up old files with the find command like this

    find dir -type f -mtime +7 -exec rm {} \;

However, in animation we often produce filenames of the form
    
    image_YYYYMMDDHHMI_YYMMDDHHMI.png

where the first timestamp is the valid time of the image, and the second the time the image was created. Hence there can be multiple images for the same moment in time from different forecasts. Usually we wish to delete the older ones away as unnecessary. However, the age of the older forecast may be only a couple hours, and using the find command to find such "aged" files is not feasible.

cleaner is a tool specifically created for the task of deleting files which have aged in one way or another.

## Command line

The command line syntax is

    cleaner [options] pattern directory [directory2 directory3 _. ]

The pattern is a Perl regular expression which is required to match the filename before it can be deleted. For example the pattern

    '\.sqd$'

matches all files which end in the string ".sqd". Note that in general the "." character matches any character, hence we have escaped it with the "\\\" character to make the match explicit. The "$" character is a special pattern character which matches the end of the line.

The available options are:

* **-maxmtime hours**  
    the maximum allowed age since the file was modified last
* **-maxage hours**  
    the maximum allowe age based on the timestamp in the filename
* **-minfiles count**  
    the minimum number of files to always be preserved
* **-maxfiles count**  
    the maximum number of files to be kept
* **-dup**  
    keep only the newest one of files with the same timestamp in the filename
* **-v** or **-verbose**  
    print information on the progress
* **-debug**  
    do not actually delete anything, just show what would be done
* **-norecurse**  
    do not recurse deeper into subdirectories
* **-stamppos count**  
    which timestamp in the filename is the significant one (default is the first)
* **-shortstamp**  
    normally the timestamp takes 12 characters, with this option the cleaner permits the minutes to be missing from the end making the timestamp only 10 characters long
* **-utc**  
    the timestamps in the filenames are in UTC (this is the default)
* **-local**  
    the timestamps in the filenames are in local time

## Examples

If we wish to keep only the 4 latest GFS forecasts, we can delete the older ones with

    cleaner -maxfiles 4 '\.sqd$' /smartmet/data/gfs/europe/surface/querydata

Alternatively, if we wish to keep only forecasts less than 48 hours old we can use

    cleaner -maxmtime 48 '\.sqd$' /smartmet/data/gfs/europe/surface/querydata

However, in case there is a problem with obtaining GFS querydata, this command might actually delete the latest available forecast. Hence it would be better to make sure atleast one forecast remains with

    cleaner -minfiles 1 -maxmtime 48 '\.sqd$' /smartmet/data/gfs/europe/surface/querydata

When creating animated forecasts, we typically have the creation time in the image filenames as shown in the introduction. To delete aged images we typically use the command like

    cleaner -maxage 48 -dup '\.png$' /smartmet/products/animation/gfs

Note the added -dup option and the use of -maxage instead of -maxmtime so that the timestamp in the filename will be the deciding factor on how "old" the image is.

Whenever creating a new script for cleaning old data, it is always a good idea to first run the cleaner command manually with the -debug option to make sure there are no dangerous typos in the pattern or elsewhere on the command line.
