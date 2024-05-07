# mLua
Lua的内存优化管理工具

## 特性
* 将Lua的内存如配置表固化到C++中，并无需修改Lua使用的代码，以减少Lua的内存占用提高Lua的GC效率（参考自云风的[lua-conf](https://github.com/cloudwu/lua-conf)）
* 分析Lua的内存占用，输出gperftools格式图片或者火焰图，分为静态分析和动态分析
* 序列化和反序列化Lua的Table

## 使用方法
#### 编译源码
```shell
# mkdir build
# cd build
# cmake ..
# make -j8
```

#### 编译tools
```shell
# cd tools
# go mod tidy
# go build png.go
# go build plua.go
```

#### Lua表固化C++
1. 执行将Lua的内存如配置表固化到C++中的测试脚本
```shell
# cd test
# lua test_cpp_table.lua
```
2. 查看测试结果，可以看到Lua的内存占用从161479降到了34，即便在使用C++的table后，Lua的内存占用有所回升，但也停留在1150
```shell
...
before lua table to cpp, lua memory is 161479.23925781
...
after lua table to cpp, lua memory is 34.1533203125
...
after use cpp table, lua memory is 1150.0283203125
```
#### 静态分析Lua的内存占用
1. 执行静态分析的测试脚本
```shell
# cd test
# lua test_static_perf.lua
```
2. 使用tools的工具，生成gperftools风格的图片和火焰图
```shell
# ./show.sh ../test/
```
3. 查看生成的图片
![image](test/static_mem.png)

#### 动态分析Lua的内存占用
1. 执行分析Lua全局内存分布的测试
```shell
# cd test
# lua test_dynamic_perf.lua
```
2. 使用tools的工具，生成gperftools风格的图片和火焰图
```shell
# ./show.sh ../test/
```
3. 查看生成的图片
   ![image](test/dynamic_mem.png)

#### 序列化和反序列化Lua的Table
1. 执行序列化和反序列化Lua的Table的测试
```shell
# cd test
# lua test_quick_archiver.lua
```
2. 查看结果。可以看到序列化和反序列化的结果是一致的，并且对于内存的占用也是很小的
```shell
save old data len:      441
is equal: true
init lua mem KB:        86.9228515625
after init data, lua mem KB:    1716.734375
save data len:  177358
after save data, lua mem KB:    379.5
after load data, lua mem KB:    1488.984375
```

## 其他
[lua全家桶](https://github.com/esrrhs/lua-family-bucket)
