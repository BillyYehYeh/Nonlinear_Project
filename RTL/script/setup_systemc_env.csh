#!/usr/bin/env tcsh
# Local SystemC environment for this workspace
setenv SYSTEMC_HOME /home/users/BillyYeh/Nonlinear_Project/.local/systemc-install
if ( $?CPLUS_INCLUDE_PATH ) then
  setenv CPLUS_INCLUDE_PATH "${SYSTEMC_HOME}/include:${CPLUS_INCLUDE_PATH}"
else
  setenv CPLUS_INCLUDE_PATH "${SYSTEMC_HOME}/include"
endif
if ( $?LIBRARY_PATH ) then
  setenv LIBRARY_PATH "${SYSTEMC_HOME}/lib64:${LIBRARY_PATH}"
else
  setenv LIBRARY_PATH "${SYSTEMC_HOME}/lib64"
endif
if ( $?LD_LIBRARY_PATH ) then
  setenv LD_LIBRARY_PATH "${SYSTEMC_HOME}/lib64:${LD_LIBRARY_PATH}"
else
  setenv LD_LIBRARY_PATH "${SYSTEMC_HOME}/lib64"
endif
if ( $?PKG_CONFIG_PATH ) then
  setenv PKG_CONFIG_PATH "${SYSTEMC_HOME}/lib64/pkgconfig:${PKG_CONFIG_PATH}"
else
  setenv PKG_CONFIG_PATH "${SYSTEMC_HOME}/lib64/pkgconfig"
endif

echo "SYSTEMC_HOME=${SYSTEMC_HOME}"
