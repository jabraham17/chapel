#!/bin/sh

alias hide=eval

yum -y install git gcc gcc-c++ m4 perl python tcsh bash perl python python-devel python-setuptools bash make gawk python3 which

yum -y install wget tar openssl-devel
hide yum -y install sudo


hide MYTMP=`su - bin -c "mktemp -d"`
hide cd $MYTMP

su - bin -c "wget https://github.com/Kitware/CMake/releases/download/v3.25.1/cmake-3.25.1.tar.gz"
su - bin -c "tar xvzf cmake-3.25.1.tar.gz"
su - bin -c "cd cmake-3.25.1 && ./bootstrap && make"
make install
update-alternatives --install /usr/bin/cmake cmake /usr/local/bin/cmake 1
