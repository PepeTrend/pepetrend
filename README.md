# pepetrend
## Firmwares for Comtrend routers (VR-3032u/AR-5315u, formerly deployed by Jazztel) compatible with Pepephone and MasMovil xDSL lines. 

This project contains all files needed to build a BCM963xx-based Comtrend xDSL Router image (VR-3032u/AR-5315u), except toolchain. For **toolchain**, please consult the **`TOOLCHAIN`** section below.

To build an image, enter:
```sh
$ make PROFILE=<profile_name>
```

For example,
```sh
$ make PROFILE=VR-3032u-PEPETREND
```

The final image is located in the **`images`** subdirectory. This image can be loaded into the router using the Web interface image upgrade item.

Besides, if you don't want to compile the code by yourself, you can check **`firmwares`** directory for latest *PepeTrend* builds.

---
### TOOLCHAIN
#### See [AR-5315u_PLD project](https://github.com/antonywcl/AR-5315u_PLD).
To prepare building environment, you need a linux box with a cross-compiler.
You may need to install more compiling tools while you build the image.
 - Download Cross-compiler:
Please download **uclibc-crosstools-gcc-4.4.2-1.tar.bz2** via: 
https://drive.google.com/file/d/0B_Oc_Z7pts5AOF9rWmJlUzAwYWs/view?usp=sharing
 - Setup Cross-compiler:
*un-tar* the cross-compiler to **`/opt`**, the path shall be **`/opt/toolchains/uclibc-crosstools-gcc-4.4.2-1/...`**

You're done!

