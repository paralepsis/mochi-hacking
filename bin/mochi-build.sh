#!/bin/bash

# NOTES:
# - Pulls BMI into ~/git/bmi, patches, builds in tree.
# - Patches BMI with patch to address OSX shared lib issue.
# - Builds in ~/git/mercury/build.
# - Installs everything into ~rross/local/
#
# - Required installation of libev and coreutils packages (macports).
#
export PATH="/Users/rross/local/bin:$PATH"
export LD_LIBRARY_PATH="/Users/rross/local/lib:$LD_LIBRARY_PATH"
export DYLD_LIBRARY_PATH="/Users/rross/local/lib:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/Users/rross/local/lib/pkgconfig"

# Test logs are in <build directory>/Testing/Temporary/LastTest.log

#
# SRCDIR  -- directory in which code is checked out (subdirs created here)
# INSTDIR -- directory in which installations are performed (subdirs created)
#
# NOTES:
# - All builds are performed in a "build" subdirectory inside the source tree,
#   except for BMI.
#
SRCDIR=/Users/rross/git
INSTDIR=/Users/rross/local

BUILDTYPE=Debug
FLAGS=
EXTRAFLAGS=
DEBUG=

#
# PATCH AND BUILD BMI
#
echo "#################################################################"
echo "#                            BMI                                #"
echo "#################################################################"

cd $SRCDIR
git clone git://git.mcs.anl.gov/bmi
cd bmi
patch -N <<"EOF"
diff --git a/Makefile.in b/Makefile.in
index f22ab91..0be7b41 100644
--- a/Makefile.in
+++ b/Makefile.in
@@ -720,7 +720,7 @@ lib/libbmi.a: $(LIBBMIOBJS)
 lib/libbmi.so: $(LIBBMIPICOBJS)
 	$(Q) "  LDSO		$@"
 	$(E)$(INSTALL) -d lib
-	$(E)$(LDSHARED) -Wl,-soname,libbmi.so -o $@ $(LIBBMIPICOBJS) $(DEPLIBS)
+	$(E)$(LDSHARED) -o $@ $(LIBBMIPICOBJS) $(DEPLIBS)
 endif
 
 # rule for building the pvfs2 library
EOF

./prepare && ./configure --enable-shared --enable-bmi-only --prefix=${INSTDIR}
make && make install

#
# CLONE MERCURY, UPDATE MERCURY SUBMODULES
#
echo "#################################################################"
echo "#                          MERCURY                              #"
echo "#################################################################"

cd $SRCDIR
git clone --recurse-submodules https://github.com/mercury-hpc/mercury.git 
# git submodule update --init

cd mercury

#
# SETUP BUILD DIR, PATCH, CONFIGURE MERCURY
#

install -d build
cd build

cmake \
   -DCMAKE_INSTALL_PREFIX=${INSTDIR} \
   -DCMAKE_BUILD_TYPE=$BUILDTYPE \
   -DCMAKE_C_FLAGS="$FLAGS $EXTRAFLAGS" \
   -DCMAKE_CXX_FLAGS="$FLAGS $EXTRAFLAGS" \
   -DCMAKE_C_FLAGS_DEBUG="$DEBUG" \
   -DNA_USE_MPI=OFF \
   -DNA_USE_CCI=OFF \
   -DNA_USE_BMI=ON \
   -DBMI_INCLUDE_DIR=${INSTDIR}/include \
   -DBMI_LIBRARY=${INSTDIR}/lib/libbmi.a \
   -DBUILD_SHARED_LIBS=ON \
   -DBUILD_EXAMPLES=OFF \
   -DBUILD_TESTING=ON \
   -DMERCURY_USE_BOOST_PP=ON \
   -DMERCURY_USE_EAGER_BULK=OFF \
   ..

#
# BUILD MERCURY
# echo "Next is make VERBOSE=1 in the Mercury build directory."
#

cd ${SRCDIR}/mercury/build
make
make test
make install

#
# CLONE AND PREP ARGOBOTS
#  
echo "#################################################################"
echo "#                          ARGOBOTS                             #"
echo "#################################################################"

cd $SRCDIR
git clone http://git.mcs.anl.gov/argo/argobots.git
cd argobots
./autogen.sh

patch -N -p1 <<"EOF"
diff --git a/src/mem/malloc.c b/src/mem/malloc.c
index f4d77c8..6526dae 100644
--- a/src/mem/malloc.c
+++ b/src/mem/malloc.c
@@ -16,6 +16,10 @@
 #include <sys/types.h>
 #include <sys/mman.h>
 
+#ifndef MAP_ANONYMOUS
+#define MAP_ANONYMOUS MAP_ANON
+#endif
+
 #define PROTS           (PROT_READ | PROT_WRITE)
 #define FLAGS_RP        (MAP_PRIVATE | MAP_ANONYMOUS)
 
EOF


#
# BUILD ARGOBOTS, TEST, AND INSTALL
#

install -d build
cd build
../configure --prefix=${INSTDIR} --enable-perf-opt --disable-thread-cancel --disable-task-cancel --disable-migration --enable-affinity
make -j 3
cd test
make check
cd ..
make install

#
# CLONE, PATCH, CONFIGURE, BUILD, TEST, AND INSTALL ABT-SNOOZER
#
echo "#################################################################"
echo "#                          ABT-SNOOZER                          #"
echo "#################################################################"

cd $SRCDIR
git clone git@xgitlab.cels.anl.gov:sds/abt-snoozer.git
cd abt-snoozer
./prepare.sh

patch -N <<"EOF"
--- configure.orig	2016-06-10 11:18:24.000000000 -0500
+++ configure	2016-06-10 11:18:42.000000000 -0500
@@ -5446,7 +5446,7 @@
                 CPPFLAGS="$CPPFLAGS -I${with_libev}/include"
                 # note: add rpath too because stock libev install uses
                 # shared libs
-                LDFLAGS="$LDFLAGS -Wl,-rpath=${with_libev}/lib -L${with_libev}/lib"
+                LDFLAGS="$LDFLAGS -L${with_libev}/lib"
                 SNOOZER_PKGCONFIG_LIBS="${SNOOZER_PKGCONFIG_LIBS} -L${with_libev}/lib -lev -lm"
 
 else
EOF

install -d build
cd build
../configure --prefix=${INSTDIR} \
  --with-libev=/opt/local \
  PKG_CONFIG_PATH="${INSTDIR}/lib/pkgconfig" 
make
make check
make install

#
# CLONE, CONFIGURE, BUILD, TEST, AND INSTALL MARGO
#
echo "#################################################################"
echo "#                            MARGO                              #"
echo "#################################################################"

cd $SRCDIR
git clone git@xgitlab.cels.anl.gov:sds/margo.git
cd margo
./prepare.sh
install -d build
cd build
../configure --prefix=${INSTDIR} CFLAGS="-g -Wall" \
  PKG_CONFIG_PATH="${INSTDIR}/lib/pkgconfig" \
  TIMEOUT=gtimeout MKTEMP=gmktemp
make -j 3
make check
make install

#
# CLONE, CONFIGURE, BUILD, TEST, AND INSTALL MARGO
#
echo "#################################################################"
echo "#                           ABT-IO                              #"
echo "#################################################################"

cd $SRCDIR
git clone git@xgitlab.cels.anl.gov:sds/abt-io.git
cd abt-io 
./prepare.sh
install -d build
cd build
../configure --prefix=${INSTDIR} CFLAGS="-g -Wall" \
  PKG_CONFIG_PATH="${INSTDIR}/lib/pkgconfig"
make -j 3
make check
make install
