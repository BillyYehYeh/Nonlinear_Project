#!/usr/bin/env bash
# Local SystemC environment for this workspace
export SYSTEMC_HOME=/home/users/BillyYeh/Nonlinear_Project/.local/systemc-install
export CPLUS_INCLUDE_PATH="$SYSTEMC_HOME/include:${CPLUS_INCLUDE_PATH}"
export LIBRARY_PATH="$SYSTEMC_HOME/lib64:${LIBRARY_PATH}"
export LD_LIBRARY_PATH="$SYSTEMC_HOME/lib64:${LD_LIBRARY_PATH}"
export PKG_CONFIG_PATH="$SYSTEMC_HOME/lib64/pkgconfig:${PKG_CONFIG_PATH}"

echo "SYSTEMC_HOME=$SYSTEMC_HOME"
