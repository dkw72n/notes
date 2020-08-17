# il2cpp 假引用问题分析

### 对象创建

il2cpp 有三种对象分配流程: `NewPtrFree`, `AllocateSpec`, `Allocate`
```cpp
// External/il2cpp/il2cpp/libil2cpp/vm/GCObject.cpp
Il2CppObject * Object::NewAllocSpecific(Il2CppClass *klass){
	Il2CppObject *o = NULL;
	// ... 省略部分初始化代码
	if (!klass->has_references)
	    o = NewPtrFree(klass);
	else if (klass->gc_desc != GC_NO_DESCRIPTOR)
	    o = AllocateSpec(klass->instance_size, klass);
	else
	    o = Allocate(klass->instance_size, klass);
	// ... 省略部分后处理代码
	return o;
}
```
* `NewPtrFree`
无指针类型, 表明对象中是纯数据类型, GC 时不做处理
* `AllocateSpec`
有GC描述符, GC 时明确知道指针的位置
* `Allocate`
所有指针大小对齐的内存都当作指针处理

当类型中存在引用时, 走 `AllocateSpec` 还是 `Allocate` 取决于 GC描述符.

### GC描述符生成
GC描述符通过 `SetupGCDescriptor(Il2CppClass*)` 生成
```cpp
// External/il2cpp/il2cpp/libil2cpp/vm/Class.cpp
void SetupGCDescriptor(Il2CppClass* klass)
{
    // ...省略初始化/声明代码
    size_t maxSetBit = 0;
    GetBitmapNoInit(klass, bitmap, maxSetBit, 0);
    if (klass == il2cpp_defaults.string_class)
        klass->gc_desc = il2cpp::gc::GarbageCollector::MakeDescriptorForString();
    else if (klass->rank)
        klass->gc_desc = il2cpp::gc::GarbageCollector::MakeDescriptorForArray();
    else
        klass->gc_desc = il2cpp::gc::GarbageCollector::MakeDescriptorForObject(bitmap, (int)maxSetBit + 1);
}
```
对象分三类生成描述符: 字符串, 数组, 其他对象.
字符串和数组都是返回 `GC_NO_DESCRIPTOR`, 其他对象根据布局的 `bitmap` 生成:

```cpp
// External/il2cpp/il2cpp/libil2cpp/gc/BoehmGC.cpp
void*
il2cpp::gc::GarbageCollector::MakeDescriptorForObject(size_t *bitmap, int numbits)
{
    /* It seems there are issues when the bitmap doesn't fit: play it safe */
    if (numbits >= 30)
        return GC_NO_DESCRIPTOR;
    else
        return (void*)GC_make_descriptor((GC_bitmap)bitmap, numbits);
}
```
可以看出, 位图大小大于阈值时, 也会返回 `GC_NO_DESCRIPTOR`, 此时, 对象中所有指针大小对齐的内存都当作指针处理. 

### struct 数组
上面看到, 数组也是走的 `GC_NO_DESCRIPTOR`, 最后走到 `Allocate`. 这个在 class 数组下没有问题, 因为数组内存中全是指向class的指针. 但对于 struct 数组, 内存是一个个 struct 的内容本身, 再走同样的流程, 结果就是全部struct的内容都被当做指针了.



### 总结

两种对象可以触发假引用:

1. `struct` 数组
2. 足够大的(>=120bytes) `class` 或者 `struct` 本身

