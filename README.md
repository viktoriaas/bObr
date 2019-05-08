# bObr
C++ program to convert CZI images to VIPS

This program was developed as part of bachelor's thesis created at Faculty of Informatics, Masaryk University, Brno, Czech Republic.

The program is capable of converting big CZI images (over 100 000 x 100 000 px) with more focusing layers (Z-index) and many scenes (S-inex) to VIPS images. Each focusing layer is converted separately so if CZI image has 6 layers, program will produce 6 VIPS images.
If program has more scenes, the area between scenes is filled with colour of top left pixel.



Program is using two libraries:
   * libCZI - https://github.com/zeiss-microscopy/libCZI
   * libvips - https://github.com/libvips/libvips
   
Libvips pages offer manual how to install it.

Installing libCZI can produce some problems so here's how I did it:
   1. apt-get install cmake
   2. wget https://github.com/zeiss-microscopy/libCZI/archive/master.zip
   3. unzip master.zip 
   4. cd libCZI-master
   5. cmake ./
   6. make
   7. just wait until the end ...
   8. cd Src/libCZI
   9. mkdir /usr/include/libCZI
   10. cp libCZI*.h /usr/include/libCZI
   11. cp ImportExport.h /usr/include/libCZI
   12. cp priv_guiddef.h /usr/include/libCZI
   13. cp liblibCZI.so /usr/lib/x86_64-linux-gnu/

For successful compilation you need compiler g++ version *at least* 5.3. Compilation can be performed with:

```g++ bObr.cpp `pkg-config vips-cpp --cflags --libs` [path_to_libCZI] -llibCZI -lstdc++fs -o bobr```

where [path_to_libCZI] can be something like `-I /home/viktorias/libCZI-master/Src/ -L /home/viktorias/libCZI-master/Src/libCZI/`.

Run as `./bobr [path_to_czi_file] [optional_path_where_to_save_VIPS]`

If optional path isn't given, the program will save VIPS images to current working directory.


Tested on Ubunutu 18.10 and Debian 9.9.
