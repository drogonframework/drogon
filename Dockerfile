FROM ubuntu:18.04

RUN apt-get update -yqq \
    && apt-get install -yqq --no-install-recommends software-properties-common \
    sudo curl wget cmake locales git gcc-8 g++-8 \
    openssl libssl-dev libjsoncpp-dev uuid-dev zlib1g-dev \
    postgresql-server-dev-all libmariadbclient-dev libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/* \
    && locale-gen en_US.UTF-8

ENV LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8 \
    CC=gcc-8 \
    CXX=g++-8 \
    AR=gcc-ar-8 \
    RANLIB=gcc-ranlib-8 \
    IROOT=/install

ENV DROGON_ROOT="$IROOT/drogon"

ADD https://api.github.com/repos/an-tao/drogon/git/refs/heads/master $IROOT/version.json
RUN git clone https://github.com/an-tao/drogon $DROGON_ROOT

WORKDIR $DROGON_ROOT

RUN ./build.sh
