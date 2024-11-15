#!/bin/bash -

CPPCHECK=tools/codecheck/cppcheck-2.0/cppcheck-v2.0
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:./tools/codecheck/cppcheck-2.0/"
check_path="."
if [ "$1" ] ;then
	check_path=$1
fi

OUT_DIR=out/check_static_v2_0/$check_path
mkdir -p "${OUT_DIR}"

CHECK_LOG="${OUT_DIR}/static.log"
CHECK_LOG_SORT="${OUT_DIR}/static_sort.log"
CHECK_ERR="${OUT_DIR}/static-err.log"
CHECK_WARN="${OUT_DIR}/static-warn.log"
rm -f ${CHECK_LOG} ${CHECK_ERR} ${CHECK_WARN}

echo check: "$check_path"
echo out log: "$OUT_DIR"
src=$check_path

${CPPCHECK} -j32 --enable=warning --force -x c++ \
	--suppress=nullPointerRedundantCheck \
	--suppress=shiftTooManyBitsSigned \
	--suppress=preprocessorErrorDirective \
	--suppress=syntaxError \
	--suppress=invalidPrintfArgType_sint \
	--suppress=invalidScanfArgType_int \
	--template '{file},{line},{severity},{id},{message}' \
	${src} --output-file=${CHECK_LOG}

# avoid that there is no check log
touch ${CHECK_LOG} ${CHECK_ERR} ${CHECK_WARN}
rm -f ${CHECK_LOG_SORT}
sort ${CHECK_LOG} > ${CHECK_LOG_SORT}
# parse result
awk -F, '$3~/warning/{print}' ${CHECK_LOG_SORT} > ${CHECK_WARN} 2>/dev/null
awk -F, '$3~/error/{print}' ${CHECK_LOG_SORT} > ${CHECK_ERR} 2>/dev/null
WARN_CNT=`wc -l < ${CHECK_WARN}`
ERR_CNT=`wc -l < ${CHECK_ERR}`
echo
echo ERROR Count : ${ERR_CNT}
echo WARNING Count : ${WARN_CNT}

tree ${OUT_DIR}
