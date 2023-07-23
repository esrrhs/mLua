# mLua
Lua的内存优化管理工具

## 特性
* 将Lua的内存固化到C++中，例如配置表，以减少Lua的内存占用，提高Lua的GC效率

## 使用方法
1. 编译源码
```shell
# mkdir build
# cd build
# cmake ..
# make -j8
```
2. 执行测试
```shell
# ./test
```
3. 查看测试结果，可以看到Lua的内存占用从16164降到了29，即便在使用C++的table后，Lua的内存占用有所回升，但也停留在124
```shell
...
before lua table to cpp, lua memory is 16164.296875
...
after lua table to cpp, lua memory is 29.7578125
...
after use cpp table, lua memory is 124.46875
```