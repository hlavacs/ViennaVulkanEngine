# Absolute path to this script. /home/user/bin/foo.sh
SCRIPT=$(realpath $0)
# Absolute path this script is in. /home/user/bin
SCRIPTPATH=`dirname $SCRIPT`
glslangValidator --target-env vulkan1.2 -V $SCRIPTPATH/raygen.rgen -o $SCRIPTPATH/rgen.spv
glslangValidator --target-env vulkan1.2 -V $SCRIPTPATH/closesthit.rchit -o $SCRIPTPATH/rchit.spv
glslangValidator --target-env vulkan1.2 -V $SCRIPTPATH/miss.rmiss -o $SCRIPTPATH/rmiss.spv
glslangValidator --target-env vulkan1.2 -V $SCRIPTPATH/shadow_miss.rmiss -o $SCRIPTPATH/shadow_rmiss.spv
