#!/usr/bin/env bash
########################################################################################################################
# This script setups dependencies for the InterOp library using Docker
#
#
#
# Build the Image
#
# $ docker build --rm -t ezralangois/interop -f ./tools/DockerFile tools
# $ docker images
# $ docker tag <image-id> ezralangois/interop:last_good
# $ docker tag ezralangois/interop:last_good ezralangois/interop:latest
# $ docker push ezralangois/interop:latest
#
########################################################################################################################
#set -ex
set -e

CMAKE_URL="http://www.cmake.org/files/v3.4/cmake-3.4.3-Linux-x86_64.tar.gz"
SWIG_URL="http://prdownloads.sourceforge.net/swig/swig-3.0.12.tar.gz"
MONO_URL="https://download.mono-project.com/sources/mono/mono-4.8.1.0.tar.bz2"
NUGET_URL="https://dist.nuget.org/win-x86-commandline/latest/nuget.exe"
GOOGLETEST_URL="https://github.com/google/googletest/archive/release-1.8.0.tar.gz"
JUNIT_URL="http://search.maven.org/remotecontent?filepath=junit/junit/4.12/junit-4.12.jar"
NUNIT_URL="https://github.com/nunit/nunitv2/releases/download/2.6.4/NUnit-2.6.4.zip"
JAVA_URL="http://download.oracle.com/otn-pub/java/jdk/8u131-b11/d54c1d3a095b4ff2b6607d096fa80163/jdk-8u131-linux-x64.rpm"
PROG_HOME=/opt
SWIG_HOME=${PROG_HOME}/swig3
JUNIT_HOME=${PROG_HOME}/junit
NUNIT_HOME=${PROG_HOME}/nunit

if hash cmake  2> /dev/null; then
    echo "Found CMake"
else
    wget --no-check-certificate --quiet -O - ${CMAKE_URL} | tar --strip-components=1 -xz -C /usr


    for PYBUILD in `ls -1 /opt/python`; do
        if [[ "$PYBUILD" == cp26* ]]; then
            continue
        fi
        if [[ "$PYBUILD" == cp33* ]]; then
            continue
        fi
        "/opt/python/${PYBUILD}/bin/pip" install numpy
    done

    # Current version 1.7.0 of auditwheel fails when building a fake pure Python lib with shared libs in data
    "/opt/python/cp36-cp36m/bin/pip" uninstall auditwheel -y
    "/opt/python/cp36-cp36m/bin/pip" install auditwheel==1.5.0
fi

if hash swig  2> /dev/null; then
    echo "Found Swig"
else
    if [ ! -e ${SWIG_HOME} ]; then
        mkdir ${SWIG_HOME}
    fi
    if [ ! -e ${SWIG_HOME}/src ]; then
        mkdir ${SWIG_HOME}/src
    fi
    wget --no-check-certificate --quiet -O - ${SWIG_URL} | tar --strip-components=1 -xz -C ${SWIG_HOME}/src
    cd ${SWIG_HOME}/src

    # TODO test if on centos or ubuntu
    yum install pcre-devel -y


    ./configure --prefix=/usr --with-python=/opt/python/cp27-cp27m/bin/python --with-python3=/opt/python/cp36-cp36m/bin/python
    make swig -j4 > /dev/null
    make install
    rm -fr ${SWIG_HOME}
    cd -
fi

if hash mono  2> /dev/null; then
    echo "Found mono"
else
    PATH_OLD=$PATH
    export PATH=/opt/python/cp27-cp27mu/bin/:$PATH
    yum install automake autoconf libtool  -y
    # For some reason GLIBC_HAS_CPU_COUNT does not work properly on Centos5 for 4.8.1.0
    #
    # The following function needs to be added to
    # 1. mono_src/mono/profiler/mono-profiler-log.c (4.8.1.0) or mono/profiler/proflog.c (Line: 48)
    # 2. mono/utils/mono-proclib.c (Line: 19)
    # #ifndef CPU_COUNT
    # #define CPU_COUNT(set) _CPU_COUNT((unsigned int *)(set), sizeof(*(set))/sizeof(unsigned int))
    # static int _CPU_COUNT(unsigned int *set, size_t len) {
    #     int cnt;
    #     cnt = 0;
    #     while (len--)
    #         cnt += __builtin_popcount(*set++);
    #     return cnt;
    # }
    #endif
    mkdir /mono_clean
    wget --no-check-certificate --quiet -O - ${MONO_URL} | tar --strip-components=1 -xj -C /mono_clean
    patch -p0 < /mono_patch.txt


    #cd /io/mono_src
    cd /mono_clean
    ./configure --prefix=/usr
    make -j4 && make install
    cd -
    rm -fr /mono_clean
    which dmcs
    which mono
    mono --version

    wget --no-check-certificate --quiet ${NUGET_URL} -O /usr/lib/nuget.exe
    echo "mono /usr/lib/nuget.exe \$@" > /usr/bin/nuget
    chmod +x /usr/bin/nuget
    export PATH=$PATH_OLD

    nuget help
fi

if [ -e /usr/include/gtest/gtest.h ]; then
    echo "Found GTest and GMock"
else
    mkdir /gtest
    wget --no-check-certificate --quiet -O - ${GOOGLETEST_URL} | tar --strip-components=1 -xz -C /gtest
    mkdir /gtest/build
    cmake -H/gtest -B/gtest/build
    cmake --build /gtest/build -- -j4
    cp -r /gtest/googletest/include/gtest /usr/include/
    cp -r /gtest/googlemock/include/gmock /usr/include/
    cp /gtest/build/googlemock/gtest/lib*.a /usr/lib/
    cp /gtest/build/googlemock/lib*.a /usr/lib/

    rm -fr /gtest
fi

if hash java  2> /dev/null; then
    echo "Found Java"
else
    wget --quiet --no-cookies --no-check-certificate --header "Cookie: gpw_e24=http%3A%2F%2Fwww.oracle.com%2F; oraclelicense=accept-securebackup-cookie" "${JAVA_URL}" -O ${JAVA_URL##*/}
    rpm -Uvh ${JAVA_URL##*/}
    rm -f  ${JAVA_URL##*/}
fi

if [ ! -e ${NUNIT_HOME}/NUnit-2.6.4 ]; then
    mkdir ${NUNIT_HOME}
    wget --no-check-certificate --quiet ${NUNIT_URL} -O  ${NUNIT_HOME}/${NUNIT_URL##*/}
    unzip ${NUNIT_HOME}/${NUNIT_URL##*/} -d ${NUNIT_HOME}
    rm -f ${NUNIT_HOME}/${JUNIT_URL##*/}
else
    echo "Found NUnit at ${NUNIT_HOME}/NUnit-2.6.4"
fi
NUNIT_HOME=${NUNIT_HOME}/NUnit-2.6.4

if [ ! -e ${JUNIT_HOME}/${JUNIT_URL##*/} ]; then
    mkdir ${JUNIT_HOME}
    wget --no-check-certificate --quiet ${JUNIT_URL} -O  ${JUNIT_HOME}/${JUNIT_URL##*/}
else
    echo "Found JUnit at ${JUNIT_HOME}/${JUNIT_URL##*/}"
fi


export JAVA_HOME=/usr/java/jdk1.8.0_131

which gcc
which g++
which swig
which java
which cmake
which mono


gcc --version
swig -version
java -version
cmake --version
mono --version


