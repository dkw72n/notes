Object.defineProperty(global, "AndroiCallStack", {
    value: (()=>{
const libutilscallstack = Process.getModuleByName("libutilscallstack.so")

function MakePrinter(fn) {
    const _ptr = Memory.alloc(32);
    const PrinterVtbl = Memory.alloc(32);
    _ptr.writePointer(PrinterVtbl)
    const cb = new NativeCallback((_self, cstr)=>{
        fn(Memory.readCString(cstr))
    }, 'void', ['pointer', 'pointer'])
    PrinterVtbl.writePointer(cb);
    PrinterVtbl.add(8).writePointer(cb);
    PrinterVtbl.add(16).writePointer(cb);
    PrinterVtbl.add(24).writePointer(cb);
    // console.log(PrinterVtbl);
    return {
        instance: _ptr,
        _vtbl: PrinterVtbl,
        _printfunc: cb
    }
}

const CallStack = (()=>{
    const _raw_ptr = {
      _ctor: libutilscallstack.getExportByName("_ZN7android9CallStackC1Ev"),
      _dtor: libutilscallstack.getExportByName("_ZN7android9CallStackD1Ev"),
      _update: libutilscallstack.getExportByName("_ZN7android9CallStack6updateEii"),
      _log: libutilscallstack.getExportByName("_ZNK7android9CallStack3logEPKc19android_LogPriorityS2_"),
      _print: libutilscallstack.getExportByName("_ZNK7android9CallStack5printERNS_7PrinterE")
    }
    var ctor = new NativeFunction(_raw_ptr._ctor, 'void', ['pointer']);
    var dtor = new NativeFunction(_raw_ptr._dtor, 'void', ['pointer']);
    var update = new NativeFunction(_raw_ptr._update, 'void', ['pointer', 'int', 'int'])
    var log = new NativeFunction(_raw_ptr._log, 'void', ['pointer', 'pointer', 'int', 'pointer']);
      // cb = ptr(0x709394)
    // console.log(MyPrinter, MyPrinter.readPointer(), MyPrinter.readPointer().readPointer())
  
    var print = new NativeFunction(_raw_ptr._print, 'void', ['pointer', 'pointer'])
    var holder = Memory.alloc(256);
    var tag = Memory.allocUtf8String("frida")
    ctor(holder)
    var arr = [];
    var DumpPrinter = MakePrinter(s=>{arr.push(s)})
    return {
        _obj: holder,
        _tag: tag,
        dump: function (tid) {
          if (!tid) tid = -1;
          arr.length = 0;
          update(this._obj, 0, tid)
          print(this._obj, DumpPrinter.instance)
          return [...arr]
        },
        deinit: function(){
          dtor(this._obj)
          this._obj = ptr(0)
          // console.log("bye!")
        }
      }
    })()

Script.bindWeak(CallStack, x=>{x.deinit()})
function install() {
    // console.log("in callstack_patch")
    Backtracer.ANDROID = 3
    const f = Thread.backtrace
    function my_backtrace(context, backtracer){
        if (backtracer == Backtracer.ANDROID){
            return CallStack.dump(context.id)
        }
        else {
            return f(context, backtracer)
        }
    }
    Thread.backtrace = my_backtrace
}

  return {
  capture: function(tid){
    return CallStack.dump(tid);
  }
}
})(),
    configurable: true,
    enumerable: true,
});

