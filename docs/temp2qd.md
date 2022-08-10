Program reads all the files files given as arguments and tries to find TEMP sounding messages from them. All the found soundings are combined in one output querydata file.

### Usage

    temp2qd fileFilter1[,fileFilter2,...] > output

### Options

* **-s stationInfoFile**  
    File that contains different sounding station infos.
* **-p <1005,UAIR>**  
    Make result datas producer id and name as wanted.
* **-t**  
    Put sounding times to nearest synoptic times.

Note that fileFilter should have quotes around it. Also TTAA and TTBB should be in order (no other station between them).
