include _conf.txt

all: $(subdirs)

.PHONY: $(subdirs)

$(subdirs):
	$(MAKE) -C $@

install:
	for dir in $(subdirs); do $(MAKE) -C $$dir install; done

clean:
	for dir in $(subdirs); do $(MAKE) -C $$dir clean; done
