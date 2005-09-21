2005: packages-2005-base packages-2005-escritorio packages-2005-noescritorio
	./build ../../conf/2005-base
	./build ../../conf/2005-escritorio
	./build ../../conf/2005-noescritorio
	./buildmeta meta-guadalinex-2005-base meta-guadalinex-2005-escritorio meta-guadalinex-2005-noescritorio

mini: packages-mini-escritorio
	./build ../../conf/2005-mini-escritorio
	./buildmeta-mini meta-guadalinex-2005-base meta-guadalinex-2005-mini-escritorio meta-guadalinex-2005-mini-noescritorio

test: packages-test
	./build ../../conf/test

test2: packages-test2
	./build ../../conf/test2

all: 2005 mini
