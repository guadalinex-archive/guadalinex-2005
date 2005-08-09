all: partconf partman partman-auto partman-basicfilesystems partman-basicmethods partman-ext3 partman-newworld partman-partitioning partman-target

partconf: 
	cd partconf-1.09ubuntu1/ && debian/rules binary

partman:
	cd partman-68ubuntu1/ && debian/rules binary

partman-auto:
	cd partman-auto-41ubuntu3/ && debian/rules binary

partman-basicfilesystems:
	cd partman-basicfilesystems-39ubuntu2/ && debian/rules binary

partman-basicmethods:
	cd partman-basicmethods-26ubuntu1/ && debian/rules binary

partman-ext3:
	cd partman-ext3-37ubuntu1/ && debian/rules binary

partman-newworld:
	cd partman-newworld-11ubuntu1/ && debian/rules binary

partman-partitioning:
	cd partman-partitioning-34ubuntu1/ && debian/rules binary

partman-target:
	cd partman-target-36ubuntu2/ && debian/rules binary

clean:
	cd partconf-1.09ubuntu1/ && debian/rules clean
	cd partman-68ubuntu1/ && debian/rules clean
	cd partman-auto-41ubuntu3/ && debian/rules clean
	cd partman-basicfilesystems-39ubuntu2/ && debian/rules clean
	cd partman-basicmethods-26ubuntu1/ && debian/rules clean
	cd partman-ext3-37ubuntu1/ && debian/rules clean
	cd partman-newworld-11ubuntu1/ && debian/rules clean
	cd partman-partitioning-34ubuntu1/ && debian/rules clean
	cd partman-target-36ubuntu2/ && debian/rules clean

.PHONY: all clear
