#!/bin/sh

#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

set -e

# Update PATH to get access to ldconfig(8)
PATH="$PATH:/usr/sbin"

# ldconfig --print-cache will return something like
# libpcsclite.so.1 (libc6,x86-64) => /usr/lib/x86_64-linux-gnu/libpcsclite.so.1
DIR=$(ldconfig --print-cache | grep libpcsclite.so.1)

# get the right part only: /usr/lib/x86_64-linux-gnu/libpcsclite.so.1
DIR=$(echo "$DIR" | cut -d'=' -f2 | cut -d' ' -f2)

# get the directory part only: /usr/lib/x86_64-linux-gnu
DIR=$(dirname "$DIR")

cmd="export LIBPCSCLITE_DELEGATE=$DIR/libpcscspy.so.0"
echo "$cmd"
$cmd
