ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/tools/redist/MySQL-python-1.2.1_p2,,$(shell pwd))
endif

INSTALL_DIR = $(MF_ROOT)/bigworld/tools/server/pycommon/redist/mysql_python

all:
	rm -rf build
	python setup.py build
	python setup.py clean
	mkdir -p $(INSTALL_DIR)
	touch `dirname $(INSTALL_DIR)`/__init__.py
	cp -a build/lib.*/* $(INSTALL_DIR)

clean:
	python setup.py clean --all
