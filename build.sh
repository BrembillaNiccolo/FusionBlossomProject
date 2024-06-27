if [ "$#" != 2 ]
then
  echo "Usage: ./build.sh <platform> <freq>"
  exit 1
fi

platform=$1
default_freq=$2
source /opt/xilinx/xrt/setup.sh
source /home/xilinx/software/Vitis_HLS/2023.1/settings64.sh


make all TARGET=hw PLATFORM=${platform} FREQ_MHZ=${default_freq}

mkdir custom_dir
mv build_dir.hw.${platform}/QECBLOSSOM.xclbin custom_dir/
mv QECBLOSSOM custom_dir/


echo "Build for code length ${code_length} finished!"
