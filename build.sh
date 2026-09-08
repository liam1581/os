#!/usr/bin/env bash
set -e

ACTION="$1"
SUB_ACTION="$2"

case "$ACTION" in
  clean)
    ./buildScripts/clean.sh "$SUB_ACTION"
    ;;
  run)
    ./buildScripts/run.sh
    ;;
  build)
    ./buildScripts/build.sh "$SUB_ACTION"
    ;;
  build-testing)
    ./buildScripts/build.sh testing
    ;;
  build-production)
    ./buildScripts/build.sh production
    ;;
  build-kernelpanic)
    ./buildScripts/build.sh kernelpanic
    ;;
  build_clean)
    ./buildScripts/clean.sh
    ./buildScripts/build.sh
    ;;
  stepinit)
    ./buildScripts/build.sh stepinit
    ;;
  *)
    if [ -z "$ACTION" ]; then
      ./buildScripts/build.sh
    else
      echo "Unknown argument: $ACTION $SUB_ACTION"
      echo "Usage:"
      echo "  ./build.sh clean"
      echo "  ./build.sh clean all"
      echo "  ./build.sh build"
      echo "  ./build.sh build-testing | build-production | build-kernelpanic"
      echo "  ./build.sh build_clean"
      echo "  ./build.sh run"
      exit 1
    fi
    ;;
esac