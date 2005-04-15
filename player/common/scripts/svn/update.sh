#!/bin/bash
set -e # Terminate if there are any errors

# Run this script to fetch any updates to "cpp_common" from the repository.

echo " - Creating temp cpp_common sub-directory..."
mkdir -p /tmp/cpp_common/
echo " - Checking cpp_common out of the repository..."
svn checkout svn://172.30.166.99/rr/cpp_common/trunk/ /tmp/cpp_common/
echo " - Updating this project's cpp_common..."
pushd ../../ > /dev/null

(cp /tmp/cpp_common/* .)
(cp /tmp/cpp_common/scripts/* ./scripts/)
(cp /tmp/cpp_common/scripts/svn/* ./scripts/svn)
(cp /tmp/cpp_common/docs/* ./docs/)

echo " - Listing updates"
svn status

popd > /dev/null

echo " - Success."
