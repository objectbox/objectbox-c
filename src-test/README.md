### Using `flatcc` and direct flatbuffers access
This directory shows one of the variants of working with ObjectBox from plain C99/C11 - by using `flatcc` generated
 files and direct flatbuffers access to read and write objects.

If you'd like to recreate the generated files, you'll need the [flatcc](https://github.com/dvidelabs/flatcc) binary installed/available.  
```shell script
flatcc --version
flatcc --common --builder c_test.fbs
```
