FROM ubuntu:22.04

ENV TZ=UTC
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update -yqq \
    && apt-get install -yqq --no-install-recommends software-properties-common \
    sudo curl wget cmake make pkg-config locales git gcc-11 g++-11 \
    openssl libssl-dev libjsoncpp-dev uuid-dev zlib1g-dev libc-ares-dev\
    postgresql-server-dev-all libmariadb-dev libsqlite3-dev libhiredis-dev\
    && rm -rf /var/lib/apt/lists/* \
    && locale-gen en_US.UTF-8

ENV LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8 \
    CC=gcc-11 \
    CXX=g++-11 \
    AR=gcc-ar-11 \
    RANLIB=gcc-ranlib-11 \
    IROOT=/install

ENV DROGON_ROOT="$IROOT/drogon"

ADD https://api.github.com/repos/drogonframework/drogon/git/refs/heads/master $IROOT/version.json
RUN git clone https://github.com/drogonframework/drogon $DROGON_ROOT

WORKDIR $DROGON_ROOT

RUN ./build.sh
