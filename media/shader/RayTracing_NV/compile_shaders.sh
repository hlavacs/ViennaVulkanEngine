# Absolute path to this script. /home/user/bin/foo.sh
SCRIPT=$(realpath $0)
# Absolute path this script is in. /home/user/bin
SCRIPTPATH=`dirname $SCRIPT`
glslangValidator -V $SCRIPTPATH/raygen.rgen -o $SCRIPTPATH/rgen.spv
glslangValidator -V $SCRIPTPATH/closesthit.rchit -o $SCRIPTPATH/rchit.spv
glslangValidator -V $SCRIPTPATH/miss.rmiss -o $SCRIPTPATH/rmiss.spv
glslangValidator -V $SCRIPTPATH/shadow_miss.rmiss -o $SCRIPTPATH/shadow_rmiss.spv
