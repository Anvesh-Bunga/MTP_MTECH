# Module definition for gym (src/gym/CMakeLists.txt)

# Source files
set(source_files
  bwp-rl-env.cc
)

# Header files
set(header_files
  bwp-rl-env.h
)

# Build the gym module and link dependencies
build_lib(
  LIBNAME gym
  SOURCE_FILES ${source_files}
  HEADER_FILES ${header_files}
  LIBRARIES_TO_LINK
    ${libcore}       # NS-3 core
    ${libnetwork}    # Network module
    ${libopengym}    # Contrib OpenGym module (for generated headers)
)

