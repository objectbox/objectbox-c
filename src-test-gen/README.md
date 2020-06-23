### Using `objectbox-cgen` and generated structs
This directory shows one of the variants of working with ObjectBox from plain C99/C11 - by using `objectbox-generator -c` 
to generate model, struct and binding files providing a simplified way to read and write objects. 

If you'd like to recreate the generated files, you'll need the `objectbox-generator` binary installed/available.  
```shell script
objectbox-generator -version
objectbox-generator -c c_test.fbs
```
