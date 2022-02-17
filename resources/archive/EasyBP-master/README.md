# EasyBP

If you are still annoyed when you want to debug a macOS PoC, but the key method is called so frequently that you can't tell which one is yours, you can use this kernel extension.

# Introduction & Usage
This kext is only a simple inline hook for is_io_connect_method, and set add a selector check within it. 
```
#define SELECTOR_MASK 0xFEFEF000
```

so, if you want to debug a selector which is 0x1, you can simply input the selector in the IOConnectCallMethod with 
```
0x1 + SELECTOR_MASK
```

EasyBP will check this method using refine_selector function. So, you can make breakpoint in this function. If hit, it means  this call is yours.
```
breakpoint set -n refine_selector
```

if you debug the syscall, you can just extent it.
[EasyBP](https://adc.github.trendmicro.com/lilang-wu/EasyBP)
