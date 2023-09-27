# Absolute path to this script. /home/user/bin/foo.sh
SCRIPT=$(realpath $0)
# Absolute path this script is in. /home/user/bin
SCRIPTPATH=`dirname $SCRIPT`
glslangValidator -V $SCRIPTPATH/shader.vert -o $SCRIPTPATH/vert.spv
glslangValidator -V $SCRIPTPATH/shader.frag -o $SCRIPTPATH/frag.spv
