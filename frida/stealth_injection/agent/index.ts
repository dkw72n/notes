import { log } from "./logger";

const target_so = "/data/local/tmp/libdemo.so"

const pread_addr = Module.getExportByName(null, "pread");
const pread = new SystemFunction(pread_addr, 'int32', ['int', 'pointer', 'uint32', 'uint32'])

type Nullable<T> = T | null;

var _dlopen: Nullable<NativeFunction> = null;
var _dlclose: Nullable<NativeFunction> = null;
var _mmap64_listener: Nullable<InvocationListener> = null;
var _open_listener: Nullable<InvocationListener> = null;
var target_fd = -1
var p_g_ld_debug_verbosity: NativePointer = new NativePointer(0);

Process.getModuleByName("linker")
    .enumerateSymbols()
    .forEach((exp, index) => {
        // log(`export ${index}: ${exp.name}`);
        if (exp.name == '__dl_g_ld_debug_verbosity'){
            p_g_ld_debug_verbosity = exp.address;
        }

        if (exp.type != 'function'){
            return;
        }
        if (exp.name == '__dl_mmap64') {
            const data = Memory.alloc(64)
            log(`${exp.name}: ${exp.address} data=${data}`)
            _mmap64_listener = Interceptor.attach(exp.address, {
                onEnter(args){
                    this.fd = args[4].toInt32();
                    this.offset = args[6].toInt32();
                    this.count = args[1].toInt32();
                    this.prot = args[2].toUInt32();
                    log(`${this.fd} ${this.offset} ${this.count} prot=${this.prot}`)
                    
                    if (this.fd == target_fd){
                        let flags = args[3].toUInt32();
                        flags |= 0x20; // MAP_ANONYMOUS
                        args[3] = new NativePointer(flags);
                        let prot = args[2].toUInt32();
                        prot |= 0x2; // PROT_WRITE
                        args[2] = new NativePointer(prot);;
                    }
                },
                onLeave(ret){
                    // log(`mmap(${this.fd} ${this.offset} ${this.count})=>${ret}`)
                    if (this.fd == target_fd){
                        let byte_read = pread(this.fd, ret, this.count, this.offset)
                        let x = <any>byte_read;
                        let err = '';
                        if (x.value == -1){
                            err = ` errno:${x.errno}`
                        }
                        log(`pread ${x.value} bytes, this.count=${this.count}` + err)
                    }
                }
            });
            return;
        }
        if (exp.name == '__dl_open64') {
            log(`${exp.name}: ${exp.address}`)
            _open_listener = Interceptor.attach(exp.address, {
                onEnter(args){
                    this.fn = args[0].readUtf8String();
                },
                onLeave(ret){
                    log(`open(${this.fn}) => ${ret}`);
                    if (this.fn == target_so){
                        target_fd = ret.toInt32();
                    }
                }
            });
            return;
        }
        if (exp.name.indexOf("__dl_dlopen") != -1){
            log(`${exp.name}: ${exp.address}`)
            _dlopen = new NativeFunction(exp.address, 'pointer', ['pointer', 'int']);
            return;
        }
        if (exp.name.indexOf("__dl_dlclose") != -1){
            log(`${exp.name}: ${exp.address}`);
            _dlclose = new NativeFunction(exp.address, 'void', ['pointer']);
            return;
        }
    });



if (_dlopen && _dlclose && _mmap64_listener && _open_listener && p_g_ld_debug_verbosity){
    p_g_ld_debug_verbosity.writeInt(2);
    let soinfo = (<NativeFunction>_dlopen)(Memory.allocUtf8String(target_so), 2);
    p_g_ld_debug_verbosity.writeInt(0);
    log(`soinfo: ${soinfo}`);
}


