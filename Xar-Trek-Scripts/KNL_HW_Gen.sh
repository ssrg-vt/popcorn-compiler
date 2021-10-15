#!/bin/bash

if [[ $1 == "" ]]; then
  printf "\n**************************************************************\n"
  printf "Usage :\n"
  printf "KNL_HW_Gen.sh <config_file>\n\n"
  printf "Example:\n"
  printf "KNL_HW_Gen.sh config.cfg \n"
  printf "**************************************************************\n\n"
  exit
fi

# Get target platform, XLCBIN file name and type
target_platform=`grep TARGET_PLAT $1 | cut -f2 -d=| awk '{print $1}'`
xclbin_file_name=`grep XCLBIN_NAME $1 | cut -f2 -d=| awk '{print $1}'`
xclbin_type=`grep XCLBIN_TYPE $1 | cut -f2 -d=| awk '{print $1}'`


# Get function file location and name
function_file_path=`grep FUNCTION_FILE_PATH $1| cut -f2 -d=| awk '{print $NF}'`
function_file_name=`grep FUNCTION_FILE_NAME $1| cut -f2 -d=| awk '{print $NF}'`
function_file="${function_file_path}${function_file_name}"
echo Function File = $function_file
sw_fc_name=`grep FUNCTION_NAME $1| cut -f2 -d=| awk '{print $NF}'`
echo Function Name = $sw_fc_name

mkdir XO_FILE
cd XO_FILE

# copy function file
cp ../$function_file $sw_fc_name.cpp

KNL_0=`grep -n -o '^[^//]*' $sw_fc_name.cpp| grep -E "(\w+\s+)+(\b$sw_fc_name\b){1}\s*"| wc -l`

# if there is one: get it
# if there are two: get the second (the first one must be the prototype)
if [[ "$KNL_0" == 1 ]]; then
  KNL_0=`grep -n -o '^[^//]*' $sw_fc_name.cpp| grep -E "(\w+\s+)+(\b$sw_fc_name\b){1}\s*"| head -n 1| cut -f1 -d:`
else
    KNL_0=`grep -n -o '^[^//]*' $sw_fc_name.cpp| grep -E "(\w+\s+)+(\b$sw_fc_name\b){1}\s*"| tail -n 1| cut -f1 -d:`
fi

echo KNL 0 =  $KNL_0


# Include wrapper
sed -i "$KNL_0 i \
extern \"C\" {  // start wrapper\n\
" $sw_fc_name.cpp

KNL_0=$((KNL_0+2))

# Append prefix KNL_HW_ to sw function and insert void before it
sed -i "${KNL_0}s/^.*$sw_fc_name/void KNL_HW_$sw_fc_name/" $sw_fc_name.cpp
# echo KNL 0 =  $KNL_0


# insert wrapper end bracket
count=0
# Had to group while with the last commands in order not to loose the value of $last_line
tail -n +$KNL_0 $sw_fc_name.cpp|grep -n -E '^[^\/\/]*'|grep -e { -e}|
(while read line;
do
  # echo line is: "$line"
  if [[ "$line" =~ .*"}".* ]]; then
    # echo close
    count=$((count-1))
  fi

  if [[ "$line" =~ .*"{".* ]]; then
    # echo open
    count=$((count+1))
  fi

  # echo count = "$count"

  # last_line contains the line with last "}"
  if [ $count == 0 ]; then
   # echo Found $line $KNL_0
   str1=`echo $line|cut -f1 -d:`
   # echo str1 = $str1
   last_line=$(($KNL_0+$str1-1))
   # echo Last = $last_line

   break
  fi
  # echo " "

  # echo

done

# Insert last bracket
sed -i "$last_line a \
} // finish wrapper \n\
" $sw_fc_name.cpp

echo)


# Generate XO file

# Set environment variables
source /tools/Xilinx/Vitis/2020.2/settings64.sh
source /opt/xilinx/xrt/setup.sh


# Generates the XCLBIN (FPGA Kernel)
v++ -c -k "KNL_HW_$sw_fc_name" -o "KNL_HW_$sw_fc_name.$target_platform.xo" \
    "./$sw_fc_name.cpp" \
    -t $xclbin_type \
    --platform $target_platform \
    -I ../$function_file_path
