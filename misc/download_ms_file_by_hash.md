已知旧版本 windows 文件的 md5, 如何下载这个文件？

假如从某处看到这个信息

```
cmdiag.exe
File Path: C:\WINDOWS\system32\cmdiag.exe
HashesPermalink
Type	Hash
MD5	DB714A4328C60A6A17F2B9CA93D42B06
SHA1	DA598EC3526A4BFE405946E491CD0CA8552CDB43
SHA256	3C5F20B983271AF305A293690C1CC31C0DB43991E88D9E410F52AFE65B6E26D5
SHA384	3DE502687B4CD2C3681F287A9416918E272E68F907892095235522F7CFD70227312A0C43B226DD9B114DDD97547AABB0
SHA512	1303A2EC0BFF028E2EBA23F3212C0DBCE57E45CD84B2266F3E6D06E53CCE3E35D8538C1468B126F9FA4170AF15079B13407835941BB57E0112CFB138ECF92F31
SSDEEP	768:uZK3pF5beTxwhnEfDURTbL3lTqA7KYPPjvujFgakEnWMGly7WwuFFU2NA6Q2P3r8:eK1eTxD6pqAm07uQkWM2iVL2N/RrIFP9
```

用上面的hash值，在 virustotal 搜索，看到有人上传过这个文件，找到下面的信息
```
Header
Target Machine	
x64
Compilation Timestamp	
2011-11-05 06:00:45 UTC
Entry Point	
39040
Contained Sections	
6
```

这个 `Compilation Timestamp` 就是 `pe/coffHdr/timeDateStamp` 的值，记下来。

接下来看区段信息

```
Sections
Name	Virtual Address	Virtual Size	Raw Size	Entropy	MD5	Chi2
.text	4096	39790	39936	6.19	32e0a9863fbf8cf6511fa3be99413356	333302.19
.rdata	45056	16044	16384	4.44	72fa3098d49e49ca4d08fd845d135042	933041.19
.data	61440	2208	512	2.4	62b6b0cf7a342b8eeccd7c7bf219ffe1	63737
.pdata	65536	2052	2560	3.76	5ed9322bd457a6a60f0b383a4be722e7	235648.22
.rsrc	69632	288	512	1.52	15f1befa007aeab621241b6f5c32f0cc	92450
.reloc	73728	152	512	2.08	6b8349a1516a58d1565ec33fd35ab733	70077
```

计算最大的地址，向上以 0x1000 对齐，在上面就是 `(73728 + 512 + 0xfff)>>12<<12` 即 `0x1300`.

这一步的目的是计算 `pe/optionalHdr/windows/sizeOfImage`，之所以要这么算，是因为 virustotal 没给出。

有了这两个值, 使用下面的 url，通过微软的符号服务器，即可下载该文件：
```
"%s/%s/%08X%x/%s" % (serverName, peName, timeStamp, imageSize, peName)

where：
    serverName => https://msdl.microsoft.com/download/symbols
    peName => see the report by virustotal
    timeStamp => `pe/coffHdr/timeDateStamp`
    imageSize => `pe/optionalHdr/windows/sizeOfImage`
```

