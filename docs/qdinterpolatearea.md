qdinterpolatearea interpolates data to desired projection and grid.

See also: [Projection descriptions in Smartmet](projection-descriptions-in-SmartMet.md)

Usage

    qdinterpolatearea [options] controlGridFile < inputData > outputData

Options

* **-s <Columns_x_Rows>**  
    Wanted grid size, default 3 x 3
* **-p** **<****projection****>**  
    Wanted projection, if given, overrides controlGridFile (fileName not given then).
* **-i** **<****inputfile****>**  
    Default is to read standard input.
* **-o** **<****outputfile****>**  
    Default is to write to standard output.

Example uses

    qdinterpolatearea myGrid.dat < myData.sqd > newData.sqd    


    qdinterpolatearea -p stereographic,10,90,60:-19.711,25.01,62.93,62.845 -s 40x50  < myData.sqd > newData.sqd  


    qdinterpolatearea -p stereographic,20,90,60:6,51.3,49,70.2 -s 63x70  < myData.sqd > newData.sqd


    qdinterpolatearea -p stereographic,20,90,60:6,51.3,49,70.2 -s 20x20km -i myData.sqd -o newData.sqd    


    qdinterpolatearea -p stereographic,20,90,60:6,51.3,49,70.2:20x20km -i myData.sqd -o newData.sqd  
 
