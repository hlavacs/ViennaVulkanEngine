# Absolute path to this script. /home/user/bin/foo.sh
ROOT_SCRIPT=$(realpath $0)
# Absolute path this script is in. /home/user/bin
ROOT_SCRIPT_PATH=`dirname $ROOT_SCRIPT`

find $ROOT_SCRIPT_PATH -maxdepth 3 -type f -name 'compile_shaders.sh' -exec {} \;