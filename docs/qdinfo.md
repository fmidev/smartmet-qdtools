The qdinfo program prints out information on the given querydata. The user can either select the subset of information to be printed,
such as the locations in the querydata, or print all of them.

qdinfo is not usually used in production scripts, but it can be provided the output is processed using a programming language
to extract the desired words from the output.

See also

* [qdinfo-tutorial](qdinfo-tutorial.md)
* [Projection descriptions in Smartmet](projection-descriptions-in-SmartMet.md)
* [Querydata producer names](querydata-producer-names.md)

## Command line

The command line syntax is 
 
     qdinfo [options]

Note in particular that the querydata filename is not a compulsory parameter. This is due to option -l which prints out all known parameter names, and does not need any querydata to do it. For all other options one must also give the name of the querydata with option -q

The available options are

* **-q filename**  
The querydata to be inspected. If a directory is given, the newest file is inspected.  
* **-A**  
all the options below combined  
* **-l**  
list all known parameter names  
* **-p**  
list all parameters in the querydata  
* **-T**  
show querydata time information in UTC time  
* **-a**  
all the options below combined  
* **-v**  
show querydata version number  
* **-P**  
show all parameters in the querydata, including subparameters  
* **-t**  
show querydata times in local time  
* **-x**  
show querydata projection information  
* **-X**  
show querydata location information  
* **-z**  
show querydata level information  
* **-r**  
show querydata producer information  
* **-M**  
show all metadata information  
* **-m name**  
show metadata for the given name  

