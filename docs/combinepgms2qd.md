

combinepgms2qd converts several RADAR formatted pgm files into single querydata, purpose is to make 3D queryDatas from radar slices

### Usage

    combinepgms2qd [options] inputfilepattern qdfileout

### Options

* **-h**  
    print usage information
* **-v**  
    verbose mode
* **-p int,name**  
    set producer id and name (default = 1014,NRD)
* **-t timestepcount**  
    how many timesteps in result data (default = 0 (= all possible))

