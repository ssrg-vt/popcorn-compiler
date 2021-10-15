#!/bin/bash
if [[ $1 == "" ]]; then
  printf "\n**************************************************************\n"
  printf "Usage :\n"
  printf "KNL_XCLBIN_Gen.sh <config_file>\n\n"
  printf "Example:\n"
  printf "KNL_XCLBIN_Gen.sh config.cfg \n"
  printf "**************************************************************\n\n"
  exit
fi

# Get target platform, XLCBIN file name and type
target_platform=`grep TARGET_PLAT $1 | cut -f2 -d=| awk '{print $1}'`
xclbin_file_name=`grep XCLBIN_NAME $1 | cut -f2 -d=| awk '{print $1}'`
xclbin_type=`grep XCLBIN_TYPE $1 | cut -f2 -d=| awk '{print $1}'`

# Partition - TODO
#if [[ $xclbin_type == "hw" ]]; then

#fi



# Set environment variables
source /tools/Xilinx/Vitis/2020.2/settings64.sh
source /opt/xilinx/xrt/setup.sh

v++ --hls.jobs 18 -l -o $xclbin_file_name \
     *.xo \
    -t $xclbin_type \
    --platform $target_platform \


# Generate files for software emulation
if [[ $xclbin_type == "sw_emu" ]]; then
# Generates the emulation configuration file
emconfigutil --platform $target_platform
fi



