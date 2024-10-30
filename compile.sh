# g++ -c -O3 ParticleFilters.c
# g++  *.o -O3 -g -lGL -lGLU -lglut -o ParticleFilters

# Compile ParticleFilters.c
g++ -c -O3 ParticleFilters.c

# Link all object files with -no-pie to avoid PIE enforcement
g++ -no-pie *.o -O3 -g -lGL -lGLU -lglut -o ParticleFilters


