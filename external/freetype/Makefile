# freetype makefile
include ./make.config

install:
	chmod +x ./build.sh
	bash -c "./build.sh $(HOSTARCH) $(DESTDIR)"

install-headers:

clean:
	-rm -rf build-freetype