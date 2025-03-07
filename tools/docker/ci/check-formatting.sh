#!/bin/bash

set -e

SRC="${1}"
EXCLUDE_FILE="${SRC}/tools/format/exclude"

if [[ -f "${EXCLUDE_FILE}" ]]; then
    echo "Excluding list of files at ${EXCLUDE_FILE}"
else
    echo "${EXCLUDE_FILE} does not exist"
fi

if [ ! -d "${SRC}" ]; then
    echo "Source tree missing. Mount source directory at ${SRC}"
    exit 1
fi

# set up configuration files

if [ -e "${SRC}/.clang-format" ]; then
    mv "${SRC}/.clang-format" "${SRC}/.clang-format.backup"
fi

if [ -e "${SRC}/.cmake-format.py" ]; then
    mv "${SRC}/.cmake-format.py" "${SRC}/.cmake-format.py.backup"
fi

cp /opt/otcommon/share/otcommon/format/clang-format "${SRC}/.clang-format"
cp /opt/otcommon/share/otcommon/format/cmake-format.py "${SRC}/.cmake-format.py"

# clang-format

CLANG_FORMAT_COMMAND="find ${SRC} \( -name '*.*pp' -o -name '*.json' \)"

if [[ -f "${EXCLUDE_FILE}" ]]; then
    while read p; do
        CLANG_FORMAT_COMMAND="${CLANG_FORMAT_COMMAND} -not -path '${SRC}/${p}/*'"
    done < "${EXCLUDE_FILE}"
fi

CLANG_FORMAT_COMMAND="${CLANG_FORMAT_COMMAND} -exec clang-format -i {} +"

# qmlformat

QML_FORMAT_COMMAND="find ${SRC} -name '*.qml'"

if [[ -f "${EXCLUDE_FILE}" ]]; then
    while read p; do
        QML_FORMAT_COMMAND="${QML_FORMAT_COMMAND} -not -path '${SRC}/${p}/*'"
    done < "${EXCLUDE_FILE}"
fi

QML_FORMAT_COMMAND="${QML_FORMAT_COMMAND} -exec /usr/lib64/qt6/bin/qmlformat -i -w 4 -n -l unix --objects-spacing --functions-spacing {} +"

# cmake-format

CMAKE_FORMAT_COMMAND="find ${SRC} \( -name 'CMakeLists.txt' -o -name '*.cmake' \)"

if [[ -f "${EXCLUDE_FILE}" ]]; then
    while read p; do
        CMAKE_FORMAT_COMMAND="${CMAKE_FORMAT_COMMAND} -not -path '${SRC}/${p}/*'"
    done < "${EXCLUDE_FILE}"
fi

CMAKE_FORMAT_COMMAND="${CMAKE_FORMAT_COMMAND} -exec cmake-format -i {} +"

# sort includes

FIX_INCLUDES_COMMAND="find ${SRC} -name '*.*pp'"

if [[ -f "${EXCLUDE_FILE}" ]]; then
    while read p; do
        FIX_INCLUDES_COMMAND="${FIX_INCLUDES_COMMAND} -not -path '${SRC}/${p}/*'"
    done < "${EXCLUDE_FILE}"
fi

FIX_INCLUDES_COMMAND="${FIX_INCLUDES_COMMAND} -exec /usr/bin/fix_includes.py --blank_lines --nocomments --safe_headers --reorder --sort_only {} +"

eval "${CLANG_FORMAT_COMMAND}"
eval "${QML_FORMAT_COMMAND}"
eval "${CMAKE_FORMAT_COMMAND}"
eval "${FIX_INCLUDES_COMMAND}"

# restore previous configuration files

rm "${SRC}/.clang-format"
rm "${SRC}/.cmake-format.py"

if [ -e "${SRC}/.clang-format.backup" ]; then
    mv "${SRC}/.clang-format.backup" "${SRC}/.clang-format"
fi

if [ -e "${SRC}/.cmake-format.py.backup" ]; then
    mv "${SRC}/.cmake-format.py.backup" "${SRC}/.cmake-format.py"
fi

# check for changed files

cd "${SRC}"
output=$(/usr/bin/git --no-pager diff)

if [ -z "$output" ]; then
  echo "Formatting check successful"

  exit 0
else
  echo "Formatting error(s) found:"
  echo "${output}"

  exit 1
fi
