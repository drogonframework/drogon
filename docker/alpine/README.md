## Build Docker Image

```shell
$ cd drogon/docker/alpine # from this repository
$ docker build --no-cache --build-arg UID=`id -u` --build-arg GID=`id -g` -t drogon-alpine . # include last dot(.)
```

## Create a Drogon Project

```shell
$ cd ~/drogon_app # example
$ docker run --rm -v="$PWD:/drogon/app" -w="/drogon/app" drogon-alpine drogon_ctl create project hello_world
```

## Build the Project

```shell
$ cd hello_world
$ docker run --rm --volume="$PWD:/drogon/app" -w="/drogon/app/build" drogon-alpine sh -c "cmake .. && make"
```

## Start Server

```shell
$ docker run --name drogon_test --rm -u 0 -v="$PWD/build:/drogon/app" -w="/drogon/app" -p 8080:80 -d drogon-alpine ./hello_world # expose port 80 to 8080
```

## Stop Server

```shell
$ docker kill drogon_test
```
