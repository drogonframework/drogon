@all:
	./build.sh

clean:
	rm -f lib/inc/drogon/config.h
	rm -rf ${build_dir}
	mkdir ${build_dir}
