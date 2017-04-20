#!/bin/bash

if [ -z "$1" ] ; then
  echo "Usage: $0 RELEASE_DATE_FILE"
  exit 1
fi

if [ -f "$1" ] ; then
  cat "$1"
else
  echo "not released yet"
fi
