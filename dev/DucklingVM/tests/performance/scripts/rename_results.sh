#!/bin/bash

# Example usage:
# 1. Copy the script to /results folder
# 2. To change the name of all results from profile_xyz to new_name invoke
#
#     ./rename_results.sh profile_xyz-ps new_name
#
# You can tab-complete the old name, the shortcode in the old name will be cut
# out.
# 3. Now all the results have names new_name-ps, new_name-hs, new_name-ua etc.
# and such names will be visible in the vtune-gui (which is handy during
# report comparisons).

current="${1%-*}"
desired="$2"

for result in ${current}*;
do
  shortcode=${result##*-}
  echo "Renaming ${result} to "$desired-$shortcode""
  mv "$result/$result.vtune" "$result/$desired-$shortcode.vtune"
  mv "$result" "$desired-$shortcode"
done

