#!/bin/bash
LNX=/home/smart/Projects/LinuxKernel.org/linux
#CSCOPE_DB_DIR=/home/smart/Projects/LinuxKernel.org/cscope-database
CSCOPE_DB_DIR=$LNX
cd / 	
find $LNX   \
	-path "$LNX/arch/arm/mach-*" ! -path "$LNX/arch/arm/mach-s5pv210*" -prune -o          	\
	-path "$LNX/arch/arm/plat-*" ! -path "$LNX/arch/arm/plat-samsung*" ! -path "$LNX/arch/arm/plat-s5p*" -prune -o	\
	-path "$LNX/arch/*" ! -path "$LNX/arch/arm*" -prune -o          	\
	-path "$LNX/arch/arm64*" -prune -o \
	-name "*.[chxsS]" -print > $CSCOPE_DB_DIR/cscope.files

# Generate the database
cd $CSCOPE_DB_DIR
cscope -b -q -k

# Now you can use the standalone cscope browser: cscope -d
# And exit using: CTRL+D
