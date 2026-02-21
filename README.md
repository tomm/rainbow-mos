# Rainbow MOS

Rainbow MOS is an eZ80 firmware for the Agon Light / Agon Light 2 / Console 8
computers. It is descended from Quark MOS by Dean Belfield, and from Console8
MOS up to v2.3.3. "Agon Platform MOS" firmware documentation should apply 
to Rainbow MOS, for features added up to v2.3.3, and any features specifically
documented here as having been ported from Platform MOS 3+.

Rainbow MOS binaries can be flashed onto the Agon EZ80 using
[Agon Flash](https://github.com/AgonPlatform/agon-flash).

Rainbow MOS's unique features are:
 - Compiled with modern AgonDev LLVM toolchain, not Zilog ZDS
 - A buffered keyboard event API that solves the issues with all the other MOS
   keyboard APIs
 - Support for framebuffer video on the eZ80 side of the Agon (with
   [eZ80 GPIO video driver](https://github.com/tomm/vga-ez80))
 - Bash-like tab autocompletion
 - Improved command output pagination
 - Reorganized memory map, reducing kernel footprint from 16KiB RAM to 8KiB
 - Syscall to override zero-page reset vectors
 - Built-in `memdump` command
 - Built-in `sideload` command (performs the function of `hexload vdp`)

Features incorporated from Platform MOS 3.x:
 - All ffs_api_* syscalls (FatFS API)
 - mos_api_unpackrtc and mos_api_flseek_p syscalls

New syscalls unique to Rainbow MOS are documented as ASM examples in
[./examples](./examples).

## Other Links
 - [eZ80 GPIO video driver](https://github.com/tomm/vga-ez80)
 - [AgonDev](https://github.com/AgonPlatform/agondev/)
 - [Agon Flash](https://github.com/AgonPlatform/agon-flash).
 - [Agon Platform MOS documentation](https://agonplatform.github.io/agon-docs/mos/API/)
