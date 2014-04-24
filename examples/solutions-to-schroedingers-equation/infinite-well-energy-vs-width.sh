#!/bin/sh
set -e

# Calculates the first 3 energy level in an infinite GaAs quantum well
# with a range of well widths. The energies are output to the file
# "infinite-well-energy-vs-width.r" (assuming parabolic dispersion)
# The file is laid out as follows:
#
#   Column 1: Layer width [angstrom]
#   Column 2: Energy of 1st state [meV]
#   Column 3: Energy of 2nd state [meV]
#   Column 4: Energy of 3rd state [meV]
#
# This script is part of the QWWAD software suite. Any use of this code
# or its derivatives in published work must be accompanied by a citation
# of:
#   P. Harrison and A. Valavanis, Quantum Wells, Wires and Dots, 4th ed.
#    Chichester, U.K.: J. Wiley, 2015, ch.2
#
# (c) Copyright 1996-2014
#     Paul Harrison <p.harrison@shu.ac.uk>
#     Alex Valavanis <a.valavanis@leeds.ac.uk>
#
# QWWAD is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# QWWAD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with QWWAD.  If not, see <http://www.gnu.org/licenses/>.

# Initialise files
outfile=infinite-well-energy-vs-width.r
rm -f $outfile

# Set fixed parameters
mass=0.067 # Effective mass relative to a free electron
nst=3      # Number of states

# Loop for different well widths
for i in `seq 1 0.1 2.3`
do
{
    # Generate well-widths exponentially so we get a smooth curve at small
    # widths
    LW=`echo $i | awk '{print 10^$1}'`

    # Calculate first 3 energy levels as a function of well width for GaAs
    efiw -L $LW -m $mass -s $nst

    printf "%f\t" "$LW" >> $outfile	# write well width to file

    energies=`awk '{print $2}' < Ee.r`
    echo $energies >> $outfile
}
done

# Clean up the workspace
rm wf_e?.r Ee.r
