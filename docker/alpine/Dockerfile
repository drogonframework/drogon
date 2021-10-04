FROM alpine:3.14

ARG USER=drogon
ARG UID=1000
ARG GID=1000
ARG USER_HOME=/drogon

ENV TZ=UTC

RUN apk update && apk --no-cache --upgrade add tzdata \
    && ln -snf /usr/share/zoneinfo/$TZ /etc/localtime \
    && echo $TZ > /etc/timezone

RUN apk --no-cache --upgrade add \
    sudo curl wget cmake make pkgconfig git gcc g++ \
    openssl libressl-dev jsoncpp-dev util-linux-dev zlib-dev c-ares-dev \
    postgresql-dev mariadb-dev sqlite-dev hiredis-dev

RUN addgroup -S -g $GID $USER \
    && adduser -D -u $UID -G $USER -h $USER_HOME $USER \
    && mkdir -p /etc/sudoers.d \
    && echo "$USER ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/$USER \
    && chmod 0440 /etc/sudoers.d/$USER

USER $USER
WORKDIR $USER_HOME

ENV LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8 \
    CC=gcc \
    CXX=g++ \
    AR=gcc-ar \
    RANLIB=gcc-ranlib \
    DROGON_INSTALLED_ROOT=$USER_HOME/install

RUN wget -O $USER_HOME/version.json https://api.github.com/repos/an-tao/drogon/git/refs/heads/master \
    && git clone https://github.com/an-tao/drogon $DROGON_INSTALLED_ROOT

RUN cd $DROGON_INSTALLED_ROOT \
    && sed -i 's/bash/sh/' ./build.sh \
    && ./build.sh
