# C++ Example App: VectorSearch-Cities

This is a sample command-line C++ application that
illustrates conducting vector-search query operations in ObjectBox. 

## Prerequisites

- Download ObjectBox

## Build

```
cmake -S . -B build
cmake --build build
```

## Run

```
cd build
./objectbox-c-examples-vectorsearch-cities
```

```
Welcome to the ObjectBox vectorsearch-cities app example
Available commands are: 
    import <filepath>          import cities database
    ls [<prefix>]              list cities (with common <prefix> if set)
    name <city>[,<n>]          search <n> nearest neighbor cities to <city> (<n> defaults to 5) 
    geo <lat>,<long>[,<n>]     search <n> nearest neighbor cities to geo-location (<n> defaults to 5)
    add <city>,<lat>,<long>    add location
    exit                       close the program
    help                       display this help
```

If you start this example for the first time, import some capital cities:

```
import ../cities.csv
Imported 211 entries from ../cities.csv
```

List all entries:

```
ls
[..]
207  Willemstad          12.11     -68.93   
208  Windhoek            -22.57    17.08    
209  Yamoussoukro        6.83      -5.29    
210  Yaoundé            3.85      11.50    
211  Yaren               -0.55     166.92   
212  Yerevan             40.19     44.52    
```

Search nearest neighbors to Berlin:

```
name Berlin, 10
 ID  Name                Location            Score     
 28  Berlin              52.52     13.40      0.00
147  Prague              50.08     14.44      7.04
 49  Copenhagen          55.68     12.57     10.66
200  Vienna              48.21     16.37     27.41
 34  Bratislava          48.15     17.11     32.82
 89  Ljubljana           46.06     14.51     42.98
196  Vaduz               47.14     9.52      44.02
 39  Budapest            47.50     19.04     56.98
203  Warsaw              52.23     21.01     57.95
 94  Luxembourg City     49.61     6.13      61.36
```

Search nearest neighbors to Area 51:

```
geo 37.23, -115.80
 ID  Name                Location            Score     
107  Mexico City         19.43     -99.13    594.53
 27  Belmopan            17.25     -88.76    1130.38
 64  Guatemala City      14.63     -90.51    1150.28
164  San Salvador        13.69     -89.22    1260.59
 67  Havana              23.11     -82.37    1317.07
```
