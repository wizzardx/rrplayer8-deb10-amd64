#!/bin/bash
set -e # Terminate if there are any errors

# Run this script to update "cpp_common" in the repository

echo " - Creating temp cpp_common sub-directory..."
mkdir -p /tmp/cpp_common/
echo " - Checking cpp_common out of the repository..."
svn checkout svn://172.30.166.99/rr/cpp_common/trunk/ /tmp/cpp_common/

echo " - Preparing to send updated cpp_common files to the repository..."
pushd ../../ > /dev/null
(cp * /tmp/cpp_common/ &> /dev/null)
(cp ./scripts/* /tmp/cpp_common/scripts/)
(cp ./scripts/svn/* /tmp/cpp_common/scripts/svn/)
(cp ./docs/* /tmp/cpp_common/docs/)
(rm -f /tmp/cpp_common/Makefile.in)
(rm -f /tmp/cpp_common/Makefile.am)

echo " - Listing changes."
svn status /tmp/cpp_common
echo -n " - Press ENTER to continue, ctrl+C to stop (go to /tmp/cpp_common and run any required svn commands...)"
read ENTER
svn commit /tmp/cpp_common

popd > /dev/null

echo " - Success."
