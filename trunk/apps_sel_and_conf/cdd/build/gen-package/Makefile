v3: packages-v3-base packages-v3-escritorio packages-v3-noescritorio packages-v3-extras
	./build ../../conf/v3-base
	./build ../../conf/v3-escritorio
	./build ../../conf/v3-noescritorio
	./build ../../conf/v3-extras
	./buildmeta meta-guadalinex-v3-base meta-guadalinex-v3-escritorio meta-guadalinex-v3-noescritorio meta-guadalinex-v3-extras

suplementos: packages-v3-suplementos
	./build ../../conf/v3-suplementos
	
mini: packages-v3-mini-escritorio
	./build ../../conf/v3-mini-escritorio
	./build ../../conf/v3-mini-noescritorio
	./build ../../conf/v3-mini-base
	./buildmeta-mini  meta-guadalinex-v3-mini-base meta-guadalinex-v3-mini-escritorio meta-guadalinex-v3-mini-noescritorio

test: packages-test
	./build ../../conf/test

test2: packages-test2
	./build ../../conf/test2

midistro: packages-midistro-componente1 packages-midistro-componente2
	./build ../../conf/midistro-componente1
	./build ../../conf/midistro-componente2
	./buildmeta-midistro meta-guadalinex-midistro-componente1 meta-guadalinex-midistro-componente2

all: v3 mini suplementos midistro
