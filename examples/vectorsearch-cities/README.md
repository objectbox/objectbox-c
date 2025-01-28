VectorSearch-Cities ObjectBox C++ Example
=========================================

This example demonstrates vector search in ObjectBox using capital cities.
You can search cities according to their geolocations (latitude and longitude),
e.g. find the closest cities to a given city or a given geolocation.
It uses a simple command-line interface; see below for some example queries.

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

Search nearest neighbors to Berlin (note that "Score" is the distance in kilometers):

```
name Berlin, 10
 ID  Name                Location            Score     
 28  Berlin              52.52     13.40      0.00
147  Prague              50.08     14.44     281.13
 49  Copenhagen          55.68     12.57     355.15
203  Warsaw              52.23     21.01     517.17
200  Vienna              48.21     16.37     523.55
 34  Bratislava          48.15     17.11     552.40
  6  Amsterdam           52.37     4.89      576.75
 94  Luxembourg City     49.61     6.13      601.97
 37  Brussels            50.85     4.35      650.65
196  Vaduz               47.14     9.52      659.56
```

Search nearest neighbors to Area 51 (note that "Score" is the distance in kilometers):

```
geo 37.23, -115.80
 ID  Name                Location            Score     
107  Mexico City         19.43     -99.13    2555.95
204  Washington D.C.     38.91     -77.04    3372.90
129  Ottawa              45.42     -75.70    3431.69
 27  Belmopan            17.25     -88.76    3452.91
 64  Guatemala City      14.63     -90.51    3542.07
```
