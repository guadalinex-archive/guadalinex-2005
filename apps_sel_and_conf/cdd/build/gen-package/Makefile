v3: packages-v3-base packages-v3-escritorio packages-v3-noescritorio packages-v3-extras
	./build ../../conf/v3-base
	./build ../../conf/v3-escritorio
	./build ../../conf/v3-noescritorio
	./build ../../conf/v3-extras
	./buildmeta meta-guadalinex-v3-base meta-guadalinex-v3-escritorio meta-guadalinex-v3-noescritorio meta-guadalinex-v3-extras

suplementos: packages-2005-suplementos
	./build ../../conf/2005-suplementos
	
mini: packages-2005-mini-escritorio
	./build ../../conf/2005-mini-escritorio
	./build ../../conf/2005-mini-noescritorio
	./build ../../conf/2005-base
	./buildmeta-mini  meta-guadalinex-2005-base meta-guadalinex-2005-mini-escritorio meta-guadalinex-2005-mini-noescritorio

test: packages-test
	./build ../../conf/test

test2: packages-test2
	./build ../../conf/test2

all: 2005 mini suplementos
