#!/bin/bash

if test -z "$2" ; then
  echo "Usage: $0 REPO_VERSION_FILE SRC_DIR"
  exit 1
fi

cd "$2" || exit 1

mod=
ret="$(git log --oneline | wc -l)"
if git status -s | egrep -q '*M ' ; then
    mod=M # the source has modifications
fi

if [ "$ret" = "0" ] ; then
  if [ -d "$2/.git" ] ; then
    echo "unknown"
  else
    cat "$1"
  fi
elif [ "$ret" = "Unversioned directory" ] ; then
  if [ -f "$1" ] ; then
    cat "$1"
  else
    echo "Unknown"
  fi
else
  echo "${ret}${mod}"
fi
