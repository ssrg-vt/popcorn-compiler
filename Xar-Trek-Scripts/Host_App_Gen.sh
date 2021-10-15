#!/bin/bash

if [[ $1 == "" ]]; then
  printf "\n**************************************************************\n"
  printf "Usage :\n"
  printf "Host_App_Gen.sh <config_file>\n\n"
  printf "Example:\n"
  printf "Host_App_Gen.sh config.cfg \n"
  printf "**************************************************************\n\n"
  exit
fi



# Get main file location
main_file_path=`grep MAIN_FILE_PATH $1| cut -f2 -d=| awk '{print $NF}'`
main_file_name=`grep MAIN_FILE_NAME $1| cut -f2 -d=| awk '{print $NF}'`
main_file="${main_file_path}${main_file_name}"
echo Main File = $main_file

# Get Candidate Function's name
sw_fc_name=`grep -o '^[^//]*'  $1| grep \(| cut -f1 -d\(| awk '{print $2}'`
printf "sw_fc_name = %s\n\n" $sw_fc_name

# make a back up of original file
cp $main_file $main_file.bak-`date +%Hh%Mm%Ss`

# Copy KNL_Function.h
cp "KNL_Function.h" $main_file_path/KNL_Function.h

# insert KNL HOST 0 Section before main
# get line number of main function
KNL_0=`grep -n -o '^[^//]*' $main_file|grep -E '\s*main\s*\(.+\)'| cut -f1 -d:`
printf "KNL 0 = %s\n" $KNL_0
sed -i "$KNL_0 i \
// KNL HOST 0 - BEGIN - Include APIs and Migration Flag\n\
#include \"KNL_Function.h\"\n\
#include \"KNL_Load_Exec.c\"\n\
extern int per_app_migration_flag;\n\
// KNL HOST 0 - END - Include APIs and Migration Flag\n\n\
" $main_file



# insert KNL HOST 1 Section after main
# get line number after first { after main function
# KNL_0=`grep -n -E '\s*main\s*\(.+\)' $main_file| cut -f1 -d:`
line_inc=`tail -n +$KNL_0 $main_file|grep -n -o '^[^//]*'|grep {|head -1| cut -f1 -d:`
# TODO - change back when not using timers on the first 3 lines
KNL_1=$((KNL_0+$line_inc+5))
printf "KNL 1 = %s\n" $KNL_1

sed -i "$KNL_1 i \
// KNL HOST 1 - BEGIN - Start Scheduler Client + INIT Kernel \n\
popcorn_client(0);\n\
while (per_app_migration_flag == -1);\n\
if (per_app_migration_flag == 2) KNL_HW_INIT (context, commands, program, fpga_kernel);\n\
// KNL HOST 1 - END - Start Scheduler Client + INIT Kernel + \n\
" $main_file


# Insert Candidate Function Migration
# insert KNL HOST 2 Section after main
# Find candidate function call - first time
line_inc=`tail -n +$KNL_0 $main_file | grep -n -o '^[^//]*'|  grep  -E "\s*$sw_fc_name\s*\(.+\)"|head -1|cut -f1 -d:`
# echo line inc = $line_inc
if [ $line_inc <> 0 ] ; then
  KNL_2=$((KNL_0+$line_inc-1))
else
  printf "Candidate Function Not Found\n"
  exit
fi
printf "KNL 2 (before function) = %s\n" $KNL_2

fc_call=`tail -n +$KNL_2 $main_file|grep -o '^[^//]*'|grep -E "\s*$sw_fc_name\s*\(.+\)"|head -1`
knl_fc2=`tail -n +$KNL_2 $main_file|grep -o '^[^//]*'|grep -E "\s*$sw_fc_name\s*\(.+\)"| head -1| cut -f2 -d\(`
knl_fc1="KNL_HW_$sw_fc_name (context, commands, program, fpga_kernel, "
# echo Candidate Function = $fc_call
# echo Hardware Kernel = $knl_fc1$knl_fc2

sed -i "$KNL_2 i \
// KNL HOST 2 - BEGIN - Popcorn Migration Options\n\
#ifdef PRINT_INFO\n\
printf(\"DBG---> Migration Flag: %d\", per_app_migration_flag);\n \
#endif\n\n\
if (per_app_migration_flag == 0) { \n\
" $main_file


KNL_2=$((KNL_2+9))
printf "KNL 2 (after function) = %s\n" $KNL_2

# $fc_call\n\


sed -i "$KNL_2 i \
} else if (per_app_migration_flag == 1) {\n\
migrate (1,0,0);\n\
$fc_call\n\
migrate (0,0,0);\n\
} else if (per_app_migration_flag == 2) {\n\
$knl_fc1$knl_fc2 \n\
}\n\
// KNL HOST 2 - END - Popcorn Migration Options\n\
" $main_file



# insert KNL HOST 3 Section before last } from main function
count=0
# Had to group while with the last commands in order not to loose the value of $last_line
tail -n +$KNL_0 $main_file| grep -n -o '^[^//]*'| grep -e{ -e}|
# tail -n +$KNL_0 $sw_fc_name.cpp|grep -n -E '^[^\/\/]*'|grep -e { -e}|

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

  if [ $count == 0 ]; then
    # echo Found $line $KNL_1
    str1=`echo $line|cut -f1 -d:`
    # echo str1 = $str1
    last_line=$(($KNL_1+$str1))
    break
  fi
  # echo " "

  # echo Last = $last_line
done

KNL_3=$((`head -n +$last_line $main_file|grep -n -o '^[^//]*'| grep -E '\s*return\s*.+\;'| cut -f1 -d:| tail -1` - 1))
echo KNL_3 = $KNL_3

# Insert Scheduler stop
sed -i "$KNL_3 i \
// KNL HOST 3 - BEGIN - Stop Scheduler Client\n\
popcorn_client(1);\n\
// KNL HOST 3 - END - Stop Scheduler Client\n\n\
" $main_file

echo)

# **********************************************************************#
#                                                                       #
#                                                                       #
# Generate KNL_Load_Exec                                                #
#                                                                       #
#                                                                       #
# **********************************************************************#


cp KNL_Load_Exec_Template.c KNL_Load_Exec.c

# Get platform and XCLBIN names
target_platform=`grep TARGET_PLAT $1 | cut -f2 -d=| awk '{print $1}'`
xclbin_file_name=`grep XCLBIN_NAME $1 | cut -f2 -d=| awk '{print $1}'`
echo Target Platform = $target_platform
echo XCLBIN File = $xclbin_file_name
sed -i "s/TARGET_PLATFORM/$target_platform/g" KNL_Load_Exec.c
sed -i "s/XCLBIN_FILE/$xclbin_file_name/g" KNL_Load_Exec.c


# Get Candidate Function's name (skip comments) and get first word before parentheses
sw_fc_name=`grep -o '^[^//]*'  $1| grep \(| cut -f1 -d\(| awk '{print $NF}'`
printf "sw_fc_name = %s\n\n" $sw_fc_name


# Get parameters quantity counting commas between parentheses in the function prototype
param_qty=`grep -o '^[^//]*'  $1| tr '\n' ' '| grep -o -P '(?<=\().*?(?=\))'| grep -o ,|wc -l`
param_qty=$((param_qty+1))
printf "param_qty = %s\n\n" $param_qty


# Get all parameters and set flag if const
for (( c=1; c<=$param_qty; c++ ))
do
  sw_param[$c]=`grep -o '^[^//]*'  $1| tr '\n' ' '| grep -o -P '(?<=\().*?(?=\))'|
  cut -f$c -d,`
  echo "${sw_param[$c]} a"
  if [[ ${sw_param[$c]} == *"const"* ]] ; then
    sw_param_const[$c]=1
  else
    sw_param_const[$c]=0
  fi
done



# For each parameter change data types to OpenCL data types
for (( c=1; c<=$param_qty; c++ ))
do
  sw_param[$c]=`grep -o '^[^//]*'  $1| tr '\n' ' '| grep -o -P '(?<=\().*?(?=\))'|
  # TODO complete options with possible C types
  sed -E 's/const\s*unsigned\s*long\s*long/cl_ulong /g'|
  sed -E 's/unsigned\s*long\s*long/cl_ulong /g'|

  sed -E 's/const\s*unsigned\s*char/cl_uchar /g'|
  sed -E 's/unsigned\s*char/cl_uchar /g'|

  sed -E 's/const\s*unsigned\s*int/cl_uint /g'|
  sed -E 's/unsigned\s*int/cl_uint /g'|

  sed -E 's/\bint/cl_int/g'|
  sed -E 's/\bchar/cl_char/g'|
  sed -E 's/\bdouble/cl_double/g'|

  cut -f$c -d,`
done

# Associative array to work as a matrix and store parameters' dimensions
declare -A sw_param_size_dimension

for (( c=1; c<=$param_qty; c++ ))
do
  sw_param_size_qt[$c]=`echo ${sw_param[$c]}| grep -o '\['|wc -l`
  sw_param_type[$c]=`echo ${sw_param[$c]}| awk '{print $1}'`

  # get the parameter name if scalar
  if [ ${sw_param_size_qt[$c]} == 0 ]; then
    sw_param_name[$c]=`echo ${sw_param[$c]}| awk '{print $2}'`
  else
    # get parameter name if array
    sw_param_name[$c]=`echo ${sw_param[$c]}| cut -f1 -d\[| awk '{print $2}'`
  fi

# # get all dimensions of each parameter
for (( j=1; j<=${sw_param_size_qt[$c]}; j++ ))
do
  sw_param_size_dimension[$c,$j]="("`echo "${sw_param[$c]}"| grep -o -P '(?<=\[).*?(?=\])'|head -$j| tail -1`")"
done


echo

echo


done




#
for (( c=1; c<=$param_qty; c++ ))
do
  echo sw_param [$c] = "${sw_param[$c]}"
  echo sw_param_type [$c] = "${sw_param_type[$c]}"
  echo sw_param_name [$c] = "${sw_param_name[$c]}"
  echo sw_param_size_qt [$c] = "${sw_param_size_qt[$c]}"
  echo sw_param_const[$c] = "${sw_param_const[$c]}"
  # echo sw_param_size [$c] = "${sw_param_size[$c]}"


  for (( i=1; i<=${sw_param_size_qt[$c]}; i++ ))
  do
    echo dimension $i = "${sw_param_size_dimension[$c,$i]}"
  done
  echo
done



# Find line number of KNL CALL 1 Section
KNL_1=`grep -n -E '\s*KNL CALL 1 - BEGIN Header\s*' KNL_Load_Exec.c| cut -f1 -d:`
KNL_1=$((KNL_1+1))
printf "KNL 1 = %s\n" $KNL_1
# Insert KNL CALL 1 Section
sed -i "$KNL_1 i \
int KNL_HW_$sw_fc_name (\
cl_context context,\
cl_command_queue commands,\
cl_program program,\
cl_kernel fpga_kernel,\
`grep -o '^[^//]*'  $1| tr '\n' ' '| grep -o -P '(?<=\().*?(?=\))'` ) \
" KNL_Load_Exec.c

# Find line number of KNL CALL 2 Section
KNL_2=`grep -n -E '\s*KNL CALL 2 - BEGIN Create Compute Kernel\s*' KNL_Load_Exec.c| cut -f1 -d:`
KNL_2=$((KNL_2+1))
printf "KNL 2 = %s\n" $KNL_2
# Insert KNL CALL 2 Section
sed -i "$KNL_2 i \
fpga_kernel = clCreateKernel(program, \"KNL_HW_$sw_fc_name\", &err);\n\
 if (!fpga_kernel || err != CL_SUCCESS) {\n\
     printf(\"Error: Failed to create compute kernel !"'\\'n"\");\n\
     printf(\"test failed"'\\'n"\");\n\
     return EXIT_FAILURE;\n\
 }\
 " KNL_Load_Exec.c


 # Find line number of KNL CALL 3 Section
 KNL_3=`grep -n -E '\s*KNL CALL 3 - BEGIN Create host data pointers and copy kernel data\s*' KNL_Load_Exec.c| cut -f1 -d:`
 printf "KNL 3 = %s\n" $KNL_3
 # Insert KNL CALL 3 Section
for (( c=1; c<=$param_qty; c++ ))
do

  case ${sw_param_size_qt[$c]} in
    0)
    KNL_3=$((KNL_3+1))
    sed -i "$KNL_3 i \
    ${sw_param_type[$c]}* KNL_Data_${sw_param_name[$c]};\n\
KNL_Data_${sw_param_name[$c]} = (${sw_param_type[$c]}*)aligned_alloc(MEM_ALIGNMENT, sizeof(${sw_param_type[$c]}*));\n\
*KNL_Data_${sw_param_name[$c]} = ${sw_param_name[$c]};\n\
  " KNL_Load_Exec.c
    KNL_3=$((KNL_3+1))
    ;;

    1)
    KNL_3=$((KNL_3+1))
    sed -i "$KNL_3 i \
    ${sw_param_type[$c]}* KNL_Data_${sw_param_name[$c]};\n\
KNL_Data_${sw_param_name[$c]} = (${sw_param_type[$c]}*)aligned_alloc(MEM_ALIGNMENT,${sw_param_size_dimension[$c,1]} * sizeof(${sw_param_type[$c]}*));\n\
	for( j = 0; j < ${sw_param_size_dimension[$c,1]};  j++)\n\
  {\n\
 		KNL_Data_${sw_param_name[$c]}[j] = ${sw_param_name[$c]}[j];\n\
  }\n\n\
    " KNL_Load_Exec.c
    KNL_3=$((KNL_3+6))
    ;;

    2)
    KNL_3=$((KNL_3+1))
    sed -i "$KNL_3 i \
    ${sw_param_type[$c]} (*KNL_Data_${sw_param_name[$c]}) [${sw_param_size_dimension[$c,2]}];\n\
KNL_Data_${sw_param_name[$c]} = (${sw_param_type[$c]}**)aligned_alloc(MEM_ALIGNMENT,${sw_param_size_dimension[$c,1]} * ${sw_param_size_dimension[$c,2]} * sizeof(${sw_param_type[$c]}*));\n\
for( i = 0; i < ${sw_param_size_dimension[$c,1]};  i++)\n\
	for( j = 0; j < ${sw_param_size_dimension[$c,2]};  j++)\n\
  {\n\
 		KNL_Data_${sw_param_name[$c]}[i][j] = ${sw_param_name[$c]}[i][j];\n\
  }\n\n\
    " KNL_Load_Exec.c
    KNL_3=$((KNL_3+7))
    ;;

    3)
    KNL_3=$((KNL_3+1))
    sed -i "$KNL_3 i \
    ${sw_param_type[$c]} (*KNL_Data_${sw_param_name[$c]}) [${sw_param_size_dimension[$c,2]}];\n\
KNL_Data_${sw_param_name[$c]} = (${sw_param_type[$c]}**)aligned_alloc(MEM_ALIGNMENT,${sw_param_size_dimension[$c,1]} * ${sw_param_size_dimension[$c,2]} * sizeof(${sw_param_type[$c]}*));\n\
for( i = 0; i < ${sw_param_size_dimension[$c,1]};  i++)\n\
  for( j = 0; j < ${sw_param_size_dimension[$c,2]};  j++)\n\
   for( k = 0; k < ${sw_param_size_dimension[$c,3]};  k++)\n\
  {\n\
 		KNL_Data_${sw_param_name[$c]}[i][j][k] = ${sw_param_name[$c]}[i][j][k];\n\
  }\n\n\
    " KNL_Load_Exec.c
    KNL_3=$((KNL_3+8))
    ;;



    *)
    printf "Size = none\n"
    ;;
  esac

done

# Find line number of KNL CALL 4 Section
KNL_4=`grep -n -E '\s*KNL CALL 4 - BEGIN Create host buffers\s*' KNL_Load_Exec.c| cut -f1 -d:`
printf "KNL 4 = %s\n" $KNL_4
# Insert KNL CALL 4 Section
for (( c=1; c<=$param_qty; c++ ))
do

 case ${sw_param_size_qt[$c]} in
   0)
   KNL_4=$((KNL_4+1))
   sed -i "$KNL_4 i \
   cl_mem KNL_Buff_${sw_param_name[$c]};\n\
KNL_Buff_${sw_param_name[$c]} = clCreateBuffer(context,  CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, sizeof(${sw_param_type[$c]}), KNL_Data_${sw_param_name[$c]}, &err);\n\
if (err != CL_SUCCESS) {\n\
printf(\"Error: can not create buffer (error=%d)"'\\'n"\", err);\n\
}\n\n\
    " KNL_Load_Exec.c
   KNL_4=$((KNL_4+5))
     ;;

   1)
   KNL_4=$((KNL_4+1))
   sed -i "$KNL_4 i \
cl_mem KNL_Buff_${sw_param_name[$c]};\n\
KNL_Buff_${sw_param_name[$c]} = clCreateBuffer(context,  CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, ${sw_param_size_dimension[$c,1]} * sizeof(${sw_param_type[$c]}), KNL_Data_${sw_param_name[$c]}, &err);\n\
if (err != CL_SUCCESS) {\n\
printf(\"Error: can not create buffer (error=%d)"'\\'n"\", err);\n\
}\n\n\
 " KNL_Load_Exec.c
KNL_4=$((KNL_4+5))
   ;;

   2)
   KNL_4=$((KNL_4+1))
   sed -i "$KNL_4 i \
   cl_mem KNL_Buff_${sw_param_name[$c]};\n\
KNL_Buff_${sw_param_name[$c]} = clCreateBuffer(context,  CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, ${sw_param_size_dimension[$c,1]} * ${sw_param_size_dimension[$c,2]} * sizeof(${sw_param_type[$c]}), KNL_Data_${sw_param_name[$c]}, &err);\n\
   if (err != CL_SUCCESS) {\n\
   printf(\"Error: can not create buffer (error=%d)"'\\'n"\", err);\n\
 }\n\n\
   " KNL_Load_Exec.c
   KNL_4=$((KNL_4+5))
   ;;

   3)
   KNL_4=$((KNL_4+1))
   sed -i "$KNL_4 i \
   cl_mem KNL_Buff_${sw_param_name[$c]};\n\
KNL_Buff_${sw_param_name[$c]} = clCreateBuffer(context,  CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, ${sw_param_size_dimension[$c,1]} * ${sw_param_size_dimension[$c,2]} * ${sw_param_size_dimension[$c,3]} * sizeof(${sw_param_type[$c]}), KNL_Data_${sw_param_name[$c]}, &err);\n\
   if (err != CL_SUCCESS) {\n\
   printf(\"Error: can not create buffer (error=%d)"'\\'n"\", err);\n\
 }\n\n\
   " KNL_Load_Exec.c
   KNL_4=$((KNL_4+5))
   ;;

   *)
   printf "Size = none\n"
   ;;
 esac

done



KNL_4=$((KNL_4+2))
sed -i "$KNL_4 i \
if (!(\
" KNL_Load_Exec.c

KNL_4=$((KNL_4+1))
for (( c=1; c<$param_qty; c++ ))
do
  sed -i "$KNL_4 i \
  KNL_Buff_${sw_param_name[$c]} && \
  " KNL_Load_Exec.c
done
KNL_4=$((KNL_4+$param_qty-1))

  sed -i "$KNL_4 i \
  KNL_Buff_${sw_param_name[$param_qty]} \
  " KNL_Load_Exec.c

  KNL_4=$((KNL_4+1))
  sed -i "$KNL_4 i )) {\
  " KNL_Load_Exec.c



KNL_4=$((KNL_4+1))
sed -i "$KNL_4 i \
   printf(\"Error: Failed to allocate device memory!"'\\'n"\");\n\
   printf(\"test failed"'\\'n"\");\n\
   return EXIT_FAILURE;\n\
}\
" KNL_Load_Exec.c
KNL_4=$((KNL_4+4))



KNL_4=$((KNL_4+1))
sed -i "$KNL_4 i \
#ifdef TIMERS_ALL \n\
    gettimeofday(&tv, NULL); \n\
    time_off = tv.tv_usec;\n\
    time_off += (tv.tv_sec*1.0e6);\n\
    printf(\"TIME--> Create Buffers in %12.8f ms"'\\'n"\", (time_off - time_on)/1.0e3);\n\
#endif\n\
 \
#ifdef TIMERS_ALL\n\
    gettimeofday(&tv, NULL);\n\
    time_on = tv.tv_usec;\n\
    time_on += (tv.tv_sec*1.0e6);\n\
#endif\n\
" KNL_Load_Exec.c
KNL_4=$((KNL_4+12))


KNL_4=$((KNL_4+1))
sed -i "$KNL_4 i \
cl_mem pt[$param_qty];\
" KNL_Load_Exec.c
KNL_4=$((KNL_4+1))



KNL_4=$((KNL_4+1))
for (( c=1; c<=$param_qty; c++ ))
do
  sed -i "$KNL_4 i \
  pt[$((c-1))] = KNL_Buff_${sw_param_name[$c]};\n\
err = clEnqueueMigrateMemObjects(commands,(cl_uint)1,&pt[$((c-1))], 0 ,0,NULL, NULL);\n\
  " KNL_Load_Exec.c
done
KNL_4=$((KNL_4+$param_qty))



# Find line number of KNL CALL 5 Section
KNL_5=`grep -n -E '\s*KNL CALL 5 - BEGIN Create kernel arguments\s*' KNL_Load_Exec.c| cut -f1 -d:`
printf "KNL 5 = %s\n" $KNL_5
# Insert KNL CALL 5 Section
KNL_5=$((KNL_5+1))
sed -i "$KNL_5 i \
err = 0;\n\
" KNL_Load_Exec.c
KNL_5=$((KNL_5+1))

for (( c=1; c<=$param_qty; c++ ))
do
  sed -i "$KNL_5 i \
  err |= clSetKernelArg(fpga_kernel, $((c-1)), sizeof(cl_mem), &KNL_Buff_${sw_param_name[$c]});\
  " KNL_Load_Exec.c
done
KNL_5=$((KNL_5+$param_qty))

KNL_5=$((KNL_5+1))
sed -i "$KNL_5 i \
if (err != CL_SUCCESS) {\n\
    printf(\"Error: Failed to set fpga_kernel arguments! %d"'\\'n"\", err);\n\
    printf(\"test failed"'\\'n"\");}\n\
" KNL_Load_Exec.c
KNL_5=$((KNL_5+3))




# Find line number of KNL CALL 6 Section
KNL_6=`grep -n -E '\s*KNL CALL 6 - BEGIN copy Global Memory to Host\s*' KNL_Load_Exec.c| cut -f1 -d:`
printf "KNL 6 = %s\n" $KNL_6
# Insert KNL CALL 6 Section
KNL_6=$((KNL_6+1))

for (( c=1; c<=$param_qty; c++ ))
do
  sed -i "$KNL_6 i \
  err |= clEnqueueMigrateMemObjects(commands,(${sw_param_type[$c]})1,&pt[$((c-1))], CL_MIGRATE_MEM_OBJECT_HOST,0,NULL, NULL);\
  " KNL_Load_Exec.c
done
KNL_6=$((KNL_6+$param_qty))

KNL_6=$((KNL_6+1))
sed -i "$KNL_6 i \
if (err != CL_SUCCESS) {\n\
    printf(\"Error: Failed to write to source array: %d"'\\'n"\", err);\n\
    printf(\"test failed"'\\'n"\");}\n\
" KNL_Load_Exec.c
KNL_6=$((KNL_6+3))


# Find line number of KNL CALL 7 Section
KNL_7=`grep -n -E '\s*KNL CALL 7 - BEGIN Copy host data to function output\s*' KNL_Load_Exec.c| cut -f1 -d:`
printf "KNL 7 = %s\n" $KNL_7
# Insert KNL CALL 7 Section
for (( c=1; c<=$param_qty; c++ ))
do

 case ${sw_param_size_qt[$c]} in
   0)
   if [[ ${sw_param_const[$c]} == 0 ]]; then
     KNL_7=$((KNL_7+1))
    echo NO CONST
   sed -i "$KNL_7 i \
${sw_param_name[$c]} = *KNL_Data_${sw_param_name[$c]};\n\
 " KNL_Load_Exec.c
   KNL_7=$((KNL_7+1))
   fi
   ;;

   1)
   if [[ ${sw_param_const[$c]} == 0 ]]; then
     KNL_7=$((KNL_7+1))
    echo NO CONST
   sed -i "$KNL_7 i \
 for( j = 0; j < ${sw_param_size_dimension[$c,1]};  j++)\n\
 {\n\
${sw_param_name[$c]}[j] =    KNL_Data_${sw_param_name[$c]}[j];\n\
 }\n\n\
   " KNL_Load_Exec.c
   KNL_7=$((KNL_7+4))
   fi
   ;;

   2)
   if [[ ${sw_param_const[$c]} == 0 ]]; then
     KNL_7=$((KNL_7+1))
    echo NO CONST
   sed -i "$KNL_7 i \
for( i = 0; i < ${sw_param_size_dimension[$c,1]};  i++)\n\
 for( j = 0; j < ${sw_param_size_dimension[$c,2]};  j++)\n\
 {\n\
${sw_param_name[$c]}[i][j] =    KNL_Data_${sw_param_name[$c]}[i][j];\n\
 }\n\n\
   " KNL_Load_Exec.c
   KNL_7=$((KNL_7+5))
   fi
   ;;


   3)
   if [[ ${sw_param_const[$c]} == 0 ]]; then
     KNL_7=$((KNL_7+1))
    echo NO CONST
   sed -i "$KNL_7 i \
for( i = 0; i < ${sw_param_size_dimension[$c,1]};  i++)\n\
  for( j = 0; j < ${sw_param_size_dimension[$c,2]};  j++)\n\
   for( k = 0; k < ${sw_param_size_dimension[$c,3]};  k++)\n\
 {\n\
${sw_param_name[$c]}[i][j][k] =    KNL_Data_${sw_param_name[$c]}[i][j][k];\n\
 }\n\n\
   " KNL_Load_Exec.c
   KNL_7=$((KNL_7+6))
   fi
   ;;

   *)
   printf "Size = none\n"
   ;;
 esac

done

# Find line number of KNL CALL 8 Section
KNL_8=`grep -n -E '\s*KNL CALL 8 - BEGIN Free Data/Buff\s*' KNL_Load_Exec.c| cut -f1 -d:`
printf "KNL 8 = %s\n" $KNL_8
# Insert KNL CALL 8 Section
for (( c=1; c<=$param_qty; c++ ))
do
  KNL_8=$((KNL_8+1))
  sed -i "$KNL_8 i \
  free(KNL_Data_${sw_param_name[$c]});\n\
  clReleaseMemObject(KNL_Buff_${sw_param_name[$c]});\n\
  " KNL_Load_Exec.c
  KNL_8=$((KNL_8+2))


done









# take out * when paramenter was declared as a pointer
sed -i 's/_\*/_/g' KNL_Load_Exec.c
# sed -i 's/const/  /g' KNL_Load_Exec.c


mv KNL_Load_Exec.c $main_file_path/KNL_Load_Exec.c
