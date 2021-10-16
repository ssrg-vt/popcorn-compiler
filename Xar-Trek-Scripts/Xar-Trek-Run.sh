#!/bin/bash

if [[ $1 == "" ]]; then
  printf "\n**************************************************************\n"
  printf "Usage :\n"
  printf "Xar-Trek-Run.sh <config_file>\n\n"
  printf "Example:\n"
  printf "Xar-Trek-Run.sh xar-trek.cfg \n"
  printf "**************************************************************\n\n"
  exit
fi

# Count number of applications on configuration file
app_qty=`grep APP_NAME xar-trek.cfg | wc -l`


# Find line numbers of APP_NAME parameters
for item in `grep -n -E '\s*APP_NAME\s*' xar-trek.cfg| cut -f1 -d:`
do
myarray[i]=$((item-1))
i=$((i+1))
done

# Create a configuration file for each application
for (( i=1; i<$app_qty; i++ ))
do
sed -n 1,$((${myarray[0]}-1))p xar-trek.cfg > APP\_$i.cfg
sed -n ${myarray[$((i-1))]},${myarray[$((i-0))]}p xar-trek.cfg >> APP\_$i.cfg
done
sed -n 1,$((${myarray[0]}-1))p xar-trek.cfg > APP\_$i.cfg
sed -n ${myarray[$((i-1))]},9000p xar-trek.cfg >> APP\_$i.cfg


# Create folders to store binaries and XCLBIN
mkdir Xar-Trek-BIN
mkdir Xar-Trek-XCLBIN

# Run the Xar-Trek compiler for each application
echo $app_qty "Applications" 
 
for (( i=1; i<=$app_qty; i++ ))
do

app_path=`grep APP_PATH APP\_$i.cfg| cut -f2 -d=| awk '{print $NF}'`
echo "Application Folder :" $app_path
mv APP\_$i.cfg $app_path
cp KNL_Function.h $app_path
cp KNL_Load_Exec_Template.c $app_path
cp libARMOpenCL.a $app_path

cd $app_path
../KNL_HW_Gen.sh APP\_$i.cfg
cp XO_FILE/*.xo ../Xar-Trek-XCLBIN
../Host_App_Gen.sh APP\_$i.cfg
binary=`grep -m 1 BIN Makefile| cut -f2 -d=| awk '{print $NF}'`
echo Binaries = $binary\_aarch64 $binary\_x86-64
source ~/rasec/setpath-popcorn
make clean; make
cp $binary\_aarch64 ../Xar-Trek-BIN
cp $binary\_x86-64 ../Xar-Trek-BIN

# Stores Application and function names
app_array[i]=`grep APP_NAME APP\_$i.cfg| cut -f2 -d=| awk '{print $NF}'`
func_array[i]="KNL_HW_"`grep FUNCTION_NAME APP\_$i.cfg| cut -f2 -d=| awk '{print $NF}'`


echo "*********************************************************************************"
echo "*********************************************************************************"
cd ..
done

# Create Execution Time and Threshold tables
# (the echo command didn't work inside the previous for loop)
for (( i=1; i<=$app_qty; i++ ))
do
echo ${app_array[$((i))]}","${func_array[$((i))]}",1000,1000" >>  KNL_HW_Sched.txt
echo ${app_array[$((i))]}",100,100,100" >>  KNL_HW_Exec.txt
done


# Copy the tables to the scheduler folder
cp KNL_HW*.txt ~/Pop_Scheduler/popcorn-scheduler


# Generate XCLBIN File
xclbin_type=`grep XCLBIN_TYPE $1 | cut -f2 -d=| awk '{print $1}'`
cd Xar-Trek-XCLBIN
../KNL_XCLBIN_Gen.sh ../$1
cp *.xclbin ../Xar-Trek-BIN
if [[ $xclbin_type == "sw_emu" ]]; then
cp *.json ../Xar-Trek-BIN
fi



cd ..



