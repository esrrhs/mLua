# mLua
Lua的内存优化管理工具

## 特性
* 将Lua的内存如配置表固化到C++中，并无需修改Lua使用的代码，以减少Lua的内存占用提高Lua的GC效率（参考自云风的[lua-conf](https://github.com/cloudwu/lua-conf)）
* 分析Lua Table的内存占用，输出gperftools格式图片或者火焰图

## 使用方法
1. 编译源码
```shell
# mkdir build
# cd build
# cmake ..
# make -j8
```
2. 执行将Lua的内存如配置表固化到C++中的测试
```shell
# cd test
# lua test_cpp_table.lua
```
3. 查看测试结果，可以看到Lua的内存占用从161479降到了34，即便在使用C++的table后，Lua的内存占用有所回升，但也停留在1150
```shell
...
before lua table to cpp, lua memory is 161479.23925781
...
after lua table to cpp, lua memory is 34.1533203125
...
after use cpp table, lua memory is 1150.0283203125
```
4. 执行分析Lua Table的内存占用的测试
```shell
# cd test
# lua test_perf.lua
```
5. 使用[pLua](https://github.com/esrrhs/pLua)的工具，生成gperftools风格的图片和火焰图
```shell
# cd pLua/tools
# ./show.sh <mLua test path>
```
6. 查看生成的图片
![image](test/mem.png)

## 其他
[lua全家桶](https://github.com/esrrhs/lua-family-bucket)
