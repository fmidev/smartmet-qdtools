The qdstat program calculates statistics on querydata values. Useful when inspecting suspicious data.

NOTE! Some parametres cannot be checked with `qdstat` or are checked wrong. For example parameter WindVectorMS is reported to have very strange values, when in fact it can be correct in the querydata. 

## Command line

The command line syntax is

    qdstat [options] querydata
    qdstat -i querydata [options]
    qdstat [options] < querydata
    cat querydata | qdstat [options]

The available options are

* **-h** [ --help ]  
print out help message  
* **-V** [ --version ]  
display version number  
* **-i** [ --infile ] arg  
input querydata  
* **-T** [ --alltimes ]  
for all times  
* **-W** [ --allstations ]  
for all stations  
* **-r** [ --percentages ]  
print percentages instead of counts  
* **-d** [ --distribution ]  
print distribution of values  
* **-I** [ --ignore ] arg  
ignore this value in statistics  
* **-b** [ --bins ] arg  
max number of bins in the distribution  
* **-B** [ --barsize ] arg  
width of the bar distribution  
* **-t** [ --times ] arg  
times to process  
* **-p** [ --parameters ] arg  
parameters to process  
* **-w** [ --stations ] arg  
stations to process  

