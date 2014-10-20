#!/bin/bash
LNX=/home/smart/Projects/LinuxKernel.org/linux
cd /	
#find  $LNX                                                                \
#	-path "$LNX/arch/*" ! -path "$LNX/arch/i386*" -prune -o               \
#	-path "$LNX/include/asm-*" ! -path "$LNX/include/asm-i386*" -prune -o \
#	-path "$LNX/tmp*" -prune -o                                           \
#	-path "$LNX/Documentation*" -prune -o                                 \
#	-path "$LNX/scripts*" -prune -o                                       \
#	-path "$LNX/drivers*" -prune -o                                       \
#	-name "*.[chxsS]" -print >$LNX/cscope.files
find  $LNX                                                                \
	-path "$LNX/arch/arm/mach-*" ! -path "$LNX/arch/arm/mach-s5pv210*" -prune -o               \
	-path "$LNX/arch/arm/plat-*" ! -path "$LNX/arch/arm/plat-samsung*" ! -path "$LNX/arch/arm/plat-s5p*" -prune -o \
	-path "$LNX/arch/*" ! -path "$LNX/arch/arm*" -prune -o \
	-path "$LNX/arch/arm64*" -prune -o \
	-name "*.[chxsS]" -print >$LNX/cscope.files

cd $LNX     # the directory with 'cscope.files'
cscope -b -q -k
