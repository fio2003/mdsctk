## Note that this Makefile is provided for convenience, but
## is DEPRECATED.
## Running cmake will overwrite this Makefile!!!
CFLAGS=-I$(GMXLDLIB)/../include
OPTIONS=-fopenmp -O3 -DHAVE_INLINE -DGSL_RANGE_CHECK_OFF -Wl,-as-needed 
# OPTIONS=-g
CC=gcc
CPP=g++
F77=gfortran
## Extra libs are not yet needed, but future versions will need their features.
## LIBS=-L${GMXLDLIB} -lgsl -lcblas -larpack -llapack -lblas -ldb_cxx -lpthread -lgmx -lgmxana 
LIBS=-L${GMXLDLIB} -lgsl -larpack -lblas -ldb_cxx -lpthread -lgmx -lgmxana 

TARGETS=auto_decomp_sparse \
auto_decomp_sparse_nystrom \
bb_xtc_to_phipsi \
check_xtc \
decomp_sparse \
decomp_sparse_nystrom \
knn_data \
knn_rms \
make_gesparse \
make_sysparse \
phipsi_to_sincos \
rms_test \
split_xtc

# Targets
all : $(TARGETS)

# Executables
$(TARGETS) : % : %.o
	$(CPP) $(CFLAGS) $(OPTIONS) $^ $(LIBS) -o $@

# Objects
%.o : %.cpp
	$(CPP) $(CFLAGS) $(OPTIONS) -c $< -o $@

clean :
	rm -f *.o

dist-clean : clean
	rm -f $(TARGETS)