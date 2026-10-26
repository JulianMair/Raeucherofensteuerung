# core besteht in M1 nur aus Headern (Interfaces). Ab M2 kommen Quelldateien dazu.
add_library(rof_core INTERFACE)
target_include_directories(rof_core INTERFACE include)
