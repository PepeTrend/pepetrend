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
### LATEST RELEASES
#### VR-3032u:
[**VR-3032u-VA31-412PEPETREND-C03_R01p01.bin**](https://github.com/PepeTrend/pepetrend/raw/master/firmwares/VR-3032u-VA31-412PEPETREND-C03_R01p01.bin)

**`md5sum: 258d05b791a9e4906d597b8377b336ad`**

**`sha256sum: 777eccc7ddb6c58701047335e4ed777bdb40d5b99879c076be695af678610759`**


#### AR-5315u:
[**AR-5315u-UC31-412PEPETREND-C01_R05p01.bin**](https://github.com/PepeTrend/pepetrend/raw/master/firmwares/AR-5315u-UC31-412PEPETREND-C01_R05p01.bin)

**`md5sum: 4ea19d293313dc10cf31de7544074f81`**

**`sha256sum: 45c92f715bdb85989a229b6ac4e0c4905919bd45953db404168e64318501050c`**

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

