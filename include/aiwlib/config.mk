# main options for make
#-------------------------------------------------------------------------------
SHELL=/bin/bash

ifndef PYTHON_H_PATH
PYTHON_H_PATH=/usr/include/python2.7
endif

CXXOPT:=$(CXXOPT) -Wall -fopenmp -O3 -fPIC -g -std=c++11 -I$(PYTHON_H_PATH)
LINKOPT:=$(LINKOPT) -lgomp -lz
SWIGOPT:=$(SWIGOPT) -Wall -python -c++

ifndef SWIG
SWIG:=swig
endif

ifndef GCC
GCC:=g++
endif
#-------------------------------------------------------------------------------
#GCC:=i586-mingw32msvc-g++
#CXXOPT:= -O3 -DMINGW -g 
#SWIGOPT:=-DMINGW
#LDOPT:=-L ~/.wine/drive_c/Python26/libs/ -lpython26
#PYTHON_H_PATH=/home/aiv/.wine/drive_c/Python26/include/
#-------------------------------------------------------------------------------
define show_target
@echo
@echo TARGET: "$@"
@if [ -f "$@" ]; then if [ "$(filter-out /usr/%,$?)" ]; then echo NEW: $(filter-out /usr/%,$?); else echo NEW: $?; fi; fi
@echo ALL: $(filter-out /usr/%,$^)
endef
#-------------------------------------------------------------------------------
ifeq (on,$(debug))
CXXOPT:=$(CXXOPT) -DEBUG
endif
#-------------------------------------------------------------------------------
define CXX

$(show_target)
$(GCC) $(CXXOPT)
endef
#-------------------------------------------------------------------------------
.SUFFIXES :
#-------------------------------------------------------------------------------
define imodule
@echo %module $(notdir $(basename $@)) > $@
@echo "%exception {" >> $@
@echo "    try{ \$$action }" >> $@
@echo "    catch(const char *e){ PyErr_SetString(PyExc_RuntimeError, e); return NULL; }" >> $@
@echo "    catch(...){ return NULL; }" >> $@
@echo "}" >> $@
@echo "%pythoncode %{def _setstate(self, state):" >> $@
@echo "    if not hasattr(self, 'this'): self.__init__()" >> $@
@echo "    self.__C_setstate__(state)" >> $@
@echo "def _swig_setattr(self, class_type, name, value):" >> $@
@echo "    if name in class_type.__swig_setmethods__: value = getattr(self, name).__class__(value)" >> $@
@echo "    return _swig_setattr_nondynamic(self, class_type, name, value, 0)" >> $@
@echo "__makefile__ = '$(word 1, $(MAKEFILE_LIST))'" >> $@
@echo "%}" >> $@
@echo "%typemap(out) bool&   %{ \$$result = PyBool_FromLong    ( *\$$1 ); %}" >> $@
@echo "%typemap(out) char&   %{ \$$result = PyInt_FromLong     ( *\$$1 ); %}" >> $@
@echo "%typemap(out) short&  %{ \$$result = PyInt_FromLong     ( *\$$1 ); %}" >> $@
@echo "%typemap(out) int&    %{ \$$result = PyInt_FromLong     ( *\$$1 ); %}" >> $@
@echo "%typemap(out) long&   %{ \$$result = PyInt_FromLong     ( *\$$1 ); %}" >> $@
@echo "%typemap(out) float&  %{ \$$result = PyFloat_FromDouble ( *\$$1 ); %}" >> $@
@echo "%typemap(out) double& %{ \$$result = PyFloat_FromDouble ( *\$$1 ); %}" >> $@
@echo '%feature("autodoc","1");' >> $@
@echo '%include "std_string.i"' >> $@
@echo "%inline %{ namespace aiw{}; %}" >> $@
endef
#-------------------------------------------------------------------------------
