FROM ubuntu:18.04

RUN apt-get update -yqq && \
    apt-get install -yqq --no-install-recommends software-properties-common && \
    apt-get install -yqq --no-install-recommends sudo curl wget cmake locales git \
    openssl libssl-dev libjsoncpp-dev uuid-dev zlib1g-dev 
RUN apt-get install -yqq --no-install-recommends postgresql-server-dev-all
RUN apt-get install -yqq --no-install-recommends libmariadbclient-dev
RUN apt-get install -yqq --no-install-recommends libsqlite3-dev
RUN apt-get install -yqq --no-install-recommends gcc-8 g++-8

RUN locale-gen en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

ENV CC=gcc-8
ENV CXX=g++-8
ENV AR=gcc-ar-8
ENV RANLIB=gcc-ranlib-8

ENV IROOT=/install
ENV DROGON_ROOT=$IROOT/drogon

WORKDIR $IROOT
ADD https://api.github.com/repos/an-tao/drogon/git/refs/heads/master version.json
RUN git clone https://github.com/an-tao/drogon

WORKDIR $DROGON_ROOT
RUN ./build.sh

